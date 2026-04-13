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
#include "config/core.h"

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
#include "map/pet.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <limits.h>
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

/* ================================================================
 * getcharisdead / autolootgrp / autolootapply
 * ================================================================ */

/* Check whether a named character is currently dead.
 * getcharisdead("char_name") -> 1 if dead, 0 otherwise */
static BUILDIN(getcharisdead)
{
	const char *name = script_getstr(st, 2);
	struct map_session_data *tsd = map->nick2sd(name, false);
	if (tsd == NULL) {
		script_pushint(st, 0);
		return true;
	}
	script_pushint(st, pc_isdead(tsd) ? 1 : 0);
	return true;
}

/* ---- autolootgrp helpers ---- */
#define ALG_MAX_GROUPS  10
#define ALG_MAX_ITEMS   10

static int64 alg_int_uid(const char *varname)
{
	return reference_uid(script->add_str(varname), 0);
}

static int alg_read_int(struct map_session_data *sd, const char *varname)
{
	return pc->readregistry(sd, alg_int_uid(varname));
}

static void alg_write_int(struct map_session_data *sd, const char *varname, int val)
{
	pc->setregistry(sd, alg_int_uid(varname), val);
}

static const char *alg_read_str(struct map_session_data *sd, const char *varname)
{
	char *v = pc->readregistry_str(sd, alg_int_uid(varname));
	return v ? v : "";
}

static void alg_write_str(struct map_session_data *sd, const char *varname, const char *val)
{
	pc->setregistry_str(sd, alg_int_uid(varname), val);
}

/* Parse space-separated item list from LOOT_GP{n}$ into array.
 * Returns item count. */
static int alg_parse_items(const char *str, int *items, int maxitems)
{
	int count = 0;
	if (!str || !str[0])
		return 0;
	char buf[512];
	safestrncpy(buf, str, sizeof(buf));
	char *tok = strtok(buf, " ");
	while (tok && count < maxitems) {
		int id = atoi(tok);
		if (id > 0)
			items[count++] = id;
		tok = strtok(NULL, " ");
	}
	return count;
}

/* Serialize item array back to space-separated string. */
static void alg_build_str(const int *items, int count, char *out, int outsz)
{
	out[0] = '\0';
	for (int i = 0; i < count; i++) {
		char tmp[16];
		snprintf(tmp, sizeof(tmp), "%d", items[i]);
		if (i > 0) strncat(out, " ", outsz - strlen(out) - 1);
		strncat(out, tmp, outsz - strlen(out) - 1);
	}
}

static void alg_group_varname(char *buf, size_t sz, const char *prefix, int grp)
{
	/* grp is 1-based; char vars are LOOT_NM0$ ... LOOT_NM9$ */
	snprintf(buf, sz, "%s%d$", prefix, grp - 1);
}

/* ---- autolootgrp script commands ---- */

/* autolootgrpload() — no-op: data lives in permanent char vars */
static BUILDIN(autolootgrpload) { return true; }

/* autolootgrpsave() — no-op: writes happen immediately via setregistry */
static BUILDIN(autolootgrpsave) { return true; }

/* autolootgrpactive() -> int */
static BUILDIN(autolootgrpactive)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) { script_pushint(st, 0); return true; }
	script_pushint(st, alg_read_int(sd, "LOOT_ACT"));
	return true;
}

/* autolootgrpsetactive(grp) */
static BUILDIN(autolootgrpsetactive)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int grp = script_getnum(st, 2);
	alg_write_int(sd, "LOOT_ACT", grp);
	return true;
}

/* autolootgrpname(grp) -> string$ */
static BUILDIN(autolootgrpname)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) { script_pushconststr(st, ""); return true; }
	int grp = script_getnum(st, 2);
	if (grp < 1 || grp > ALG_MAX_GROUPS) { script_pushconststr(st, ""); return true; }
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_NM", grp);
	script_pushstrcopy(st, alg_read_str(sd, vname));
	return true;
}

/* autolootgrpsetname(grp, name$) */
static BUILDIN(autolootgrpsetname)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int grp = script_getnum(st, 2);
	const char *name = script_getstr(st, 3);
	if (grp < 1 || grp > ALG_MAX_GROUPS) return true;
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_NM", grp);
	alg_write_str(sd, vname, name);
	return true;
}

/* autolootgrpcount(grp) -> int */
static BUILDIN(autolootgrpcount)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) { script_pushint(st, 0); return true; }
	int grp = script_getnum(st, 2);
	if (grp < 1 || grp > ALG_MAX_GROUPS) { script_pushint(st, 0); return true; }
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_GP", grp);
	int items[ALG_MAX_ITEMS];
	int cnt = alg_parse_items(alg_read_str(sd, vname), items, ALG_MAX_ITEMS);
	script_pushint(st, cnt);
	return true;
}

/* autolootgrpget(grp, idx) -> int item_id */
static BUILDIN(autolootgrpget)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) { script_pushint(st, 0); return true; }
	int grp = script_getnum(st, 2);
	int idx = script_getnum(st, 3);
	if (grp < 1 || grp > ALG_MAX_GROUPS || idx < 0 || idx >= ALG_MAX_ITEMS) {
		script_pushint(st, 0); return true;
	}
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_GP", grp);
	int items[ALG_MAX_ITEMS] = {0};
	alg_parse_items(alg_read_str(sd, vname), items, ALG_MAX_ITEMS);
	script_pushint(st, items[idx]);
	return true;
}

/* autolootgrpset(grp, idx, item_id) */
static BUILDIN(autolootgrpset)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int grp    = script_getnum(st, 2);
	int idx    = script_getnum(st, 3);
	int item_id = script_getnum(st, 4);
	if (grp < 1 || grp > ALG_MAX_GROUPS || idx < 0 || idx >= ALG_MAX_ITEMS) return true;
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_GP", grp);
	int items[ALG_MAX_ITEMS] = {0};
	int cnt = alg_parse_items(alg_read_str(sd, vname), items, ALG_MAX_ITEMS);
	if (idx >= cnt) cnt = idx + 1;
	items[idx] = item_id;
	char out[256];
	alg_build_str(items, cnt, out, sizeof(out));
	alg_write_str(sd, vname, out);
	return true;
}

/* autolootgrpremove(grp, idx) */
static BUILDIN(autolootgrpremove)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int grp = script_getnum(st, 2);
	int idx = script_getnum(st, 3);
	if (grp < 1 || grp > ALG_MAX_GROUPS || idx < 0) return true;
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_GP", grp);
	int items[ALG_MAX_ITEMS] = {0};
	int cnt = alg_parse_items(alg_read_str(sd, vname), items, ALG_MAX_ITEMS);
	if (idx >= cnt) return true;
	/* Shift remaining items left */
	for (int i = idx; i < cnt - 1; i++)
		items[i] = items[i + 1];
	cnt--;
	char out[256];
	alg_build_str(items, cnt, out, sizeof(out));
	alg_write_str(sd, vname, out);
	return true;
}

/* autolootgrpclear(grp) — wipes name and all items for a group */
static BUILDIN(autolootgrpclear)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int grp = script_getnum(st, 2);
	if (grp < 1 || grp > ALG_MAX_GROUPS) return true;
	char vname[20];
	alg_group_varname(vname, sizeof(vname), "LOOT_NM", grp);
	alg_write_str(sd, vname, "");
	alg_group_varname(vname, sizeof(vname), "LOOT_GP", grp);
	alg_write_str(sd, vname, "");
	return true;
}

/* autolootapply(on, rate)
 * on   : 1 = enable autoloot, 0 = disable
 * rate : 0-100 (percentage of drop rate threshold)
 * Also loads items from the active group into autolootid. */
static BUILDIN(autolootapply)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int on   = script_getnum(st, 2);
	int rate = script_getnum(st, 3); /* 0-100 */

	/* Internal unit is 0-10000 (1/100 of a percent) */
	sd->state.autoloot = (on && rate > 0) ? (unsigned int)(rate * 100) : 0u;

	/* Reload autolootid from active group */
	memset(sd->state.autolootid, 0, sizeof(sd->state.autolootid));
	sd->state.autolooting = 0;

	if (on) {
		int active_grp = alg_read_int(sd, "LOOT_ACT"); /* 1-based, 0 = none */
		if (active_grp >= 1 && active_grp <= ALG_MAX_GROUPS) {
			char vname[20];
			alg_group_varname(vname, sizeof(vname), "LOOT_GP", active_grp);
			int items[ALG_MAX_ITEMS] = {0};
			int cnt = alg_parse_items(alg_read_str(sd, vname), items, ALG_MAX_ITEMS);
			for (int i = 0; i < cnt && i < AUTOLOOTITEM_SIZE; i++) {
				if (items[i] > 0) {
					sd->state.autolootid[i] = items[i];
					sd->state.autolooting = 1;
				}
			}
		}
	}
	return true;
}

/* ---- successenchant(slot, card_id)
 * Clears refine + all cards on the equipped item at script slot,
 * then writes card_id into card slot 3 (enchant slot).
 * slot: 1-based equip slot matching getequipid() convention */
static BUILDIN(successenchant)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int slotnum = script_getnum(st, 2);
	int card_id  = script_getnum(st, 3);
	int slot = slotnum - 1;
	if (slot < 0 || slot >= (int)ARRAYLENGTH(script->equip)) return true;
	int idx = pc->checkequip(sd, (int)script->equip[slot]);
	if (idx < 0) return true;
	/* Force unequip so bonuses are removed */
	pc->unequipitem(sd, idx, PCUNEQUIPITEM_RECALC | PCUNEQUIPITEM_FORCE);
	/* Clear refine and all card slots */
	sd->status.inventory[idx].refine = 0;
	memset(sd->status.inventory[idx].card, 0, sizeof(sd->status.inventory[idx].card));
	/* Write enchantment into card slot 3 */
	sd->status.inventory[idx].card[3] = (short)card_id;
	/* Refresh client inventory display */
	clif->inventoryList(sd);
	return true;
}

/* ---- failedenchant(slot)
 * Destroys (removes) the equipped item at script slot on enchant failure.
 * slot: 1-based equip slot matching getequipid() convention */
static BUILDIN(failedenchant)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int slotnum = script_getnum(st, 2);
	int slot = slotnum - 1;
	if (slot < 0 || slot >= (int)ARRAYLENGTH(script->equip)) return true;
	int idx = pc->checkequip(sd, (int)script->equip[slot]);
	if (idx < 0) return true;
	/* Unequip the item first so bonuses are removed */
	pc->unequipitem(sd, idx, PCUNEQUIPITEM_RECALC | PCUNEQUIPITEM_FORCE);
	/* Delete exactly one piece of the item from inventory */
	pc->delitem(sd, idx, 1, 0, DELITEM_NORMAL, LOG_TYPE_SCRIPT);
	return true;
}

/* ---- getsecurity() -> int
 * Returns 1 if item transfer is blocked for this character, 0 otherwise. */
static BUILDIN(getsecurity)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) { script_pushint(st, 0); return true; }
	script_pushint(st, alg_read_int(sd, "SECURITY_LOCK"));
	return true;
}

/* ---- setsecurity(flag)
 * Sets (1) or clears (0) the item transfer block for this character. */
static BUILDIN(setsecurity)
{
	struct map_session_data *sd = script->rid2sd(st);
	if (sd == NULL) return true;
	int val = script_getnum(st, 2);
	alg_write_int(sd, "SECURITY_LOCK", val ? 1 : 0);
	return true;
}

/* ---- Custom cashshop currency hook ----
 * Maps cashshop NPC names to the account variable used as currency.
 * When a player buys from one of these shops the plugin deducts points
 * from the matching account variable instead of #CASHPOINTS/#KAFRAPOINTS. */
static const char *custom_shop_var(const char *shopname)
{
	static const struct { const char *name; const char *var; } tbl[] = {
		{"pacotakp",    "#TRIUMPHPOINTS"},
		{"v1kp",        "#TRIUMPHPOINTS"},
		{"v1kp2",       "#TRIUMPHPOINTS"},
		{"v1kp3",       "#TRIUMPHPOINTS"},
		{"v1kp4",       "#TRIUMPHPOINTS"},
		{"v1kp5",       "#TRIUMPHPOINTS"},
		{"v1kp6",       "#TRIUMPHPOINTS"},
		{"mvpshop",     "#mvp_points"},
		{"presenceshop","#point_presence"},
		{"stuffnormal", "#TRIUMPHPOINTS"},
		{"stuffbg",     "#TRIUMPHPOINTS"},
		{NULL, NULL}
	};
	for (int i = 0; tbl[i].name; i++) {
		if (strcmp(shopname, tbl[i].name) == 0)
			return tbl[i].var;
	}
	return NULL;
}

/* Pre-hook for npc_cashshop_buy (single item). */
static int hook_cashshop_buy_pre(struct map_session_data **sd_ptr, int *nameid_ptr, int *amount_ptr, int *points_ptr)
{
	struct map_session_data *sd = *sd_ptr;
	if (!sd) return 0;
	struct npc_data *nd = map->id2nd(sd->npc_shopid);
	if (!nd || nd->subtype != CASHSHOP) return 0;
	const char *varname = custom_shop_var(nd->exname);
	if (!varname) return 0;

	int nameid = *nameid_ptr;
	int amount  = *amount_ptr;
	struct npc_item_list *shop = nd->u.shop.shop_item;
	unsigned short shop_size   = nd->u.shop.count;
	int i;
	ARR_FIND(0, shop_size, i, shop[i].nameid == nameid);
	if (i == shop_size || shop[i].value <= 0) {
		hookStop(); return ERROR_TYPE_ITEM_ID;
	}
	if ((long long)shop[i].value * amount > INT_MAX) {
		hookStop(); return ERROR_TYPE_ITEM_ID;
	}
	int price = shop[i].value * amount;
	int64 uid = reference_uid(script->add_str(varname), 0);
	int balance = pc->readregistry(sd, uid);
	if (balance < price) {
		hookStop(); return ERROR_TYPE_MONEY;
	}
	struct item_data *id = itemdb->exists(nameid);
	if (!id) { hookStop(); return ERROR_TYPE_ITEM_ID; }
	switch (pc->checkadditem(sd, nameid, amount)) {
		case ADDITEM_NEW:
			if (pc->inventoryblank(sd) == 0) {
				hookStop(); return ERROR_TYPE_INVENTORY_WEIGHT;
			}
			break;
		case ADDITEM_OVERAMOUNT:
			hookStop(); return ERROR_TYPE_INVENTORY_WEIGHT;
	}
	if ((long long)id->weight * amount + sd->weight > sd->max_weight) {
		hookStop(); return ERROR_TYPE_INVENTORY_WEIGHT;
	}
	/* Deduct from custom variable */
	pc->setregistry(sd, uid, balance - price);
	/* Give item */
	if (!pet->create_egg(sd, nameid)) {
		struct item item_tmp;
		memset(&item_tmp, 0, sizeof(item_tmp));
		item_tmp.nameid    = nameid;
		item_tmp.identify  = 1;
		pc->additem(sd, &item_tmp, amount, LOG_TYPE_NPC);
	}
	hookStop();
	return ERROR_TYPE_NONE;
}

/* Pre-hook for npc_cashshop_buylist (multiple items). */
static int hook_cashshop_buylist_pre(struct map_session_data **sd_ptr, int *points_ptr, struct itemlist **item_list_ptr)
{
	struct map_session_data *sd = *sd_ptr;
	if (!sd) return 0;
	struct npc_data *nd = map->id2nd(sd->npc_shopid);
	if (!nd || nd->subtype != CASHSHOP) return 0;
	const char *varname = custom_shop_var(nd->exname);
	if (!varname) return 0;

	struct itemlist *item_list = *item_list_ptr;
	struct npc_item_list *shop = nd->u.shop.shop_item;
	unsigned short shop_size   = nd->u.shop.count;

	/* Validate and calculate total price */
	int total = 0, new_ = 0;
	long long w = 0;
	for (int li = 0; li < VECTOR_LENGTH(*item_list); li++) {
		struct itemlist_entry *entry = &VECTOR_INDEX(*item_list, li);
		int ji;
		ARR_FIND(0, shop_size, ji, shop[ji].nameid == entry->id);
		if (ji == shop_size || shop[ji].value <= 0) {
			hookStop(); return ERROR_TYPE_ITEM_ID;
		}
		total += shop[ji].value * entry->amount;
		switch (pc->checkadditem(sd, entry->id, entry->amount)) {
			case ADDITEM_NEW: new_++; break;
			case ADDITEM_OVERAMOUNT:
				hookStop(); return ERROR_TYPE_INVENTORY_WEIGHT;
		}
		struct item_data *id = itemdb->exists(entry->id);
		if (id) w += (long long)id->weight * entry->amount;
	}
	if (w + sd->weight > sd->max_weight || pc->inventoryblank(sd) < new_) {
		hookStop(); return ERROR_TYPE_INVENTORY_WEIGHT;
	}
	int64 uid    = reference_uid(script->add_str(varname), 0);
	int balance  = pc->readregistry(sd, uid);
	if (balance < total) {
		hookStop(); return ERROR_TYPE_MONEY;
	}
	/* Deduct */
	pc->setregistry(sd, uid, balance - total);
	/* Give items */
	for (int li = 0; li < VECTOR_LENGTH(*item_list); li++) {
		struct itemlist_entry *entry = &VECTOR_INDEX(*item_list, li);
		if (!pet->create_egg(sd, entry->id)) {
			struct item item_tmp;
			memset(&item_tmp, 0, sizeof(item_tmp));
			item_tmp.nameid   = entry->id;
			item_tmp.identify = 1;
			pc->additem(sd, &item_tmp, entry->amount, LOG_TYPE_NPC);
		}
	}
	hookStop();
	return ERROR_TYPE_NONE;
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
	addScriptCommand("getcharisdead",    "s",   getcharisdead);
	addScriptCommand("autolootgrpload",  "",    autolootgrpload);
	addScriptCommand("autolootgrpsave",  "",    autolootgrpsave);
	addScriptCommand("autolootgrpactive","",    autolootgrpactive);
	addScriptCommand("autolootgrpsetactive","i",autolootgrpsetactive);
	addScriptCommand("autolootgrpname",  "i",   autolootgrpname);
	addScriptCommand("autolootgrpsetname","is", autolootgrpsetname);
	addScriptCommand("autolootgrpcount", "i",   autolootgrpcount);
	addScriptCommand("autolootgrpget",   "ii",  autolootgrpget);
	addScriptCommand("autolootgrpset",   "iii", autolootgrpset);
	addScriptCommand("autolootgrpremove","ii",  autolootgrpremove);
	addScriptCommand("autolootgrpclear", "i",   autolootgrpclear);
	addScriptCommand("autolootapply",    "ii",  autolootapply);
	addScriptCommand("successenchant",   "ii",  successenchant);
	addScriptCommand("failedenchant",    "i",   failedenchant);
	addScriptCommand("getsecurity",      "",    getsecurity);
	addScriptCommand("setsecurity",      "i",   setsecurity);
	/* Hooks */
	addHookPost(mob, dead,                  hook_mob_dead_post);
	addHookPost(pc,  useitem,               hook_pc_useitem_post);
	addHookPre(npc,  parse_unknown_mapflag, hook_parse_unknown_mapflag_pre);
	addHookPre(npc,  cashshop_buy,          hook_cashshop_buy_pre);
	addHookPre(npc,  cashshop_buylist,      hook_cashshop_buylist_pre);
	battle->config_read("conf/import/harus_battle.conf", true);
	ShowStatus("Harus Misc Plugin loaded.\n");
}

HPExport void plugin_final(void) { }
