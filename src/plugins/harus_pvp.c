/**
 * Harus PvP Plugin — PvP Mode toggle + PvP Event system
 */
#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"
#include "common/timer.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/clif.h"
#include "map/map.h"
#include "map/mob.h"
#include "map/npc.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/status.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HPExport struct hplugin_info pinfo = {
	"Harus_PvP", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

/* ---- Data ---- */
#define PVP_DATA_ID 0
#define PVP_MAPDATA_ID 0

struct pvp_pdata {
	int pvpmode;
	int64 pvpmode_tick;
	int pvpevent_fame;
};

struct pvp_mdata {
	unsigned int pvpevent  : 1;
	unsigned int nopvpmode : 1;
};

static int pvpevent_flag = 0;

/* ---- Configs ---- */
static int cfg_pvpmode_onlypc        = 1;
static int cfg_pvpmode_gvgreductions = 0;
static int cfg_pvpmode_expbonus      = 0;
static int cfg_pvpmode_nowarp_cmd    = 0;
static int cfg_pvpmode_enable_delay  = 30000;
static int cfg_pvpmode_disable_delay = 30000;
static int cfg_pvpevent_short_damage = 100;
static int cfg_pvpevent_long_damage  = 100;
static int cfg_pvpevent_weapon_damage= 100;
static int cfg_pvpevent_magic_damage = 100;
static int cfg_pvpevent_misc_damage  = 100;
static int cfg_pvpevent_flee_penalty = 0;
static int cfg_pvpevent_cashperkill  = 0;

static struct { const char *name; int *val; } hcfg[] = {
	{ "pvpmode_onlypc",             &cfg_pvpmode_onlypc },
	{ "pvpmode_gvgreductions",      &cfg_pvpmode_gvgreductions },
	{ "pvpmode_expbonus",           &cfg_pvpmode_expbonus },
	{ "pvpmode_nowarp_cmd",         &cfg_pvpmode_nowarp_cmd },
	{ "pvpmode_enable_delay",       &cfg_pvpmode_enable_delay },
	{ "pvpmode_disable_delay",      &cfg_pvpmode_disable_delay },
	{ "pvpevent_short_damage_rate", &cfg_pvpevent_short_damage },
	{ "pvpevent_long_damage_rate",  &cfg_pvpevent_long_damage },
	{ "pvpevent_weapon_damage_rate",&cfg_pvpevent_weapon_damage },
	{ "pvpevent_magic_damage_rate", &cfg_pvpevent_magic_damage },
	{ "pvpevent_misc_damage_rate",  &cfg_pvpevent_misc_damage },
	{ "pvpevent_flee_penalty",      &cfg_pvpevent_flee_penalty },
	{ "pvpevent_cashperkill",       &cfg_pvpevent_cashperkill },
};
#define HCFG_COUNT (sizeof(hcfg)/sizeof(hcfg[0]))

static void hcfg_parse(const char *k, const char *v) {
	int i; for (i=0;i<(int)HCFG_COUNT;i++) if (strcmpi(k,hcfg[i].name)==0) { *hcfg[i].val=atoi(v); return; }
}
static int hcfg_return(const char *k) {
	int i; for (i=0;i<(int)HCFG_COUNT;i++) if (strcmpi(k,hcfg[i].name)==0) return *hcfg[i].val; return 0;
}

/* ---- Helpers ---- */
static struct pvp_pdata *ppd(struct map_session_data *sd) {
	struct pvp_pdata *d;
	nullpo_retr(NULL, sd);
	d = getFromMSD(sd, PVP_DATA_ID);
	if (!d) { CREATE(d, struct pvp_pdata, 1); memset(d,0,sizeof(*d)); addToMSD(sd,d,PVP_DATA_ID,true); }
	return d;
}

static struct pvp_mdata *pmd(int16 m) {
	struct pvp_mdata *d;
	if (m<0||m>=map->count) return NULL;
	d = getFromMAPD(&map->list[m], PVP_MAPDATA_ID);
	if (!d) { CREATE(d, struct pvp_mdata, 1); memset(d,0,sizeof(*d)); addToMAPD(&map->list[m],d,PVP_MAPDATA_ID,true); }
	return d;
}

static void pvpmode_on(struct map_session_data *sd) {
	struct pvp_pdata *d;
	struct pvp_mdata *md;
	nullpo_retv(sd);
	d = ppd(sd); md = pmd(sd->bl.m);
	if (d->pvpmode) { clif->message(sd->fd,"PvP Mode ja esta ativo."); return; }
	if (md && md->nopvpmode) { clif->message(sd->fd,"PvP Mode nao permitido neste mapa."); return; }
	if (map->list[sd->bl.m].flag.gvg || map->list[sd->bl.m].flag.battleground) { clif->message(sd->fd,"Voce nao pode usar PvP Mode neste mapa."); return; }
	if (sd->duel_group > 0) { clif->message(sd->fd,"Voce nao pode usar PvP Mode durante um duelo."); return; }
	if (cfg_pvpmode_enable_delay > 0 && d->pvpmode_tick != 0) {
		int64 elapsed = timer->gettick() - d->pvpmode_tick;
		if (elapsed < cfg_pvpmode_enable_delay) {
			char buf[128]; snprintf(buf,sizeof(buf),"Aguarde %d segundos.",(int)((cfg_pvpmode_enable_delay-elapsed)/1000));
			clif->message(sd->fd,buf); return;
		}
	}
	d->pvpmode = 1; d->pvpmode_tick = timer->gettick();
	sd->state.killer = 1; sd->state.killable = 1;
	clif->map_property(sd, MAPPROPERTY_FREEPVPZONE);
	clif->message(sd->fd,"PvP Mode ativado!");
}

static void pvpmode_off(struct map_session_data *sd) {
	struct pvp_pdata *d;
	nullpo_retv(sd);
	d = ppd(sd);
	if (!d->pvpmode) { clif->message(sd->fd,"PvP Mode ja esta desativado."); return; }
	if (cfg_pvpmode_disable_delay > 0 && d->pvpmode_tick != 0) {
		int64 elapsed = timer->gettick() - d->pvpmode_tick;
		if (elapsed < cfg_pvpmode_disable_delay) {
			char buf[128]; snprintf(buf,sizeof(buf),"Aguarde %d segundos.",(int)((cfg_pvpmode_disable_delay-elapsed)/1000));
			clif->message(sd->fd,buf); return;
		}
	}
	d->pvpmode = 0; d->pvpmode_tick = timer->gettick();
	sd->state.killer = 0; sd->state.killable = 0;
	clif->map_property(sd, MAPPROPERTY_NOTHING);
	clif->message(sd->fd,"PvP Mode desativado.");
}

/* ---- Atcommands ---- */
ACMD(pvpmode) { struct pvp_pdata *d=ppd(sd); if(d->pvpmode) pvpmode_off(sd); else pvpmode_on(sd); return true; }

ACMD(whopk) {
	struct s_mapiterator *iter; struct map_session_data *pl; char buf[256]; int count=0;
	clif->message(fd,"=== Jogadores em PvP Mode ===");
	iter = mapit_getallusers();
	for (pl=BL_UCAST(BL_PC,mapit->first(iter)); mapit->exists(iter); pl=BL_UCAST(BL_PC,mapit->next(iter))) {
		struct pvp_pdata *pd = getFromMSD(pl, PVP_DATA_ID);
		if (pd && pd->pvpmode && pl->bl.m==sd->bl.m) { snprintf(buf,sizeof(buf),"  %s (Lv.%d)",pl->status.name,pl->status.base_level); clif->message(fd,buf); count++; }
	}
	mapit->free(iter); snprintf(buf,sizeof(buf),"Total: %d",count); clif->message(fd,buf); return true;
}

/* ---- Script commands ---- */
BUILDIN(getpvpmode) { struct map_session_data *sd=script->rid2sd(st); script_pushint(st,sd?ppd(sd)->pvpmode:0); return true; }
BUILDIN(pvpeventstart) { pvpevent_flag=1; return true; }
BUILDIN(pvpeventstop)  { pvpevent_flag=0; return true; }
BUILDIN(pvpeventcheck) { script_pushint(st,pvpevent_flag); return true; }
BUILDIN(pvpevent_addpoints) { struct map_session_data *sd=script->rid2sd(st); if(sd) ppd(sd)->pvpevent_fame+=script_getnum(st,2); return true; }

BUILDIN(setpvpevent) {
	const char *mn=script_getstr(st,2); int v=script_getnum(st,3);
	int16 m=map->mapname2mapid(mn); struct pvp_mdata *md;
	if(m<0) return true; md=pmd(m); if(md) md->pvpevent=v?1:0; return true;
}
BUILDIN(setnopvpmode) {
	const char *mn=script_getstr(st,2); int v=script_getnum(st,3);
	int16 m=map->mapname2mapid(mn); struct pvp_mdata *md;
	if(m<0) return true; md=pmd(m); if(md) md->nopvpmode=v?1:0; return true;
}

/* ---- Hooks ---- */
static int hook_pc_dead_post(int retVal, struct map_session_data *sd, struct block_list *src) {
	struct pvp_pdata *d = getFromMSD(sd, PVP_DATA_ID);
	if (d && d->pvpmode) { d->pvpmode=0; sd->state.killer=0; sd->state.killable=0; }
	return retVal;
}

static bool hook_pc_authok_post(bool retVal, struct map_session_data *sd,
	int login_id2, time_t expiration_time, int group_id,
	const struct mmo_charstatus *st, bool changing_mapservers)
{
	if (retVal && sd) { struct pvp_pdata *d=getFromMSD(sd,PVP_DATA_ID); if(d&&d->pvpmode){sd->state.killer=1;sd->state.killable=1;} }
	return retVal;
}

static int64 hook_calc_damage_post(int64 retVal, struct block_list *src, struct block_list *bl,
	struct Damage *d, int64 damage, uint16 skill_id, uint16 skill_lv)
{
	if (retVal<=0||!src||!bl) return retVal;
	if (pvpevent_flag) { struct pvp_mdata *md=pmd(src->m); if(md&&md->pvpevent) retVal=retVal*cfg_pvpevent_weapon_damage/100; }
	return retVal;
}

static int hook_mob_dead_post(int retVal, struct mob_data *md, struct block_list *src, int type) {
	struct map_session_data *sd;
	if (!src||src->type!=BL_PC) return retVal;
	sd = BL_CAST(BL_PC, src); if(!sd) return retVal;
	if (pvpevent_flag && cfg_pvpevent_cashperkill>0) {
		struct pvp_mdata *mdata=pmd(sd->bl.m);
		if(mdata && mdata->pvpevent) pc->getcash(sd, cfg_pvpevent_cashperkill, 0);
	}
	return retVal;
}

static void hook_parse_mapflag_pre(const char **name, const char **w3, const char **w4,
	const char **start, const char **buffer, const char **filepath, int **retval)
{
	if (strcmpi(*name,"pvp_event")==0||strcmpi(*name,"pvpevent")==0
	 ||strcmpi(*name,"nopvpmode")==0||strcmpi(*name,"pvpe_area")==0)
		hookStop();
}

/* ---- Lifecycle ---- */
HPExport void server_preinit(void) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) addBattleConf(hcfg[i].name,hcfg_parse,hcfg_return,false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE!=SERVER_TYPE_MAP) return;
	addAtcommand("pvpmode", pvpmode);
	addAtcommand("whopk",   whopk);
	addScriptCommand("getpvpmode",        "",  getpvpmode);
	addScriptCommand("pvpeventstart",     "",  pvpeventstart);
	addScriptCommand("pvpeventstop",      "",  pvpeventstop);
	addScriptCommand("pvpeventcheck",     "",  pvpeventcheck);
	addScriptCommand("pvpevent_addpoints","i", pvpevent_addpoints);
	addScriptCommand("setpvpevent",       "si",setpvpevent);
	addScriptCommand("setnopvpmode",      "si",setnopvpmode);
	addHookPost(pc,     dead,                hook_pc_dead_post);
	addHookPost(pc,     authok,              hook_pc_authok_post);
	addHookPost(battle, calc_damage,         hook_calc_damage_post);
	addHookPost(mob,    dead,                hook_mob_dead_post);
	addHookPre(npc,     parse_unknown_mapflag, hook_parse_mapflag_pre);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus PvP Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
