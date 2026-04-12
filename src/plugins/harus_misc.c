/**
 * Harus Misc Plugin — Kill Count, Hunting Missions, Toggle features, Restock,
 *                      Costume, Ancient WoE, remaining configs
 */
#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/clif.h"
#include "map/itemdb.h"
#include "map/log.h"
#include "map/map.h"
#include "map/mob.h"
#include "map/npc.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/storage.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

HPExport struct hplugin_info pinfo = {
	"Harus_Misc", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

/* ---- Constants ---- */
#define MAX_KILLCOUNT_TRACK 5
#define MAX_HUNTING_SLOTS   5

/* ---- Data ---- */
struct misc_pdata {
	int battleinfo;
	int showcast;
	int showcastdelay;
	int displaydrop;
	int view_aura;
	int user_font;
	struct { int mob_id; int count; } killtrack[MAX_KILLCOUNT_TRACK];
	int killtrack_total;
	struct { int mob_id; int count; } hunting[MAX_HUNTING_SLOTS];
	int hunting_time;
};

struct misc_mdata {
	unsigned int ancient    : 1;
	unsigned int noguildwar : 1;
};

/* ---- Configs ---- */
static int cfg_renewal_system_enable  = 0;
static int cfg_warg_can_falcon        = 0;
static int cfg_ancient_reserved_id    = 999997;
static int cfg_hunting_reserved_id    = 999998;
static int cfg_woe_reserved_id        = 999999;
static int cfg_channel_system_enable  = 1;
static int cfg_channel_announces      = 1;
static int cfg_channel_min_chat_delay = 1000;
static int cfg_guild_wars             = 0;
static int cfg_max_guild_opposition   = 3;
static int cfg_guild_skills_sep_delay = 0;
static int cfg_super_woe_enable       = 0;
static int cfg_region_display         = 0;
static int cfg_at_changegm_cost       = 0;
static int cfg_mob_slave_adddrop      = 0;
static int cfg_reflect_damage_fix     = 0;
static int cfg_dancing_weaponfix      = 0;
static int cfg_anti_mayapurple_hack   = 0;
static int cfg_reserved_costume_id    = 999;
static int cfg_autotrade_mapflag      = 0;
static int cfg_at_timeout             = 0;
static int cfg_at_tax                 = 0;
static int cfg_homunculus_autoloot    = 0;
static int cfg_idle_no_autoloot       = 0;
static int cfg_chat_allowed_interval  = 10;
static int cfg_chat_time_interval     = 10000;
static int cfg_chat_flood_automute    = 0;
static int cfg_action_keyboard_limit  = 0;
static int cfg_action_mouse_limit     = 0;
static int cfg_action_dual_limit      = 0;
static int cfg_myinfo_event_vote      = 0;

static struct { const char *name; int *val; } hcfg[] = {
	{ "renewal_system_enable",     &cfg_renewal_system_enable },
	{ "warg_can_falcon",           &cfg_warg_can_falcon },
	{ "ancient_reserved_char_id",  &cfg_ancient_reserved_id },
	{ "hunting_reserved_char_id",  &cfg_hunting_reserved_id },
	{ "woe_reserved_char_id",      &cfg_woe_reserved_id },
	{ "channel_system_enable",     &cfg_channel_system_enable },
	{ "channel_announces",         &cfg_channel_announces },
	{ "channel_min_chat_delay",    &cfg_channel_min_chat_delay },
	{ "guild_wars",                &cfg_guild_wars },
	{ "max_guild_opposition",      &cfg_max_guild_opposition },
	{ "guild_skills_separed_delay",&cfg_guild_skills_sep_delay },
	{ "super_woe_enable",          &cfg_super_woe_enable },
	{ "region_display",            &cfg_region_display },
	{ "at_changegm_cost",          &cfg_at_changegm_cost },
	{ "mob_slave_adddrop",         &cfg_mob_slave_adddrop },
	{ "reflect_damage_fix",        &cfg_reflect_damage_fix },
	{ "dancing_weaponchange_fix",  &cfg_dancing_weaponfix },
	{ "anti_mayapurple_hack",      &cfg_anti_mayapurple_hack },
	{ "reserved_costume_id",       &cfg_reserved_costume_id },
	{ "autotrade_mapflag",         &cfg_autotrade_mapflag },
	{ "at_timeout",                &cfg_at_timeout },
	{ "at_tax",                    &cfg_at_tax },
	{ "homunculus_autoloot",       &cfg_homunculus_autoloot },
	{ "idle_no_autoloot",          &cfg_idle_no_autoloot },
	{ "chat_allowed_per_interval", &cfg_chat_allowed_interval },
	{ "chat_time_interval",        &cfg_chat_time_interval },
	{ "chat_flood_automute",       &cfg_chat_flood_automute },
	{ "action_keyboard_limit",     &cfg_action_keyboard_limit },
	{ "action_mouse_limit",        &cfg_action_mouse_limit },
	{ "action_dual_limit",         &cfg_action_dual_limit },
	{ "myinfo_event_vote_points",  &cfg_myinfo_event_vote },
};
#define HCFG_COUNT (sizeof(hcfg)/sizeof(hcfg[0]))

static void hcfg_parse(const char *k, const char *v) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0){*hcfg[i].val=atoi(v);return;}
}
static int hcfg_return(const char *k) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0) return *hcfg[i].val; return 0;
}

/* ---- Helpers ---- */
static struct misc_pdata *mpd(struct map_session_data *sd) {
	struct misc_pdata *d;
	nullpo_retr(NULL, sd);
	d = getFromMSD(sd, 0);
	if (!d) { CREATE(d, struct misc_pdata, 1); memset(d,0,sizeof(*d)); addToMSD(sd,d,0,true); }
	return d;
}

static struct misc_mdata *mmd(int16 m) {
	struct misc_mdata *d;
	if (m < 0 || m >= map->count) return NULL;
	d = getFromMAPD(&map->list[m], 0);
	if (!d) { CREATE(d, struct misc_mdata, 1); memset(d,0,sizeof(*d)); addToMAPD(&map->list[m],d,0,true); }
	return d;
}

/* ---- Atcommands ---- */
ACMD(battleinfo) {
	struct misc_pdata *d = mpd(sd);
	d->battleinfo = !d->battleinfo;
	clif->message(fd, d->battleinfo ? "Battle Info: ON" : "Battle Info: OFF");
	return true;
}

ACMD(showcast) {
	struct misc_pdata *d = mpd(sd);
	d->showcast = !d->showcast;
	clif->message(fd, d->showcast ? "Show Cast: ON" : "Show Cast: OFF");
	return true;
}

ACMD(showcastdelay) {
	struct misc_pdata *d = mpd(sd);
	d->showcastdelay = !d->showcastdelay;
	clif->message(fd, d->showcastdelay ? "Show Cast Delay: ON" : "Show Cast Delay: OFF");
	return true;
}

ACMD(displaydrop) {
	struct misc_pdata *d = mpd(sd);
	d->displaydrop = !d->displaydrop;
	clif->message(fd, d->displaydrop ? "Display Drop: ON" : "Display Drop: OFF");
	return true;
}

ACMD(harusaura) {
	struct misc_pdata *d = mpd(sd);
	int eff = 0;
	if (*message) eff = atoi(message);
	d->view_aura = eff;
	if (eff) { char buf[64]; snprintf(buf,sizeof(buf),"Aura definida: %d",eff); clif->message(fd,buf); }
	else clif->message(fd,"Aura removida.");
	pc->setpos(sd, sd->mapindex, sd->bl.x, sd->bl.y, CLR_OUTSIGHT);
	return true;
}

ACMD(killcount) {
	struct misc_pdata *d = mpd(sd);
	char buf[256]; int i;
	clif->message(fd, "=== Kill Count ===");
	for (i = 0; i < MAX_KILLCOUNT_TRACK; i++) {
		if (d->killtrack[i].mob_id) {
			struct mob_db *mdb = mob->db(d->killtrack[i].mob_id);
			snprintf(buf,sizeof(buf),"  [%d] %s: %d", i+1, mdb?mdb->jname:"???", d->killtrack[i].count);
			clif->message(fd,buf);
		}
	}
	snprintf(buf,sizeof(buf),"Total: %d", d->killtrack_total);
	clif->message(fd,buf);
	return true;
}

ACMD(mission) {
	struct misc_pdata *d = mpd(sd);
	char buf[256]; int i;
	if (!d->hunting_time) { clif->message(fd,"Sem missao ativa."); return true; }
	clif->message(fd, "=== Missao de Caca ===");
	for (i = 0; i < MAX_HUNTING_SLOTS; i++) {
		if (d->hunting[i].mob_id) {
			struct mob_db *mdb = mob->db(d->hunting[i].mob_id);
			snprintf(buf,sizeof(buf),"  %s: %d abatidos", mdb?mdb->jname:"???", d->hunting[i].count);
			clif->message(fd,buf);
		}
	}
	return true;
}

ACMD(restock) {
	npc->event(sd, "restock_npc::Onused", 0);
	return true;
}

/* ---- Script commands ---- */
BUILDIN(killcountget) {
	struct map_session_data *sd=script->rid2sd(st);
	int s=script_getnum(st,2);
	if(!sd||s<0||s>=MAX_KILLCOUNT_TRACK){script_pushint(st,0);return true;}
	script_pushint(st,mpd(sd)->killtrack[s].count); return true;
}

BUILDIN(killcountname) {
	struct map_session_data *sd=script->rid2sd(st);
	int s=script_getnum(st,2);
	if(!sd||s<0||s>=MAX_KILLCOUNT_TRACK||!mpd(sd)->killtrack[s].mob_id){script_pushconststr(st,"");return true;}
	script_pushstrcopy(st, mob->db(mpd(sd)->killtrack[s].mob_id)->jname); return true;
}

BUILDIN(killcountval) {
	struct map_session_data *sd=script->rid2sd(st);
	int s=script_getnum(st,2);
	if(!sd||s<0||s>=MAX_KILLCOUNT_TRACK){script_pushint(st,0);return true;}
	script_pushint(st,mpd(sd)->killtrack[s].mob_id); return true;
}

BUILDIN(killcountadd) {
	struct map_session_data *sd=script->rid2sd(st); int mid,i; struct misc_pdata *d;
	if(!sd){script_pushint(st,0);return true;} d=mpd(sd); mid=script_getnum(st,2);
	for(i=0;i<MAX_KILLCOUNT_TRACK;i++){
		if(d->killtrack[i].mob_id==0){d->killtrack[i].mob_id=mid;d->killtrack[i].count=0;script_pushint(st,1);return true;}
	}
	script_pushint(st,0); return true;
}

BUILDIN(killcountremove) {
	struct map_session_data *sd=script->rid2sd(st);
	int s=script_getnum(st,2);
	if(!sd||s<0||s>=MAX_KILLCOUNT_TRACK){script_pushint(st,0);return true;}
	mpd(sd)->killtrack[s].mob_id=0; mpd(sd)->killtrack[s].count=0;
	script_pushint(st,1); return true;
}

BUILDIN(killcountreset) {
	struct map_session_data *sd=script->rid2sd(st);
	if(sd){struct misc_pdata *d=mpd(sd);memset(d->killtrack,0,sizeof(d->killtrack));d->killtrack_total=0;}
	return true;
}

BUILDIN(killcounttotal) {
	struct map_session_data *sd=script->rid2sd(st);
	script_pushint(st,sd?mpd(sd)->killtrack_total:0); return true;
}

/* Hunting */
BUILDIN(mission_sethunting) {
	struct map_session_data *sd=script->rid2sd(st);
	int s=script_getnum(st,2);
	if(!sd||s<0||s>=MAX_HUNTING_SLOTS) return true;
	mpd(sd)->hunting[s].mob_id=script_getnum(st,3);
	mpd(sd)->hunting[s].count=script_getnum(st,4); return true;
}

BUILDIN(mission_settime) {
	struct map_session_data *sd=script->rid2sd(st);
	if(sd) mpd(sd)->hunting_time=(int)time(NULL)+script_getnum(st,2); return true;
}

/* Costume */
BUILDIN(costume_cmd) {
	struct map_session_data *sd=script->rid2sd(st); int headid;
	if(!sd) return true;
	headid=script_getnum(st,2);
	if(headid>0){sd->status.look.head_top=headid;clif->changelook(&sd->bl,LOOK_HEAD_TOP,headid);}
	return true;
}

/* Item Bound */
BUILDIN(itembound) {
	struct map_session_data *sd=script->rid2sd(st); struct item it;
	int nameid,amount,bound;
	if(!sd) return true;
	nameid=script_getnum(st,2); amount=script_getnum(st,3);
	bound=script_hasdata(st,4)?script_getnum(st,4):1;
	memset(&it,0,sizeof(it)); it.nameid=nameid; it.identify=1; it.amount=amount; it.bound=(unsigned char)bound;
	pc->additem(sd,&it,amount,LOG_TYPE_SCRIPT); return true;
}

/* Rent Storage */
BUILDIN(openrentstorage) {
	struct map_session_data *sd=script->rid2sd(st);
	if(sd) storage->open(sd); return true;
}

/* Ancient WoE class check */
BUILDIN(class2ancientwoe) {
	struct map_session_data *sd=script->rid2sd(st);
	script_pushint(st,(sd&&sd->status.class<4001)?1:0); return true;
}

/* Restock script command */
BUILDIN(restock_cmd) {
	struct map_session_data *sd=script->rid2sd(st);
	int nameid,amount,i,count;
	if(!sd) return true;
	nameid=script_getnum(st,2); amount=script_getnum(st,3);
	if(nameid<=0||amount<=0) return true;
	if(!sd->storage.received) return true;
	count=VECTOR_LENGTH(sd->storage.item);
	for(i=0;i<count;i++){
		struct item *it=&VECTOR_INDEX(sd->storage.item,i);
		if(it->nameid==nameid&&it->amount>0){
			if(it->amount<amount)amount=it->amount;
			storage->get(sd,i,amount); break;
		}
	}
	return true;
}

/* Set ancient map flag via script */
BUILDIN(setancient) {
	const char *mapname=script_getstr(st,2); int val=script_getnum(st,3);
	int16 m=map->mapname2mapid(mapname); struct misc_mdata *md;
	if(m<0) return true; md=mmd(m);
	if(md) md->ancient=val?1:0; return true;
}

/* ---- Hooks ---- */
static int hook_mob_dead_post(int retVal, struct mob_data *md, struct block_list *src, int type) {
	struct map_session_data *sd; struct misc_pdata *d; int i;
	if (!src || src->type != BL_PC) return retVal;
	sd = BL_CAST(BL_PC, src); if (!sd) return retVal;
	d = mpd(sd);
	for (i = 0; i < MAX_KILLCOUNT_TRACK; i++) {
		if (d->killtrack[i].mob_id == md->class_) { d->killtrack[i].count++; d->killtrack_total++; break; }
	}
	for (i = 0; i < MAX_HUNTING_SLOTS; i++) {
		if (d->hunting[i].mob_id == md->class_) d->hunting[i].count++;
	}
	return retVal;
}

static int hook_pc_useitem_post(int retVal, struct map_session_data *sd, int n) {
	if (retVal == 1 && sd != NULL) {
		int nameid = sd->status.inventory[n].nameid;
		pc->setreg(sd, script->add_variable("@consumed_id"), nameid);
		npc->event(sd, "restock_npc::OnPCConsumeEvent", 0);
	}
	return retVal;
}

static void hook_parse_unknown_mapflag_pre(const char **name, const char **w3, const char **w4,
	const char **start, const char **buffer, const char **filepath, int **retval)
{
	if (strcmpi(*name, "ancient") == 0
	 || strcmpi(*name, "noguildwar") == 0
	 || strcmpi(*name, "vending_cell") == 0)
		hookStop();
}

/* ---- Lifecycle ---- */
HPExport void server_preinit(void) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) addBattleConf(hcfg[i].name,hcfg_parse,hcfg_return,false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE!=SERVER_TYPE_MAP) return;
	/* Atcommands */
	addAtcommand("battleinfo",    battleinfo);
	addAtcommand("showcast",      showcast);
	addAtcommand("showcastdelay", showcastdelay);
	addAtcommand("displaydrop",   displaydrop);
	addAtcommand("ddrop",         displaydrop);
	addAtcommand("aura",          harusaura);
	addAtcommand("killcount",     killcount);
	addAtcommand("kc",            killcount);
	addAtcommand("mission",       mission);
	addAtcommand("restock",       restock);
	/* Script commands */
	addScriptCommand("killcountget",     "i",   killcountget);
	addScriptCommand("killcountname",    "i",   killcountname);
	addScriptCommand("killcountval",     "i",   killcountval);
	addScriptCommand("killcountadd",     "i",   killcountadd);
	addScriptCommand("killcountremove",  "i",   killcountremove);
	addScriptCommand("killcountreset",   "",    killcountreset);
	addScriptCommand("killcounttotal",   "",    killcounttotal);
	addScriptCommand("mission_sethunting","iii", mission_sethunting);
	addScriptCommand("mission_settime",  "i",   mission_settime);
	addScriptCommand("costume",          "i",   costume_cmd);
	addScriptCommand("itembound",        "ii?", itembound);
	addScriptCommand("openrentstorage",  "",    openrentstorage);
	addScriptCommand("class2ancientwoe", "",    class2ancientwoe);
	addScriptCommand("restock",          "ii",  restock_cmd);
	addScriptCommand("setancient",       "si",  setancient);
	/* Hooks */
	addHookPost(mob, dead,                  hook_mob_dead_post);
	addHookPost(pc,  useitem,               hook_pc_useitem_post);
	addHookPre(npc,  parse_unknown_mapflag, hook_parse_unknown_mapflag_pre);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus Misc Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
