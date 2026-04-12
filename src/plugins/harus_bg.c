/**
 * Harus BG Plugin — Battleground extended script commands + queue stubs
 */
#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/battleground.h"
#include "map/clif.h"
#include "map/itemdb.h"
#include "map/log.h"
#include "map/map.h"
#include "common/mapindex.h"
#include "map/npc.h"
#include "map/pc.h"
#include "map/script.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HPExport struct hplugin_info pinfo = {
	"Harus_BG", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

/* ---- Configs ---- */
static int cfg_bg_short_damage_rate   = 80;
static int cfg_bg_long_damage_rate    = 80;
static int cfg_bg_weapon_damage_rate  = 60;
static int cfg_bg_magic_damage_rate   = 60;
static int cfg_bg_misc_damage_rate    = 60;
static int cfg_bg_idle_announce       = 300000;
static int cfg_bg_idle_autokick       = 0;
static int cfg_bg_reserved_char_id    = 999996;
static int cfg_bg_items_on_pvp        = 1;
static int cfg_bg_reward_rates        = 100;
static int cfg_bg_ranking_bonus       = 1;
static int cfg_bg_ranked_mode         = 0;
static int cfg_bg_ranked_max_games    = 30;
static int cfg_bg_reportafk_leaderonly= 0;
static int cfg_bg_queue2team_balanced = 1;
static int cfg_bg_logincount_check    = 0;
static int cfg_bg_queue_onlytowns    = 0;
static int cfg_bg_eAmod_mode         = 0;

static struct { const char *name; int *val; } hcfg[] = {
	{ "bg_short_damage_rate",   &cfg_bg_short_damage_rate },
	{ "bg_long_damage_rate",    &cfg_bg_long_damage_rate },
	{ "bg_weapon_damage_rate",  &cfg_bg_weapon_damage_rate },
	{ "bg_magic_damage_rate",   &cfg_bg_magic_damage_rate },
	{ "bg_misc_damage_rate",    &cfg_bg_misc_damage_rate },
	{ "bg_idle_announce",       &cfg_bg_idle_announce },
	{ "bg_idle_autokick",       &cfg_bg_idle_autokick },
	{ "bg_reserved_char_id",    &cfg_bg_reserved_char_id },
	{ "bg_items_on_pvp",        &cfg_bg_items_on_pvp },
	{ "bg_reward_rates",        &cfg_bg_reward_rates },
	{ "bg_ranking_bonus",       &cfg_bg_ranking_bonus },
	{ "bg_ranked_mode",         &cfg_bg_ranked_mode },
	{ "bg_ranked_max_games",    &cfg_bg_ranked_max_games },
	{ "bg_reportafk_leaderonly",&cfg_bg_reportafk_leaderonly },
	{ "bg_queue2team_balanced", &cfg_bg_queue2team_balanced },
	{ "bg_logincount_check",    &cfg_bg_logincount_check },
	{ "bg_queue_onlytowns",     &cfg_bg_queue_onlytowns },
	{ "bg_eAmod_mode",          &cfg_bg_eAmod_mode },
};
#define HCFG_COUNT (sizeof(hcfg)/sizeof(hcfg[0]))

static void hcfg_parse(const char *k, const char *v) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0){*hcfg[i].val=atoi(v);return;}
}
static int hcfg_return(const char *k) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0) return *hcfg[i].val; return 0;
}

/* ---- Script commands ---- */
BUILDIN(bg_team_create) {
	const char *mn=script_getstr(st,2); int x=script_getnum(st,3),y=script_getnum(st,4);
	const char *le=script_getstr(st,5),*de=script_getstr(st,6);
	unsigned short mi=mapindex->name2id(mn);
	script_pushint(st,bg->create(mi,(short)x,(short)y,le,de)); return true;
}

BUILDIN(bg_getitem) {
	int bgid=script_getnum(st,2),nameid=script_getnum(st,3),amount=script_getnum(st,4),i;
	struct battleground_data *bgd=bg->team_search(bgid);
	if(!bgd){script_pushint(st,0);return true;}
	for(i=0;i<MAX_BG_MEMBERS;i++){struct map_session_data *p=bgd->members[i].sd;if(p){struct item it;memset(&it,0,sizeof(it));it.nameid=nameid;it.identify=1;pc->additem(p,&it,amount,LOG_TYPE_SCRIPT);}}
	script_pushint(st,1);return true;
}

BUILDIN(bg_reward) {
	int bgid=script_getnum(st,2),nameid=script_getnum(st,3),amount=script_getnum(st,4),i;
	struct battleground_data *bgd=bg->team_search(bgid);
	if(!bgd){script_pushint(st,0);return true;}
	amount=amount*cfg_bg_reward_rates/100; if(amount<1) amount=1;
	for(i=0;i<MAX_BG_MEMBERS;i++){struct map_session_data *p=bgd->members[i].sd;if(p&&nameid>0){struct item it;memset(&it,0,sizeof(it));it.nameid=nameid;it.identify=1;pc->additem(p,&it,amount,LOG_TYPE_SCRIPT);}}
	script_pushint(st,1);return true;
}

BUILDIN(bg_rankpoints) {
	int bgid=script_getnum(st,2),amount=script_getnum(st,3),i;
	struct battleground_data *bgd=bg->team_search(bgid);
	if(!bgd) return true;
	for(i=0;i<MAX_BG_MEMBERS;i++){struct map_session_data *p=bgd->members[i].sd;if(p){int c=pc->readregistry(p,script->add_variable("bg_rankpoints"));pc->setregistry(p,script->add_variable("bg_rankpoints"),c+amount);}}
	return true;
}

BUILDIN(bg_team_reveal) { script_pushint(st,bg->team_search(script_getnum(st,2))?1:0); return true; }
BUILDIN(bg_team_setquest) { script_pushint(st,1); return true; }

BUILDIN(bg_logincount) {
	const char *mn=script_getstr(st,2); int16 m=map->mapname2mapid(mn);
	struct s_mapiterator *iter; struct map_session_data *pl; int c=0;
	if(m<0){script_pushint(st,0);return true;}
	iter=mapit_getallusers();
	for(pl=BL_UCAST(BL_PC,mapit->first(iter));mapit->exists(iter);pl=BL_UCAST(BL_PC,mapit->next(iter))) if(pl->bl.m==m) c++;
	mapit->free(iter); script_pushint(st,c); return true;
}
BUILDIN(map_logincount) { return buildin_bg_logincount(st); }

BUILDIN(bg_clean) {
	int bgid=script_getnum(st,2),i; struct battleground_data *bgd=bg->team_search(bgid);
	if(!bgd) return true;
	for(i=MAX_BG_MEMBERS-1;i>=0;i--) if(bgd->members[i].sd) bg->team_leave(bgd->members[i].sd,BGTL_QUIT);
	return true;
}

BUILDIN(bg_cleanmap) { const char *mn=script_getstr(st,2); int16 m=map->mapname2mapid(mn); if(m>=0) map->foreachinmap(map->removemobs_sub,m,BL_MOB); return true; }
BUILDIN(bg_team_guildid) { script_pushint(st,bg->team_search(script_getnum(st,2))?script_getnum(st,2):0); return true; }

BUILDIN(bg_gettriumphpoints) {
	struct map_session_data *sd=script->rid2sd(st);
	script_pushint(st,sd?pc->readregistry(sd,script->add_variable("triumph_points")):0); return true;
}

BUILDIN(bg_monster_reveal)  { script_pushint(st,1); return true; }
BUILDIN(bg_monster_inmunity){ script_pushint(st,1); return true; }

BUILDIN(bg_team_updatescore) {
	const char *mn=script_getstr(st,2); int16 m=map->mapname2mapid(mn);
	if(m>=0) clif->bg_updatescore(m); return true;
}

/* Queue stubs */
BUILDIN(bg_queue_create)      { script_pushint(st,1); return true; }
BUILDIN(bg_queue_event)       { return true; }
BUILDIN(bg_queue_join)        { return true; }
BUILDIN(bg_queue_leave)       { return true; }
BUILDIN(bg_queue_partyjoin)   { return true; }
BUILDIN(bg_queue_data)        { script_pushint(st,0); return true; }
BUILDIN(bg_queue2team)        { script_pushint(st,0); return true; }
BUILDIN(bg_queue2team_single) { script_pushint(st,0); return true; }
BUILDIN(bg_queue2teams)       { script_pushint(st,0); return true; }
BUILDIN(bg_balance_teams)     { return true; }

/* ---- Lifecycle ---- */
HPExport void server_preinit(void) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) addBattleConf(hcfg[i].name,hcfg_parse,hcfg_return,false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE!=SERVER_TYPE_MAP) return;
	addScriptCommand("bg_team_create",    "siiss", bg_team_create);
	addScriptCommand("bg_getitem",        "iii",   bg_getitem);
	addScriptCommand("bg_reward",         "iii",   bg_reward);
	addScriptCommand("bg_rankpoints",     "ii",    bg_rankpoints);
	addScriptCommand("bg_team_reveal",    "i",     bg_team_reveal);
	addScriptCommand("bg_team_setquest",  "ii",    bg_team_setquest);
	addScriptCommand("bg_logincount",     "s",     bg_logincount);
	addScriptCommand("map_logincount",    "s",     map_logincount);
	addScriptCommand("bg_clean",          "i",     bg_clean);
	addScriptCommand("bg_cleanmap",       "s",     bg_cleanmap);
	addScriptCommand("bg_team_guildid",   "i",     bg_team_guildid);
	addScriptCommand("bg_gettriumphpoints","i",    bg_gettriumphpoints);
	addScriptCommand("bg_team_updatescore","sii",  bg_team_updatescore);
	addScriptCommand("bg_monster_reveal", "ii",    bg_monster_reveal);
	addScriptCommand("bg_monster_inmunity","ii",   bg_monster_inmunity);
	addScriptCommand("bg_queue_create",    "si?",  bg_queue_create);
	addScriptCommand("bg_queue_event",     "is",   bg_queue_event);
	addScriptCommand("bg_queue_join",      "i",    bg_queue_join);
	addScriptCommand("bg_queue_leave",     "i",    bg_queue_leave);
	addScriptCommand("bg_queue_partyjoin", "i",    bg_queue_partyjoin);
	addScriptCommand("bg_queue_data",      "ii",   bg_queue_data);
	addScriptCommand("bg_queue2team",      "ii",   bg_queue2team);
	addScriptCommand("bg_queue2team_single","iii",  bg_queue2team_single);
	addScriptCommand("bg_queue2teams",     "iiiii",bg_queue2teams);
	addScriptCommand("bg_balance_teams",   "",     bg_balance_teams);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus BG Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
