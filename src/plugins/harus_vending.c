/**
 * Harus Extended Vending Plugin — Multi-currency vending system
 */
#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/msgtable.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"
#include "common/utils.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/chrif.h"
#include "map/clif.h"
#include "map/itemdb.h"
#include "map/log.h"
#include "map/map.h"
#include "map/npc.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/searchstore.h"
#include "map/vending.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HPExport struct hplugin_info pinfo = {
	"Harus_Vending", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

/* ---- Data ---- */
#define VEND_DATA_ID 0

struct vend_pdata {
	int vend_coin;
};

/* ---- Configs ---- */
static int cfg_extended_vending   = 1;
static int cfg_skill_zeny2item    = 0;
static int cfg_vending_cash_id    = 0;
static int cfg_vending_zeny_id    = 0;
static int cfg_show_broadcast_info= 0;
static int cfg_ex_vending_info    = 0;
static int cfg_ex_vending_report  = 0;

static struct { const char *name; int *val; } hcfg[] = {
	{ "extended_vending",   &cfg_extended_vending },
	{ "skill_zeny2item",    &cfg_skill_zeny2item },
	{ "vending_cash_id",    &cfg_vending_cash_id },
	{ "vending_zeny_id",    &cfg_vending_zeny_id },
	{ "show_broadcast_info",&cfg_show_broadcast_info },
	{ "ex_vending_info",    &cfg_ex_vending_info },
	{ "ex_vending_report",  &cfg_ex_vending_report },
};
#define HCFG_COUNT (sizeof(hcfg)/sizeof(hcfg[0]))

static void hcfg_parse(const char *k, const char *v) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0){*hcfg[i].val=atoi(v);return;}
}
static int hcfg_return(const char *k) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0) return *hcfg[i].val; return 0;
}

/* ---- Helpers ---- */
static struct vend_pdata *vpd(struct map_session_data *sd) {
	struct vend_pdata *d;
	nullpo_retr(NULL, sd);
	d = getFromMSD(sd, VEND_DATA_ID);
	if (!d) { CREATE(d, struct vend_pdata, 1); memset(d,0,sizeof(*d)); addToMSD(sd,d,VEND_DATA_ID,true); }
	return d;
}

static const char *coin_name(int vc) {
	if (vc==0||(cfg_vending_zeny_id&&vc==cfg_vending_zeny_id)) return "Zeny";
	if (cfg_vending_cash_id&&vc==cfg_vending_cash_id) return "Cash";
	return itemdb_name(vc);
}

/* ---- Atcommand ---- */
ACMD(vendcoin) {
	struct vend_pdata *d = vpd(sd);
	char buf[256]; int coin_id;
	if (!cfg_extended_vending) { clif->message(fd,"Extended Vending desativado."); return false; }
	if (!*message) {
		if (d->vend_coin==0||(cfg_vending_zeny_id&&d->vend_coin==cfg_vending_zeny_id))
			clif->message(fd,"Moeda atual: Zeny");
		else if (cfg_vending_cash_id&&d->vend_coin==cfg_vending_cash_id)
			clif->message(fd,"Moeda atual: Cash Points");
		else { snprintf(buf,sizeof(buf),"Moeda atual: %s (ID: %d)",itemdb_name(d->vend_coin),d->vend_coin); clif->message(fd,buf); }
		return true;
	}
	coin_id = atoi(message);
	if (coin_id==0) { d->vend_coin=0; clif->message(fd,"Moeda resetada para Zeny."); return true; }
	if (!itemdb->exists(coin_id)) { clif->message(fd,"Item nao encontrado."); return false; }
	d->vend_coin = coin_id;
	snprintf(buf,sizeof(buf),"Moeda definida: %s (ID: %d)",itemdb_name(coin_id),coin_id);
	clif->message(fd,buf); return true;
}

/* ---- Script commands ---- */
BUILDIN(setvendcoin) { struct map_session_data *sd=script->rid2sd(st); if(sd) vpd(sd)->vend_coin=script_getnum(st,2); return true; }
BUILDIN(getvendcoin) { struct map_session_data *sd=script->rid2sd(st); script_pushint(st,sd?vpd(sd)->vend_coin:0); return true; }

/* ---- Hooks ---- */
static void hook_vending_open_pre(struct map_session_data **sd_ptr, const char **message,
	const uint8 **data, int *count)
{
	struct map_session_data *sd;
	struct vend_pdata *d;
	static char new_msg[MESSAGE_SIZE];
	if (!cfg_extended_vending||!cfg_show_broadcast_info) return;
	sd = *sd_ptr; if (!sd) return;
	d = getFromMSD(sd, VEND_DATA_ID);
	if (!d||d->vend_coin==0) return;
	snprintf(new_msg,sizeof(new_msg),"[%s] %s",coin_name(d->vend_coin),*message);
	*message = new_msg;
}

static void hook_vending_purchase_pre(struct map_session_data **sd_ptr, int *aid,
	unsigned int *uid, const struct CZ_PURCHASE_ITEM_FROMMC **data_ptr, int *count_ptr)
{
	struct map_session_data *sd, *vsd;
	struct vend_pdata *vd;
	const struct CZ_PURCHASE_ITEM_FROMMC *data;
	int count, vc;
	int i, j, cursor, w, new_=0, blank, vend_list[MAX_VENDING];
	int64 z;
	struct s_vending vend[MAX_VENDING];

	if (!cfg_extended_vending) return;
	sd = *sd_ptr; vsd = map->id2sd(*aid);
	if (!sd||!vsd||!vsd->state.vending||vsd->bl.id==sd->bl.id) return;
	vd = getFromMSD(vsd, VEND_DATA_ID);
	vc = vd ? vd->vend_coin : 0;
	if (vc==0||(cfg_vending_zeny_id&&vc==cfg_vending_zeny_id)) return;

	/* Take over */
	hookStop(); data=*data_ptr; count=*count_ptr;

	if (vsd->vender_id!=*uid) { clif->buyvending(sd,0,0,6); return; }
	if (!searchstore->queryremote(sd,*aid)&&(sd->bl.m!=vsd->bl.m||!check_distance_bl(&sd->bl,&vsd->bl,AREA_SIZE))) return;
	searchstore->clearremote(sd);
	if (count<1||count>MAX_VENDING||count>vsd->vend_num) return;
	blank = pc->inventoryblank(sd);
	memcpy(&vend,&vsd->vending,sizeof(vsd->vending));
	z=0; w=0;

	for (i=0;i<count;i++) {
		short amount=data[i].count, idx=data[i].index-2;
		if (amount<=0) return;
		if (idx<0||idx>=MAX_CART) return;
		ARR_FIND(0,vsd->vend_num,j,vsd->vending[j].index==idx);
		if (j==vsd->vend_num) return;
		vend_list[i] = j;
		z += (int64)vsd->vending[j].value * amount;

		if (cfg_vending_cash_id&&vc==cfg_vending_cash_id) {
			if (z>sd->cashPoints||z<0) { clif->buyvending(sd,idx,amount,1); return; }
		} else {
			int k, lc=0;
			for (k=0;k<sd->status.inventorySize;k++)
				if (sd->status.inventory[k].nameid==vc&&sd->status.inventory[k].amount>0&&!sd->status.inventory[k].bound)
					lc+=sd->status.inventory[k].amount;
			if (z>lc||z<0) { clif->buyvending(sd,idx,amount,1); return; }
			if (pc->inventoryblank(vsd)<=0) { clif->buyvending(sd,idx,amount,4); return; }
			{ int vw=itemdb_weight(vc)*(int)z; if(vw+vsd->weight>vsd->max_weight){clif->buyvending(sd,idx,amount,4);return;} }
		}
		w += itemdb_weight(vsd->status.cart[idx].nameid)*amount;
		if (w+sd->weight>sd->max_weight) { clif->buyvending(sd,idx,amount,2); return; }
		if (vend[j].amount>vsd->status.cart[idx].amount) vend[j].amount=vsd->status.cart[idx].amount;
		if (vend[j].amount<amount) { clif->buyvending(sd,idx,vsd->vending[j].amount,4); return; }
		vend[j].amount -= amount;
		switch (pc->checkadditem(sd,vsd->status.cart[idx].nameid,amount)) {
			case ADDITEM_EXIST: break;
			case ADDITEM_NEW: new_++; if(new_>blank) return; break;
			case ADDITEM_OVERAMOUNT: return;
		}
	}

	/* Payment */
	if (cfg_vending_cash_id&&vc==cfg_vending_cash_id) {
		pc->paycash(sd,(int)z,0); pc->getcash(vsd,(int)z,0);
	} else {
		int remaining=(int)z, k;
		struct item ci;
		for (k=0;k<sd->status.inventorySize&&remaining>0;k++) {
			if (sd->status.inventory[k].nameid==vc&&sd->status.inventory[k].amount>0&&!sd->status.inventory[k].bound) {
				int del=(sd->status.inventory[k].amount>remaining)?remaining:sd->status.inventory[k].amount;
				pc->delitem(sd,k,del,0,DELITEM_SOLD,LOG_TYPE_VENDING); remaining-=del;
			}
		}
		memset(&ci,0,sizeof(ci)); ci.nameid=vc; ci.identify=1; ci.amount=(int)z;
		pc->additem(vsd,&ci,(int)z,LOG_TYPE_VENDING);
	}

	/* Transfer items */
	for (i=0;i<count;i++) {
		short amount=data[i].count, idx=data[i].index-2;
		pc->additem(sd,&vsd->status.cart[idx],amount,LOG_TYPE_VENDING);
		vsd->vending[vend_list[i]].amount -= amount;
		clif->vendingreport(vsd,idx,amount,sd->status.char_id,(int)z);
		pc->cart_delitem(vsd,idx,amount,0,LOG_TYPE_VENDING);
		if (battle->bc->buyer_name) {
			char temp[256]; sprintf(temp,msg_sd(vsd,MSGTBL_NAME_BOUGHT_ITEM),sd->status.name);
			clif_disp_onlyself(vsd,temp);
		}
	}
	if (cfg_ex_vending_info) {
		char buf[256]; const char *cn=coin_name(vc);
		snprintf(buf,sizeof(buf),"%s comprou em sua loja. Lucro: %d %s",sd->status.name,(int)z,cn); clif_disp_onlyself(vsd,buf);
		snprintf(buf,sizeof(buf),"Voce comprou na loja de %s. Custo: %d %s",vsd->status.name,(int)z,cn); clif_disp_onlyself(sd,buf);
	}

	/* Compact list */
	for (i=0,cursor=0;i<vsd->vend_num;i++) {
		if (vsd->vending[i].amount==0) continue;
		if (cursor!=i) { vsd->vending[cursor].index=vsd->vending[i].index; vsd->vending[cursor].amount=vsd->vending[i].amount; vsd->vending[cursor].value=vsd->vending[i].value; }
		cursor++;
	}
	vsd->vend_num = cursor;
	if (map->save_settings&2) { chrif->save(sd,0); chrif->save(vsd,0); }
	if (vsd->state.autotrade) {
		ARR_FIND(0,vsd->vend_num,i,vsd->vending[i].amount>0);
		if (i==vsd->vend_num) { vending->close(vsd); map->quit(vsd); } else pc->autotrade_update(vsd,PAUC_REFRESH);
	}
}

static void hook_parse_mapflag_pre(const char **name, const char **w3, const char **w4,
	const char **start, const char **buffer, const char **filepath, int **retval)
{
	if (strcmpi(*name,"vending_cell")==0) hookStop();
}

/* ---- Lifecycle ---- */
HPExport void server_preinit(void) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) addBattleConf(hcfg[i].name,hcfg_parse,hcfg_return,false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE!=SERVER_TYPE_MAP) return;
	addAtcommand("vendcoin", vendcoin);
	addScriptCommand("setvendcoin","i", setvendcoin);
	addScriptCommand("getvendcoin","",  getvendcoin);
	addHookPre(vending, open,                hook_vending_open_pre);
	addHookPre(vending, purchase,            hook_vending_purchase_pre);
	addHookPre(npc,     parse_unknown_mapflag, hook_parse_mapflag_pre);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus Vending Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
