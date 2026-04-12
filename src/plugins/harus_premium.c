/**
 * Harus Premium Plugin — VIP system, GoldPC timer, session EXP tracking
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
#include "map/npc.h"
#include "map/pc.h"
#include "map/script.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

HPExport struct hplugin_info pinfo = {
	"Harus_Premium", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

/* ---- Forward decl ---- */
static int goldpc_timer_func(int tid, int64 tick, int id, intptr_t data);

/* ---- Data ---- */
#define PREM_DATA_ID 0

struct prem_pdata {
	int premium_tick;
	int goldpc_points;
	int goldpc_tid;
	int session_start;
	uint64 session_base_exp;
	uint64 session_job_exp;
};

/* ---- Configs ---- */
static int cfg_premium_bonusexp  = 50;
static int cfg_premium_dropboost = 50;
static int cfg_premium_discount  = 10;
static int cfg_goldpc_timer      = 0;
static int cfg_goldpc_timer_rates= 0;
static int cfg_goldpc_value      = 1;
static int cfg_goldpc_ticks      = 3600;
static int cfg_goldpc_vip        = 0;
static int cfg_goldpc_maxpoints  = 100;

static struct { const char *name; int *val; } hcfg[] = {
	{ "premium_bonusexp",          &cfg_premium_bonusexp },
	{ "premium_dropboost",         &cfg_premium_dropboost },
	{ "premium_discount",          &cfg_premium_discount },
	{ "feature_goldpc_timer",      &cfg_goldpc_timer },
	{ "feature_goldpc_timer_rates",&cfg_goldpc_timer_rates },
	{ "feature_goldpc_value",      &cfg_goldpc_value },
	{ "feature_goldpc_ticks",      &cfg_goldpc_ticks },
	{ "feature_goldpc_vip",        &cfg_goldpc_vip },
	{ "feature_goldpc_maxpoints",  &cfg_goldpc_maxpoints },
};
#define HCFG_COUNT (sizeof(hcfg)/sizeof(hcfg[0]))

static void hcfg_parse(const char *k, const char *v) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0){*hcfg[i].val=atoi(v);return;}
}
static int hcfg_return(const char *k) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) if(strcmpi(k,hcfg[i].name)==0) return *hcfg[i].val; return 0;
}

/* ---- Helpers ---- */
static struct prem_pdata *ppd(struct map_session_data *sd) {
	struct prem_pdata *d;
	nullpo_retr(NULL, sd);
	d = getFromMSD(sd, PREM_DATA_ID);
	if (!d) { CREATE(d, struct prem_pdata, 1); memset(d,0,sizeof(*d)); d->goldpc_tid=INVALID_TIMER; addToMSD(sd,d,PREM_DATA_ID,true); }
	return d;
}

static bool is_premium(struct map_session_data *sd) {
	struct prem_pdata *d = ppd(sd);
	return (d && d->premium_tick > (int)time(NULL));
}

/* ---- GoldPC ---- */
static int goldpc_timer_func(int tid, int64 tick, int id, intptr_t data_param) {
	struct map_session_data *sd = map->id2sd(id);
	struct prem_pdata *d;
	int add;
	if (!sd) return 0;
	d = ppd(sd); if (!d) return 0;
	add = cfg_goldpc_value;
	if (cfg_goldpc_vip && is_premium(sd)) add *= 2;
	d->goldpc_points += add;
	if (d->goldpc_points > cfg_goldpc_maxpoints) d->goldpc_points = cfg_goldpc_maxpoints;
	npc->event(sd, "GoldPC::OnTimer", 0);
	if (cfg_goldpc_timer)
		d->goldpc_tid = timer->add(timer->gettick()+(int64)cfg_goldpc_ticks*1000, goldpc_timer_func, sd->bl.id, 0);
	return 0;
}

static void goldpc_start(struct map_session_data *sd) {
	struct prem_pdata *d;
	if (!cfg_goldpc_timer) return;
	nullpo_retv(sd); d = ppd(sd);
	if (d->goldpc_tid != INVALID_TIMER) timer->delete(d->goldpc_tid, goldpc_timer_func);
	d->goldpc_tid = timer->add(timer->gettick()+(int64)cfg_goldpc_ticks*1000, goldpc_timer_func, sd->bl.id, 0);
}

/* ---- Atcommands ---- */
ACMD(vip) {
	struct prem_pdata *d = ppd(sd); char buf[256];
	if (is_premium(sd)) {
		time_t t=(time_t)d->premium_tick; struct tm *ti=localtime(&t); char ts[64];
		strftime(ts,sizeof(ts),"%d/%m/%Y %H:%M",ti);
		snprintf(buf,sizeof(buf),"VIP ativo ate: %s",ts);
	} else snprintf(buf,sizeof(buf),"Voce nao e VIP.");
	clif->message(fd,buf); return true;
}

ACMD(expinfo) {
	struct prem_pdata *d = ppd(sd); char buf[256];
	int sec=(int)(time(NULL)-d->session_start); if(sec<=0) sec=1;
	snprintf(buf,sizeof(buf),"=== Sessao (%02d:%02d:%02d) ===",sec/3600,(sec%3600)/60,sec%60); clif->message(fd,buf);
	snprintf(buf,sizeof(buf),"Base: %"PRIu64" | Job: %"PRIu64,d->session_base_exp,d->session_job_exp); clif->message(fd,buf);
	snprintf(buf,sizeof(buf),"Base/h: %"PRIu64" | Job/h: %"PRIu64,d->session_base_exp*3600/(uint64)sec,d->session_job_exp*3600/(uint64)sec); clif->message(fd,buf);
	return true;
}

/* ---- Script commands ---- */
BUILDIN(isPremium) { struct map_session_data *sd=script->rid2sd(st); script_pushint(st,(sd&&is_premium(sd))?1:0); return true; }

BUILDIN(setpremium) {
	struct map_session_data *sd=script->rid2sd(st);
	if(sd) ppd(sd)->premium_tick=(int)time(NULL)+script_getnum(st,2); return true;
}

BUILDIN(addgoldpoints) {
	struct map_session_data *sd=script->rid2sd(st); struct prem_pdata *d;
	if(!sd) return true; d=ppd(sd);
	d->goldpc_points=min(d->goldpc_points+script_getnum(st,2),cfg_goldpc_maxpoints); return true;
}
BUILDIN(delgoldpoints) {
	struct map_session_data *sd=script->rid2sd(st); struct prem_pdata *d;
	if(!sd) return true; d=ppd(sd);
	d->goldpc_points=max(d->goldpc_points-script_getnum(st,2),0); return true;
}
BUILDIN(getgoldpoints) { struct map_session_data *sd=script->rid2sd(st); script_pushint(st,sd?ppd(sd)->goldpc_points:0); return true; }

BUILDIN(get_playtime) {
	struct map_session_data *sd=script->rid2sd(st);
	script_pushint(st,sd?(int)((time(NULL)-sd->status.last_login)/60):0); return true;
}

/* ---- Hooks ---- */
static int hook_pc_reg_received_post(int retVal, struct map_session_data *sd) {
	struct prem_pdata *d = ppd(sd);
	d->premium_tick  = pc->readregistry(sd, script->add_variable("#PREMIUM_TICK"));
	d->goldpc_points = pc->readregistry(sd, script->add_variable("#GOLDPCPOINTS"));
	d->session_start = (int)time(NULL);
	d->session_base_exp = 0; d->session_job_exp = 0;
	goldpc_start(sd);
	return retVal;
}

static bool hook_pc_gainexp_pre(struct map_session_data **sd, struct block_list **src,
	uint64 *base_exp, uint64 *job_exp, bool *is_quest)
{
	if (*sd && is_premium(*sd) && cfg_premium_bonusexp > 0) {
		*base_exp = *base_exp * (uint64)(100+cfg_premium_bonusexp) / 100;
		*job_exp  = *job_exp  * (uint64)(100+cfg_premium_bonusexp) / 100;
	}
	return false;
}

static bool hook_pc_gainexp_post(bool retVal, struct map_session_data *sd,
	struct block_list *src, uint64 base_exp, uint64 job_exp, bool is_quest)
{
	if (retVal && sd) { struct prem_pdata *d=ppd(sd); d->session_base_exp+=base_exp; d->session_job_exp+=job_exp; }
	return retVal;
}

/* ---- Lifecycle ---- */
HPExport void server_preinit(void) {
	int i; for(i=0;i<(int)HCFG_COUNT;i++) addBattleConf(hcfg[i].name,hcfg_parse,hcfg_return,false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE!=SERVER_TYPE_MAP) return;
	addAtcommand("vip",     vip);
	addAtcommand("expinfo", expinfo);
	addScriptCommand("isPremium",    "", isPremium);
	addScriptCommand("setpremium",   "i",setpremium);
	addScriptCommand("addgoldpoints","i",addgoldpoints);
	addScriptCommand("delgoldpoints","i",delgoldpoints);
	addScriptCommand("getgoldpoints","", getgoldpoints);
	addScriptCommand("get_playtime", "", get_playtime);
	addHookPost(pc, reg_received, hook_pc_reg_received_post);
	addHookPre(pc,  gainexp,      hook_pc_gainexp_pre);
	addHookPost(pc, gainexp,      hook_pc_gainexp_post);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus Premium Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
