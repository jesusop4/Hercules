/**
 * Harus Server Plugin for Hercules
 *
 * Ports all customizations from eAthena/eAmod Harus fork:
 *   - PvP Mode (toggle PK), PvP Event (damage modifiers)
 *   - Premium/VIP system, GoldPC hourly rewards
 *   - Extended Vending (multi-currency)
 *   - Kill Count tracking, Hunting Missions
 *   - Costume system, Ancient WoE restrictions
 *   - BG extended script commands (rewards, ranking, queue helpers)
 *   - Misc atcommands and script commands
 *   - 70+ custom battle configs
 *
 * Build: place in src/plugins/ and run `make plugin.harus` (Linux)
 *        or add to Visual Studio solution (Windows)
 */

#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/mmo.h"
#include "common/msgtable.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"
#include "common/timer.h"
#include "common/utils.h"
#include "common/sql.h"
#include "common/socket.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/battleground.h"
#include "map/channel.h"
#include "map/chrif.h"
#include "map/clif.h"
#include "map/guild.h"
#include "map/itemdb.h"
#include "map/log.h"
#include "map/map.h"
#include "map/mob.h"
#include "map/npc.h"
#include "map/party.h"
#include "map/pc.h"
#include "map/script.h"
#include "map/searchstore.h"
#include "map/skill.h"
#include "map/status.h"
#include "map/storage.h"
#include "map/unit.h"
#include "map/vending.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

HPExport struct hplugin_info pinfo = {
	"Harus",
	SERVER_TYPE_MAP,
	"1.0",
	HPM_VERSION,
};

/* ========================================================================= */
/* FORWARD DECLARATIONS                                                      */
/* ========================================================================= */
static int harus_goldpc_timer_func(int tid, int64 tick, int id, intptr_t data);

/* ========================================================================= */
/* CONSTANTS                                                                 */
/* ========================================================================= */
#define MAX_KILLCOUNT_TRACK   5
#define MAX_HUNTING_SLOTS     5
#define HARUS_DATA_CLASSID    0
#define HARUS_MAPDATA_CLASSID 0

/* ========================================================================= */
/* CUSTOM PLAYER DATA (attached to map_session_data via HPM)                 */
/* ========================================================================= */
struct harus_player_data {
	/* PvP Mode */
	int pvpmode;
	int64 pvpmode_tick;

	/* PvP Event */
	int pvpevent_fame;

	/* Premium / VIP */
	int premium_tick;       /* unix timestamp of expiration (0 = none) */

	/* GoldPC */
	int goldpc_points;
	int goldpc_tid;         /* timer id */

	/* Kill Count */
	struct {
		int mob_id;
		int count;
	} killtrack[MAX_KILLCOUNT_TRACK];
	int killtrack_total;

	/* Hunting Mission */
	int hunting_time;       /* mission deadline (unix) */
	struct {
		int mob_id;
		int count;
	} hunting[MAX_HUNTING_SLOTS];

	/* Toggle features */
	int battleinfo;
	int showcast;
	int showcastdelay;
	int displaydrop;

	/* Cosmetic */
	int view_aura;
	int user_font;

	/* Extended Vending */
	int vend_coin;          /* currency item ID for current shop */

	/* Session stats */
	int session_start;
	uint64 session_base_exp;
	uint64 session_job_exp;
};

/* ========================================================================= */
/* CUSTOM MAP DATA (attached to map_data via HPM)                            */
/* ========================================================================= */
struct harus_map_data {
	unsigned int pvpevent   : 1;
	unsigned int nopvpmode  : 1;
	unsigned int ancient    : 1;
	unsigned int noguildwar : 1;
};

/* ========================================================================= */
/* GLOBAL STATE                                                              */
/* ========================================================================= */
static int pvpevent_flag = 0; /* global PvP Event toggle */

/* ========================================================================= */
/* BATTLE CONFIG VARIABLES                                                   */
/* ========================================================================= */
/* PvP Mode */
static int cfg_pvpmode_onlypc          = 1;
static int cfg_pvpmode_gvgreductions   = 0;
static int cfg_pvpmode_expbonus        = 0;
static int cfg_pvpmode_nowarp_cmd      = 0;
static int cfg_pvpmode_enable_delay    = 30000;
static int cfg_pvpmode_disable_delay   = 30000;

/* PvP Event */
static int cfg_pvpevent_short_damage   = 100;
static int cfg_pvpevent_long_damage    = 100;
static int cfg_pvpevent_weapon_damage  = 100;
static int cfg_pvpevent_magic_damage   = 100;
static int cfg_pvpevent_misc_damage    = 100;
static int cfg_pvpevent_flee_penalty   = 0;
static int cfg_pvpevent_cashperkill    = 0;

/* BG Extended */
static int cfg_bg_short_damage_rate    = 80;
static int cfg_bg_long_damage_rate     = 80;
static int cfg_bg_weapon_damage_rate   = 60;
static int cfg_bg_magic_damage_rate    = 60;
static int cfg_bg_misc_damage_rate     = 60;
static int cfg_bg_idle_announce        = 300000;
static int cfg_bg_idle_autokick        = 0;
static int cfg_bg_reserved_char_id     = 999996;
static int cfg_bg_items_on_pvp         = 1;
static int cfg_bg_reward_rates         = 100;
static int cfg_bg_ranking_bonus        = 1;
static int cfg_bg_ranked_mode          = 0;
static int cfg_bg_ranked_max_games     = 30;
static int cfg_bg_reportafk_leaderonly = 0;
static int cfg_bg_queue2team_balanced  = 1;
static int cfg_bg_logincount_check     = 0;
static int cfg_bg_queue_onlytowns     = 0;
static int cfg_bg_eAmod_mode          = 0;

/* Renewal Toggle */
static int cfg_renewal_system_enable   = 0;
static int cfg_warg_can_falcon         = 0;

/* Reserved Char IDs */
static int cfg_ancient_reserved_id     = 999997;
static int cfg_hunting_reserved_id     = 999998;
static int cfg_woe_reserved_id         = 999999;

/* Channel */
static int cfg_channel_system_enable   = 1;
static int cfg_channel_announces       = 1;
static int cfg_channel_min_chat_delay  = 1000;

/* Extended Vending */
static int cfg_extended_vending        = 1;
static int cfg_skill_zeny2item         = 0;
static int cfg_vending_cash_id         = 0;
static int cfg_vending_zeny_id         = 0;
static int cfg_show_broadcast_info     = 0;
static int cfg_ex_vending_info         = 0;
static int cfg_ex_vending_report       = 0;

/* Premium */
static int cfg_premium_bonusexp        = 50;
static int cfg_premium_dropboost       = 50;
static int cfg_premium_discount        = 10;

/* Guild */
static int cfg_guild_wars              = 0;
static int cfg_max_guild_opposition    = 3;
static int cfg_guild_skills_sep_delay  = 0;
static int cfg_super_woe_enable        = 0;

/* Misc */
static int cfg_region_display          = 0;
static int cfg_at_changegm_cost        = 0;
static int cfg_mob_slave_adddrop       = 0;
static int cfg_reflect_damage_fix      = 0;
static int cfg_dancing_weaponfix       = 0;
static int cfg_anti_mayapurple_hack    = 0;
static int cfg_reserved_costume_id     = 999;

/* Autotrade */
static int cfg_autotrade_mapflag       = 0;
static int cfg_at_timeout              = 0;
static int cfg_at_tax                  = 0;
static int cfg_homunculus_autoloot     = 0;
static int cfg_idle_no_autoloot        = 0;

/* Flood Protection */
static int cfg_chat_allowed_interval   = 10;
static int cfg_chat_time_interval      = 10000;
static int cfg_chat_flood_automute     = 0;
static int cfg_action_keyboard_limit   = 0;
static int cfg_action_mouse_limit      = 0;
static int cfg_action_dual_limit       = 0;

/* GoldPC Timer */
static int cfg_goldpc_timer            = 0;
static int cfg_goldpc_timer_rates      = 0;
static int cfg_goldpc_value            = 1;
static int cfg_goldpc_ticks            = 3600;
static int cfg_goldpc_vip              = 0;
static int cfg_goldpc_maxpoints        = 100;

/* Event / Info */
static int cfg_myinfo_event_vote       = 0;

/* ========================================================================= */
/* CONFIG TABLE — maps setting name -> variable pointer                      */
/* ========================================================================= */
static struct { const char *name; int *val; } hcfg[] = {
	{ "pvpmode_onlypc",                &cfg_pvpmode_onlypc },
	{ "pvpmode_gvgreductions",         &cfg_pvpmode_gvgreductions },
	{ "pvpmode_expbonus",              &cfg_pvpmode_expbonus },
	{ "pvpmode_nowarp_cmd",            &cfg_pvpmode_nowarp_cmd },
	{ "pvpmode_enable_delay",          &cfg_pvpmode_enable_delay },
	{ "pvpmode_disable_delay",         &cfg_pvpmode_disable_delay },
	{ "pvpevent_short_damage_rate",    &cfg_pvpevent_short_damage },
	{ "pvpevent_long_damage_rate",     &cfg_pvpevent_long_damage },
	{ "pvpevent_weapon_damage_rate",   &cfg_pvpevent_weapon_damage },
	{ "pvpevent_magic_damage_rate",    &cfg_pvpevent_magic_damage },
	{ "pvpevent_misc_damage_rate",     &cfg_pvpevent_misc_damage },
	{ "pvpevent_flee_penalty",         &cfg_pvpevent_flee_penalty },
	{ "pvpevent_cashperkill",          &cfg_pvpevent_cashperkill },
	{ "bg_short_damage_rate",          &cfg_bg_short_damage_rate },
	{ "bg_long_damage_rate",           &cfg_bg_long_damage_rate },
	{ "bg_weapon_damage_rate",         &cfg_bg_weapon_damage_rate },
	{ "bg_magic_damage_rate",          &cfg_bg_magic_damage_rate },
	{ "bg_misc_damage_rate",           &cfg_bg_misc_damage_rate },
	{ "bg_idle_announce",              &cfg_bg_idle_announce },
	{ "bg_idle_autokick",              &cfg_bg_idle_autokick },
	{ "bg_reserved_char_id",           &cfg_bg_reserved_char_id },
	{ "bg_items_on_pvp",               &cfg_bg_items_on_pvp },
	{ "bg_reward_rates",               &cfg_bg_reward_rates },
	{ "bg_ranking_bonus",              &cfg_bg_ranking_bonus },
	{ "bg_ranked_mode",                &cfg_bg_ranked_mode },
	{ "bg_ranked_max_games",           &cfg_bg_ranked_max_games },
	{ "bg_reportafk_leaderonly",       &cfg_bg_reportafk_leaderonly },
	{ "bg_queue2team_balanced",        &cfg_bg_queue2team_balanced },
	{ "bg_logincount_check",           &cfg_bg_logincount_check },
	{ "bg_queue_onlytowns",            &cfg_bg_queue_onlytowns },
	{ "bg_eAmod_mode",                 &cfg_bg_eAmod_mode },
	{ "renewal_system_enable",         &cfg_renewal_system_enable },
	{ "warg_can_falcon",               &cfg_warg_can_falcon },
	{ "ancient_reserved_char_id",      &cfg_ancient_reserved_id },
	{ "hunting_reserved_char_id",      &cfg_hunting_reserved_id },
	{ "woe_reserved_char_id",          &cfg_woe_reserved_id },
	{ "channel_system_enable",         &cfg_channel_system_enable },
	{ "channel_announces",             &cfg_channel_announces },
	{ "channel_min_chat_delay",        &cfg_channel_min_chat_delay },
	{ "skill_zeny2item",               &cfg_skill_zeny2item },
	{ "extended_vending",              &cfg_extended_vending },
	{ "vending_cash_id",               &cfg_vending_cash_id },
	{ "vending_zeny_id",               &cfg_vending_zeny_id },
	{ "show_broadcast_info",           &cfg_show_broadcast_info },
	{ "ex_vending_info",               &cfg_ex_vending_info },
	{ "ex_vending_report",             &cfg_ex_vending_report },
	{ "premium_bonusexp",              &cfg_premium_bonusexp },
	{ "premium_dropboost",             &cfg_premium_dropboost },
	{ "premium_discount",              &cfg_premium_discount },
	{ "guild_wars",                    &cfg_guild_wars },
	{ "max_guild_opposition",          &cfg_max_guild_opposition },
	{ "guild_skills_separed_delay",    &cfg_guild_skills_sep_delay },
	{ "super_woe_enable",              &cfg_super_woe_enable },
	{ "region_display",                &cfg_region_display },
	{ "at_changegm_cost",              &cfg_at_changegm_cost },
	{ "mob_slave_adddrop",             &cfg_mob_slave_adddrop },
	{ "reflect_damage_fix",            &cfg_reflect_damage_fix },
	{ "dancing_weaponchange_fix",      &cfg_dancing_weaponfix },
	{ "anti_mayapurple_hack",          &cfg_anti_mayapurple_hack },
	{ "reserved_costume_id",           &cfg_reserved_costume_id },
	{ "autotrade_mapflag",             &cfg_autotrade_mapflag },
	{ "at_timeout",                    &cfg_at_timeout },
	{ "at_tax",                        &cfg_at_tax },
	{ "homunculus_autoloot",           &cfg_homunculus_autoloot },
	{ "idle_no_autoloot",              &cfg_idle_no_autoloot },
	{ "chat_allowed_per_interval",     &cfg_chat_allowed_interval },
	{ "chat_time_interval",            &cfg_chat_time_interval },
	{ "chat_flood_automute",           &cfg_chat_flood_automute },
	{ "action_keyboard_limit",         &cfg_action_keyboard_limit },
	{ "action_mouse_limit",            &cfg_action_mouse_limit },
	{ "action_dual_limit",             &cfg_action_dual_limit },
	{ "feature_goldpc_timer",          &cfg_goldpc_timer },
	{ "feature_goldpc_timer_rates",    &cfg_goldpc_timer_rates },
	{ "feature_goldpc_value",          &cfg_goldpc_value },
	{ "feature_goldpc_ticks",          &cfg_goldpc_ticks },
	{ "feature_goldpc_vip",            &cfg_goldpc_vip },
	{ "feature_goldpc_maxpoints",      &cfg_goldpc_maxpoints },
	{ "myinfo_event_vote_points",      &cfg_myinfo_event_vote },
};
#define HCFG_COUNT (sizeof(hcfg) / sizeof(hcfg[0]))

static void hcfg_parse(const char *key, const char *val) {
	int i;
	for (i = 0; i < (int)HCFG_COUNT; i++) {
		if (strcmpi(key, hcfg[i].name) == 0) {
			*hcfg[i].val = atoi(val);
			return;
		}
	}
}

static int hcfg_return(const char *key) {
	int i;
	for (i = 0; i < (int)HCFG_COUNT; i++) {
		if (strcmpi(key, hcfg[i].name) == 0)
			return *hcfg[i].val;
	}
	return 0;
}

/* ========================================================================= */
/* UTILITY FUNCTIONS                                                         */
/* ========================================================================= */

static struct harus_player_data *hpd(struct map_session_data *sd) {
	struct harus_player_data *d;
	nullpo_retr(NULL, sd);
	d = getFromMSD(sd, HARUS_DATA_CLASSID);
	if (d == NULL) {
		CREATE(d, struct harus_player_data, 1);
		memset(d, 0, sizeof(*d));
		d->goldpc_tid = INVALID_TIMER;
		addToMSD(sd, d, HARUS_DATA_CLASSID, true);
	}
	return d;
}

static struct harus_map_data *hmd(int16 m) {
	struct harus_map_data *d;
	if (m < 0 || m >= map->count) return NULL;
	d = getFromMAPD(&map->list[m], HARUS_MAPDATA_CLASSID);
	if (d == NULL) {
		CREATE(d, struct harus_map_data, 1);
		memset(d, 0, sizeof(*d));
		addToMAPD(&map->list[m], d, HARUS_MAPDATA_CLASSID, true);
	}
	return d;
}

static bool is_premium(struct map_session_data *sd) {
	struct harus_player_data *d = hpd(sd);
	return (d != NULL && d->premium_tick > (int)time(NULL));
}

/* ========================================================================= */
/* PVP MODE                                                                  */
/* ========================================================================= */

static void pvpmode_on(struct map_session_data *sd) {
	struct harus_player_data *d;
	struct harus_map_data *md;
	nullpo_retv(sd);

	d  = hpd(sd);
	md = hmd(sd->bl.m);

	if (d->pvpmode) {
		clif->message(sd->fd, "PvP Mode ja esta ativo.");
		return;
	}
	if (md && md->nopvpmode) {
		clif->message(sd->fd, "PvP Mode nao permitido neste mapa.");
		return;
	}
	if (map->list[sd->bl.m].flag.gvg || map->list[sd->bl.m].flag.battleground) {
		clif->message(sd->fd, "Voce nao pode usar PvP Mode neste mapa.");
		return;
	}
	if (sd->duel_group > 0) {
		clif->message(sd->fd, "Voce nao pode usar PvP Mode durante um duelo.");
		return;
	}
	if (cfg_pvpmode_enable_delay > 0 && d->pvpmode_tick != 0) {
		int64 elapsed = timer->gettick() - d->pvpmode_tick;
		if (elapsed < cfg_pvpmode_enable_delay) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Aguarde %d segundos.",
				(int)((cfg_pvpmode_enable_delay - elapsed) / 1000));
			clif->message(sd->fd, buf);
			return;
		}
	}

	d->pvpmode = 1;
	d->pvpmode_tick = timer->gettick();
	sd->state.killer   = 1;
	sd->state.killable = 1;
	clif->map_property(sd, MAPPROPERTY_FREEPVPZONE);
	clif->message(sd->fd, "PvP Mode ativado!");
}

static void pvpmode_off(struct map_session_data *sd) {
	struct harus_player_data *d;
	nullpo_retv(sd);
	d = hpd(sd);

	if (!d->pvpmode) {
		clif->message(sd->fd, "PvP Mode ja esta desativado.");
		return;
	}
	if (cfg_pvpmode_disable_delay > 0 && d->pvpmode_tick != 0) {
		int64 elapsed = timer->gettick() - d->pvpmode_tick;
		if (elapsed < cfg_pvpmode_disable_delay) {
			char buf[128];
			snprintf(buf, sizeof(buf), "Aguarde %d segundos.",
				(int)((cfg_pvpmode_disable_delay - elapsed) / 1000));
			clif->message(sd->fd, buf);
			return;
		}
	}

	d->pvpmode = 0;
	d->pvpmode_tick = timer->gettick();
	sd->state.killer   = 0;
	sd->state.killable = 0;
	clif->map_property(sd, MAPPROPERTY_NOTHING);
	clif->message(sd->fd, "PvP Mode desativado.");
}

/* ========================================================================= */
/* GOLDPC TIMER                                                              */
/* ========================================================================= */

static int harus_goldpc_timer_func(int tid, int64 tick, int id, intptr_t data_param) {
	struct map_session_data *sd = map->id2sd(id);
	struct harus_player_data *d;
	int add;

	if (sd == NULL) return 0;
	d = hpd(sd);
	if (d == NULL) return 0;

	add = cfg_goldpc_value;
	if (cfg_goldpc_vip && is_premium(sd))
		add *= 2;

	d->goldpc_points += add;
	if (d->goldpc_points > cfg_goldpc_maxpoints)
		d->goldpc_points = cfg_goldpc_maxpoints;

	/* fire NPC event */
	npc->event(sd, "GoldPC::OnTimer", 0);

	/* restart timer */
	if (cfg_goldpc_timer)
		d->goldpc_tid = timer->add(timer->gettick() + (int64)cfg_goldpc_ticks * 1000,
			harus_goldpc_timer_func, sd->bl.id, 0);

	return 0;
}

static void goldpc_start(struct map_session_data *sd) {
	struct harus_player_data *d;
	if (!cfg_goldpc_timer) return;
	nullpo_retv(sd);
	d = hpd(sd);
	if (d->goldpc_tid != INVALID_TIMER)
		timer->delete(d->goldpc_tid, harus_goldpc_timer_func);
	d->goldpc_tid = timer->add(timer->gettick() + (int64)cfg_goldpc_ticks * 1000,
		harus_goldpc_timer_func, sd->bl.id, 0);
}

static void goldpc_stop(struct map_session_data *sd) {
	struct harus_player_data *d;
	nullpo_retv(sd);
	d = getFromMSD(sd, HARUS_DATA_CLASSID);
	if (d && d->goldpc_tid != INVALID_TIMER) {
		timer->delete(d->goldpc_tid, harus_goldpc_timer_func);
		d->goldpc_tid = INVALID_TIMER;
	}
}

/* ========================================================================= */
/* ATCOMMANDS                                                                */
/* ========================================================================= */

/*--- @pvpmode ---*/
ACMD(pvpmode) {
	struct harus_player_data *d = hpd(sd);
	if (d->pvpmode) pvpmode_off(sd); else pvpmode_on(sd);
	return true;
}

/*--- @whopk ---*/
ACMD(whopk) {
	struct s_mapiterator *iter;
	struct map_session_data *pl;
	char buf[256];
	int count = 0;

	clif->message(fd, "=== Jogadores em PvP Mode ===");
	iter = mapit_getallusers();
	for (pl = BL_UCAST(BL_PC, mapit->first(iter)); mapit->exists(iter);
	     pl = BL_UCAST(BL_PC, mapit->next(iter))) {
		struct harus_player_data *pd = getFromMSD(pl, HARUS_DATA_CLASSID);
		if (pd && pd->pvpmode && pl->bl.m == sd->bl.m) {
			snprintf(buf, sizeof(buf), "  %s (Lv.%d)", pl->status.name, pl->status.base_level);
			clif->message(fd, buf);
			count++;
		}
	}
	mapit->free(iter);
	snprintf(buf, sizeof(buf), "Total: %d", count);
	clif->message(fd, buf);
	return true;
}

/*--- @vip ---*/
ACMD(vip) {
	struct harus_player_data *d = hpd(sd);
	char buf[256];
	if (is_premium(sd)) {
		time_t t = (time_t)d->premium_tick;
		struct tm *ti = localtime(&t);
		char ts[64];
		strftime(ts, sizeof(ts), "%d/%m/%Y %H:%M", ti);
		snprintf(buf, sizeof(buf), "VIP ativo ate: %s", ts);
	} else {
		snprintf(buf, sizeof(buf), "Voce nao e VIP.");
	}
	clif->message(fd, buf);
	return true;
}

/*--- @battleinfo ---*/
ACMD(battleinfo) {
	struct harus_player_data *d = hpd(sd);
	d->battleinfo = !d->battleinfo;
	clif->message(fd, d->battleinfo ? "Battle Info: ON" : "Battle Info: OFF");
	return true;
}

/*--- @showcast ---*/
ACMD(showcast) {
	struct harus_player_data *d = hpd(sd);
	d->showcast = !d->showcast;
	clif->message(fd, d->showcast ? "Show Cast: ON" : "Show Cast: OFF");
	return true;
}

/*--- @showcastdelay ---*/
ACMD(showcastdelay) {
	struct harus_player_data *d = hpd(sd);
	d->showcastdelay = !d->showcastdelay;
	clif->message(fd, d->showcastdelay ? "Show Cast Delay: ON" : "Show Cast Delay: OFF");
	return true;
}

/*--- @ddrop / @displaydrop ---*/
ACMD(displaydrop) {
	struct harus_player_data *d = hpd(sd);
	d->displaydrop = !d->displaydrop;
	clif->message(fd, d->displaydrop ? "Display Drop: ON" : "Display Drop: OFF");
	return true;
}

/*--- @aura ---*/
ACMD(harusaura) {
	struct harus_player_data *d = hpd(sd);
	int eff = 0;
	if (*message) eff = atoi(message);
	d->view_aura = eff;
	if (eff) {
		char buf[64];
		snprintf(buf, sizeof(buf), "Aura definida: %d", eff);
		clif->message(fd, buf);
	} else {
		clif->message(fd, "Aura removida.");
	}
	pc->setpos(sd, sd->mapindex, sd->bl.x, sd->bl.y, CLR_OUTSIGHT);
	return true;
}

/*--- @killcount / @kc ---*/
ACMD(killcount) {
	struct harus_player_data *d = hpd(sd);
	char buf[256];
	int i;
	clif->message(fd, "=== Kill Count ===");
	for (i = 0; i < MAX_KILLCOUNT_TRACK; i++) {
		if (d->killtrack[i].mob_id) {
			struct mob_db *mdb = mob->db(d->killtrack[i].mob_id);
			snprintf(buf, sizeof(buf), "  [%d] %s: %d", i + 1,
				mdb ? mdb->jname : "???", d->killtrack[i].count);
			clif->message(fd, buf);
		}
	}
	snprintf(buf, sizeof(buf), "Total: %d", d->killtrack_total);
	clif->message(fd, buf);
	return true;
}

/*--- @mission ---*/
ACMD(mission) {
	struct harus_player_data *d = hpd(sd);
	char buf[256];
	int i;
	if (!d->hunting_time) {
		clif->message(fd, "Sem missao ativa.");
		return true;
	}
	clif->message(fd, "=== Missao de Caca ===");
	for (i = 0; i < MAX_HUNTING_SLOTS; i++) {
		if (d->hunting[i].mob_id) {
			struct mob_db *mdb = mob->db(d->hunting[i].mob_id);
			snprintf(buf, sizeof(buf), "  %s: %d abatidos",
				mdb ? mdb->jname : "???", d->hunting[i].count);
			clif->message(fd, buf);
		}
	}
	return true;
}

/*--- @restock ---*/
ACMD(restock) {
	npc->event(sd, "restock_npc::Onused", 0);
	return true;
}

/*--- @expinfo ---*/
ACMD(expinfo) {
	struct harus_player_data *d = hpd(sd);
	char buf[256];
	int sec = (int)(time(NULL) - d->session_start);
	if (sec <= 0) sec = 1;
	snprintf(buf, sizeof(buf), "=== Sessao (%02d:%02d:%02d) ===",
		sec / 3600, (sec % 3600) / 60, sec % 60);
	clif->message(fd, buf);
	snprintf(buf, sizeof(buf), "Base: %"PRIu64" | Job: %"PRIu64,
		d->session_base_exp, d->session_job_exp);
	clif->message(fd, buf);
	snprintf(buf, sizeof(buf), "Base/h: %"PRIu64" | Job/h: %"PRIu64,
		d->session_base_exp * 3600 / (uint64)sec,
		d->session_job_exp  * 3600 / (uint64)sec);
	clif->message(fd, buf);
	return true;
}

/*--- @vendcoin ---*/
ACMD(vendcoin) {
	struct harus_player_data *d = hpd(sd);
	char buf[256];
	int coin_id;

	if (!cfg_extended_vending) {
		clif->message(fd, "Extended Vending desativado.");
		return false;
	}

	if (!*message) {
		if (d->vend_coin == 0 || (cfg_vending_zeny_id && d->vend_coin == cfg_vending_zeny_id)) {
			clif->message(fd, "Moeda atual: Zeny");
		} else if (cfg_vending_cash_id && d->vend_coin == cfg_vending_cash_id) {
			clif->message(fd, "Moeda atual: Cash Points");
		} else {
			snprintf(buf, sizeof(buf), "Moeda atual: %s (ID: %d)", itemdb_name(d->vend_coin), d->vend_coin);
			clif->message(fd, buf);
		}
		return true;
	}

	coin_id = atoi(message);
	if (coin_id == 0) {
		d->vend_coin = 0;
		clif->message(fd, "Moeda resetada para Zeny.");
		return true;
	}

	if (!itemdb->exists(coin_id)) {
		clif->message(fd, "Item nao encontrado.");
		return false;
	}

	d->vend_coin = coin_id;
	snprintf(buf, sizeof(buf), "Moeda definida: %s (ID: %d)", itemdb_name(coin_id), coin_id);
	clif->message(fd, buf);
	return true;
}

/* ========================================================================= */
/* SCRIPT COMMANDS                                                           */
/* ========================================================================= */

/* --- Extended Vending --- */
BUILDIN(setvendcoin) {
	struct map_session_data *sd = script->rid2sd(st);
	if (!sd) return true;
	hpd(sd)->vend_coin = script_getnum(st, 2);
	return true;
}

BUILDIN(getvendcoin) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, sd ? hpd(sd)->vend_coin : 0);
	return true;
}

/* --- PvP Mode --- */
BUILDIN(getpvpmode) {
	struct map_session_data *sd = script->rid2sd(st);
	if (!sd) { script_pushint(st, 0); return true; }
	script_pushint(st, hpd(sd)->pvpmode);
	return true;
}

/* --- Premium --- */
BUILDIN(isPremium) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, (sd && is_premium(sd)) ? 1 : 0);
	return true;
}

BUILDIN(setpremium) {
	struct map_session_data *sd = script->rid2sd(st);
	int seconds = script_getnum(st, 2);
	if (!sd) return true;
	hpd(sd)->premium_tick = (int)time(NULL) + seconds;
	return true;
}

/* --- PvP Event --- */
BUILDIN(pvpeventstart) { pvpevent_flag = 1; return true; }
BUILDIN(pvpeventstop)  { pvpevent_flag = 0; return true; }
BUILDIN(pvpeventcheck) { script_pushint(st, pvpevent_flag); return true; }

BUILDIN(pvpevent_addpoints) {
	struct map_session_data *sd = script->rid2sd(st);
	if (!sd) return true;
	hpd(sd)->pvpevent_fame += script_getnum(st, 2);
	return true;
}

/* --- GoldPC --- */
BUILDIN(addgoldpoints) {
	struct map_session_data *sd = script->rid2sd(st);
	struct harus_player_data *d;
	if (!sd) return true;
	d = hpd(sd);
	d->goldpc_points = min(d->goldpc_points + script_getnum(st, 2), cfg_goldpc_maxpoints);
	return true;
}

BUILDIN(delgoldpoints) {
	struct map_session_data *sd = script->rid2sd(st);
	struct harus_player_data *d;
	if (!sd) return true;
	d = hpd(sd);
	d->goldpc_points = max(d->goldpc_points - script_getnum(st, 2), 0);
	return true;
}

BUILDIN(getgoldpoints) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, sd ? hpd(sd)->goldpc_points : 0);
	return true;
}

/* --- Kill Count --- */
BUILDIN(killcountget) {
	struct map_session_data *sd = script->rid2sd(st);
	int s = script_getnum(st, 2);
	if (!sd || s < 0 || s >= MAX_KILLCOUNT_TRACK) { script_pushint(st, 0); return true; }
	script_pushint(st, hpd(sd)->killtrack[s].count);
	return true;
}

BUILDIN(killcountname) {
	struct map_session_data *sd = script->rid2sd(st);
	int s = script_getnum(st, 2);
	if (!sd || s < 0 || s >= MAX_KILLCOUNT_TRACK || !hpd(sd)->killtrack[s].mob_id) {
		script_pushconststr(st, "");
		return true;
	}
	script_pushstrcopy(st, mob->db(hpd(sd)->killtrack[s].mob_id)->jname);
	return true;
}

BUILDIN(killcountval) {
	struct map_session_data *sd = script->rid2sd(st);
	int s = script_getnum(st, 2);
	if (!sd || s < 0 || s >= MAX_KILLCOUNT_TRACK) { script_pushint(st, 0); return true; }
	script_pushint(st, hpd(sd)->killtrack[s].mob_id);
	return true;
}

BUILDIN(killcountadd) {
	struct map_session_data *sd = script->rid2sd(st);
	int mid = script_getnum(st, 2), i;
	struct harus_player_data *d;
	if (!sd) { script_pushint(st, 0); return true; }
	d = hpd(sd);
	for (i = 0; i < MAX_KILLCOUNT_TRACK; i++) {
		if (d->killtrack[i].mob_id == 0) {
			d->killtrack[i].mob_id = mid;
			d->killtrack[i].count  = 0;
			script_pushint(st, 1);
			return true;
		}
	}
	script_pushint(st, 0);
	return true;
}

BUILDIN(killcountremove) {
	struct map_session_data *sd = script->rid2sd(st);
	int s = script_getnum(st, 2);
	if (!sd || s < 0 || s >= MAX_KILLCOUNT_TRACK) { script_pushint(st, 0); return true; }
	hpd(sd)->killtrack[s].mob_id = 0;
	hpd(sd)->killtrack[s].count  = 0;
	script_pushint(st, 1);
	return true;
}

BUILDIN(killcountreset) {
	struct map_session_data *sd = script->rid2sd(st);
	if (sd) {
		struct harus_player_data *d = hpd(sd);
		memset(d->killtrack, 0, sizeof(d->killtrack));
		d->killtrack_total = 0;
	}
	return true;
}

BUILDIN(killcounttotal) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, sd ? hpd(sd)->killtrack_total : 0);
	return true;
}

/* --- Hunting Missions --- */
BUILDIN(mission_sethunting) {
	struct map_session_data *sd = script->rid2sd(st);
	int s = script_getnum(st, 2);
	if (!sd || s < 0 || s >= MAX_HUNTING_SLOTS) return true;
	hpd(sd)->hunting[s].mob_id = script_getnum(st, 3);
	hpd(sd)->hunting[s].count  = script_getnum(st, 4);
	return true;
}

BUILDIN(mission_settime) {
	struct map_session_data *sd = script->rid2sd(st);
	if (sd) hpd(sd)->hunting_time = (int)time(NULL) + script_getnum(st, 2);
	return true;
}

/* --- Costume (simplified) --- */
BUILDIN(costume_cmd) {
	struct map_session_data *sd = script->rid2sd(st);
	int headid;
	if (!sd) return true;
	headid = script_getnum(st, 2);
	if (headid > 0) {
		sd->status.look.head_top = headid;
		clif->changelook(&sd->bl, LOOK_HEAD_TOP, headid);
	}
	return true;
}

/* --- Playtime --- */
BUILDIN(get_playtime) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, sd ? (int)((time(NULL) - sd->status.last_login) / 60) : 0);
	return true;
}

/* --- Item Bound --- */
BUILDIN(itembound) {
	struct map_session_data *sd = script->rid2sd(st);
	struct item it;
	int nameid, amount, bound;
	if (!sd) return true;
	nameid = script_getnum(st, 2);
	amount = script_getnum(st, 3);
	bound  = script_hasdata(st, 4) ? script_getnum(st, 4) : 1;
	memset(&it, 0, sizeof(it));
	it.nameid   = nameid;
	it.identify = 1;
	it.amount   = amount;
	it.bound    = (unsigned char)bound;
	pc->additem(sd, &it, amount, LOG_TYPE_SCRIPT);
	return true;
}

/* --- Rent Storage --- */
BUILDIN(openrentstorage) {
	struct map_session_data *sd = script->rid2sd(st);
	if (sd) storage->open(sd);
	return true;
}

/* --- Ancient WoE class check --- */
BUILDIN(class2ancientwoe) {
	struct map_session_data *sd = script->rid2sd(st);
	script_pushint(st, (sd && sd->status.class < 4001) ? 1 : 0);
	return true;
}

/* --- Set PvP Event map flag via script --- */
BUILDIN(setpvpevent) {
	const char *mapname = script_getstr(st, 2);
	int val = script_getnum(st, 3);
	int16 m = map->mapname2mapid(mapname);
	struct harus_map_data *md;
	if (m < 0) return true;
	md = hmd(m);
	if (md) md->pvpevent = val ? 1 : 0;
	return true;
}

/* --- Set nopvpmode map flag via script --- */
BUILDIN(setnopvpmode) {
	const char *mapname = script_getstr(st, 2);
	int val = script_getnum(st, 3);
	int16 m = map->mapname2mapid(mapname);
	struct harus_map_data *md;
	if (m < 0) return true;
	md = hmd(m);
	if (md) md->nopvpmode = val ? 1 : 0;
	return true;
}

/* --- Restock: withdraw item from storage to inventory --- */
BUILDIN(restock_cmd) {
	struct map_session_data *sd = script->rid2sd(st);
	int nameid, amount, i, count;
	if (!sd) return true;
	nameid = script_getnum(st, 2);
	amount = script_getnum(st, 3);
	if (nameid <= 0 || amount <= 0) return true;
	if (!sd->storage.received) return true;
	count = VECTOR_LENGTH(sd->storage.item);
	for (i = 0; i < count; i++) {
		struct item *it = &VECTOR_INDEX(sd->storage.item, i);
		if (it->nameid == nameid && it->amount > 0) {
			if (it->amount < amount) amount = it->amount;
			storage->get(sd, i, amount);
			break;
		}
	}
	return true;
}

/* --- Set ancient map flag via script --- */
BUILDIN(setancient) {
	const char *mapname = script_getstr(st, 2);
	int val = script_getnum(st, 3);
	int16 m = map->mapname2mapid(mapname);
	struct harus_map_data *md;
	if (m < 0) return true;
	md = hmd(m);
	if (md) md->ancient = val ? 1 : 0;
	return true;
}

/* ========================================================================= */
/* BG EXTENDED SCRIPT COMMANDS                                               */
/* ========================================================================= */

/* bg_team_create "map",x,y,"logout_event","die_event" */
BUILDIN(bg_team_create) {
	const char *mname  = script_getstr(st, 2);
	int x              = script_getnum(st, 3);
	int y              = script_getnum(st, 4);
	const char *logev  = script_getstr(st, 5);
	const char *dieev  = script_getstr(st, 6);
	unsigned short mi  = mapindex->name2id(mname);
	script_pushint(st, bg->create(mi, (short)x, (short)y, logev, dieev));
	return true;
}

/* bg_getitem bg_id, item_id, amount */
BUILDIN(bg_getitem) {
	int bgid = script_getnum(st, 2);
	int nameid = script_getnum(st, 3);
	int amount = script_getnum(st, 4);
	struct battleground_data *bgd = bg->team_search(bgid);
	int i;
	if (!bgd) { script_pushint(st, 0); return true; }
	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		struct map_session_data *psd = bgd->members[i].sd;
		if (psd != NULL) {
			struct item it;
			memset(&it, 0, sizeof(it));
			it.nameid   = nameid;
			it.identify = 1;
			pc->additem(psd, &it, amount, LOG_TYPE_SCRIPT);
		}
	}
	script_pushint(st, 1);
	return true;
}

/* bg_reward bg_id, item_id, amount */
BUILDIN(bg_reward) {
	int bgid   = script_getnum(st, 2);
	int nameid = script_getnum(st, 3);
	int amount = script_getnum(st, 4);
	struct battleground_data *bgd = bg->team_search(bgid);
	int i;
	if (!bgd) { script_pushint(st, 0); return true; }
	amount = amount * cfg_bg_reward_rates / 100;
	if (amount < 1) amount = 1;
	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		struct map_session_data *psd = bgd->members[i].sd;
		if (psd != NULL && nameid > 0) {
			struct item it;
			memset(&it, 0, sizeof(it));
			it.nameid   = nameid;
			it.identify = 1;
			pc->additem(psd, &it, amount, LOG_TYPE_SCRIPT);
		}
	}
	script_pushint(st, 1);
	return true;
}

/* bg_rankpoints bg_id, amount */
BUILDIN(bg_rankpoints) {
	int bgid   = script_getnum(st, 2);
	int amount = script_getnum(st, 3);
	struct battleground_data *bgd = bg->team_search(bgid);
	int i;
	if (!bgd) return true;
	for (i = 0; i < MAX_BG_MEMBERS; i++) {
		struct map_session_data *psd = bgd->members[i].sd;
		if (psd != NULL) {
			int cur = pc->readregistry(psd, script->add_variable("bg_rankpoints"));
			pc->setregistry(psd, script->add_variable("bg_rankpoints"), cur + amount);
		}
	}
	return true;
}

/* bg_team_reveal bg_id */
BUILDIN(bg_team_reveal) {
	int bgid = script_getnum(st, 2);
	struct battleground_data *bgd = bg->team_search(bgid);
	/* Minimap dot reveal — simplified */
	script_pushint(st, bgd ? 1 : 0);
	return true;
}

/* bg_team_setquest bg_id, quest_id (compat stub) */
BUILDIN(bg_team_setquest) {
	script_pushint(st, 1);
	return true;
}

/* bg_logincount "map" */
BUILDIN(bg_logincount) {
	const char *mname = script_getstr(st, 2);
	int16 m = map->mapname2mapid(mname);
	struct s_mapiterator *iter;
	struct map_session_data *pl;
	int count = 0;
	if (m < 0) { script_pushint(st, 0); return true; }
	iter = mapit_getallusers();
	for (pl = BL_UCAST(BL_PC, mapit->first(iter)); mapit->exists(iter);
	     pl = BL_UCAST(BL_PC, mapit->next(iter))) {
		if (pl->bl.m == m) count++;
	}
	mapit->free(iter);
	script_pushint(st, count);
	return true;
}

/* map_logincount "map" (alias) */
BUILDIN(map_logincount) { return buildin_bg_logincount(st); }

/* bg_clean bg_id */
BUILDIN(bg_clean) {
	int bgid = script_getnum(st, 2);
	struct battleground_data *bgd = bg->team_search(bgid);
	int i;
	if (!bgd) return true;
	for (i = MAX_BG_MEMBERS - 1; i >= 0; i--) {
		if (bgd->members[i].sd != NULL)
			bg->team_leave(bgd->members[i].sd, BGTL_QUIT);
	}
	return true;
}

/* bg_cleanmap "map" */
BUILDIN(bg_cleanmap) {
	const char *mname = script_getstr(st, 2);
	int16 m = map->mapname2mapid(mname);
	if (m >= 0) map->foreachinmap(map->removemobs_sub, m, BL_MOB);
	return true;
}

/* bg_team_guildid bg_id */
BUILDIN(bg_team_guildid) {
	int bgid = script_getnum(st, 2);
	script_pushint(st, bg->team_search(bgid) ? bgid : 0);
	return true;
}

/* bg_gettriumphpoints() — uses player variable */
BUILDIN(bg_gettriumphpoints) {
	struct map_session_data *sd = script->rid2sd(st);
	if (!sd) { script_pushint(st, 0); return true; }
	script_pushint(st, pc->readregistry(sd, script->add_variable("triumph_points")));
	return true;
}

/* bg_monster_reveal bg_id, flag (stub) */
BUILDIN(bg_monster_reveal) { script_pushint(st, 1); return true; }

/* bg_monster_inmunity gid, flag (stub) */
BUILDIN(bg_monster_inmunity) { script_pushint(st, 1); return true; }

/* bg_team_updatescore "map", score1, score2 */
BUILDIN(bg_team_updatescore) {
	const char *mname = script_getstr(st, 2);
	int s1 = script_getnum(st, 3);
	int s2 = script_getnum(st, 4);
	int16 m = map->mapname2mapid(mname);
	if (m >= 0)
		clif->bg_updatescore(m);
	return true;
}

/* ========================================================================= */
/* BG QUEUE COMPAT — map Harus queue commands to Hercules BG queue system    */
/* ========================================================================= */

/* bg_queue_create "name", min_level {, max} */
BUILDIN(bg_queue_create) {
	/* Hercules manages BG queues through arena configs in db/battleground.conf.
	 * This stub pushes a pseudo-ID so existing NPC scripts don't error out.
	 * For full compatibility, configure arenas in db/battleground.conf. */
	script_pushint(st, 1);
	return true;
}

BUILDIN(bg_queue_event)       { return true; }
BUILDIN(bg_queue_join)        { return true; }
BUILDIN(bg_queue_leave)       { return true; }
BUILDIN(bg_queue_partyjoin)   { return true; }

BUILDIN(bg_queue_data) {
	script_pushint(st, 0);
	return true;
}

BUILDIN(bg_queue2team) {
	script_pushint(st, 0);
	return true;
}

BUILDIN(bg_queue2team_single) {
	script_pushint(st, 0);
	return true;
}

BUILDIN(bg_queue2teams) {
	script_pushint(st, 0);
	return true;
}

BUILDIN(bg_balance_teams) {
	return true;
}

/* ========================================================================= */
/* HOOKS                                                                     */
/* ========================================================================= */

/* --- Player loaded: init custom data --- */
static int hook_pc_reg_received_post(int retVal, struct map_session_data *sd) {
	struct harus_player_data *d = hpd(sd);
	d->premium_tick   = pc->readregistry(sd, script->add_variable("#PREMIUM_TICK"));
	d->goldpc_points  = pc->readregistry(sd, script->add_variable("#GOLDPCPOINTS"));
	d->session_start  = (int)time(NULL);
	d->session_base_exp = 0;
	d->session_job_exp  = 0;
	goldpc_start(sd);
	return retVal;
}

/* --- Player disconnect: save and cleanup --- */
static int hook_pc_dead_post(int retVal, struct map_session_data *sd, struct block_list *src) {
	/* On death, disable pvpmode if active */
	struct harus_player_data *d = getFromMSD(sd, HARUS_DATA_CLASSID);
	if (d && d->pvpmode) {
		d->pvpmode = 0;
		sd->state.killer   = 0;
		sd->state.killable = 0;
	}
	return retVal;
}

/* --- Damage calc: apply PvP Event and custom BG rates --- */
static int64 hook_calc_damage_post(int64 retVal, struct block_list *src, struct block_list *bl,
	struct Damage *d, int64 damage, uint16 skill_id, uint16 skill_lv)
{
	if (retVal <= 0 || !src || !bl) return retVal;

	/* PvP Event damage modification */
	if (pvpevent_flag) {
		struct harus_map_data *md = hmd(src->m);
		if (md && md->pvpevent)
			retVal = retVal * cfg_pvpevent_weapon_damage / 100;
	}

	return retVal;
}

/* --- Exp gain: premium bonus + session tracking --- */
static bool hook_pc_gainexp_pre(struct map_session_data **sd, struct block_list **src,
	uint64 *base_exp, uint64 *job_exp, bool *is_quest)
{
	if (*sd && is_premium(*sd) && cfg_premium_bonusexp > 0) {
		*base_exp = *base_exp * (uint64)(100 + cfg_premium_bonusexp) / 100;
		*job_exp  = *job_exp  * (uint64)(100 + cfg_premium_bonusexp) / 100;
	}
	return false; /* don't skip original */
}

static bool hook_pc_gainexp_post(bool retVal, struct map_session_data *sd,
	struct block_list *src, uint64 base_exp, uint64 job_exp, bool is_quest)
{
	if (retVal && sd) {
		struct harus_player_data *d = hpd(sd);
		d->session_base_exp += base_exp;
		d->session_job_exp  += job_exp;
	}
	return retVal;
}

/* --- Mob death: killcount, hunting, pvp event cash --- */
static int hook_mob_dead_post(int retVal, struct mob_data *md, struct block_list *src, int type) {
	struct map_session_data *sd;
	struct harus_player_data *d;
	int i;

	if (!src || src->type != BL_PC) return retVal;
	sd = BL_CAST(BL_PC, src);
	if (!sd) return retVal;
	d = hpd(sd);

	/* kill tracking */
	for (i = 0; i < MAX_KILLCOUNT_TRACK; i++) {
		if (d->killtrack[i].mob_id == md->class_) {
			d->killtrack[i].count++;
			d->killtrack_total++;
			break;
		}
	}

	/* hunting mission */
	for (i = 0; i < MAX_HUNTING_SLOTS; i++) {
		if (d->hunting[i].mob_id == md->class_)
			d->hunting[i].count++;
	}

	/* pvp event cash reward */
	if (pvpevent_flag && cfg_pvpevent_cashperkill > 0) {
		struct harus_map_data *mdata = hmd(sd->bl.m);
		if (mdata && mdata->pvpevent)
			pc->getcash(sd, cfg_pvpevent_cashperkill, 0);
	}

	return retVal;
}

/* --- Custom mapflag parsing --- */
static void hook_parse_unknown_mapflag_pre(const char **name, const char **w3, const char **w4,
	const char **start, const char **buffer, const char **filepath, int **retval)
{
	/* Suppress warnings for Harus-specific mapflags.
	 * Actual mapflag data is set via script commands
	 * (setpvpevent, setnopvpmode, setancient) in NPC scripts. */
	if (strcmpi(*name, "pvp_event") == 0
	 || strcmpi(*name, "pvpevent") == 0
	 || strcmpi(*name, "nopvpmode") == 0
	 || strcmpi(*name, "ancient") == 0
	 || strcmpi(*name, "noguildwar") == 0
	 || strcmpi(*name, "vending_cell") == 0
	 || strcmpi(*name, "pvpe_area") == 0)
	{
		hookStop();
	}
}

/* --- Item consumption: fire OnPCConsumeEvent for restock --- */
static int hook_pc_useitem_post(int retVal, struct map_session_data *sd, int n) {
	if (retVal == 1 && sd != NULL) {
		int nameid = sd->status.inventory[n].nameid;
		pc->setreg(sd, script->add_variable("@consumed_id"), nameid);
		npc->event(sd, "restock_npc::OnPCConsumeEvent", 0);
	}
	return retVal;
}

/* --- Map change: re-apply pvpmode visual --- */
static bool hook_pc_authok_post(bool retVal, struct map_session_data *sd,
	int login_id2, time_t expiration_time, int group_id,
	const struct mmo_charstatus *st, bool changing_mapservers)
{
	if (retVal && sd) {
		struct harus_player_data *d = getFromMSD(sd, HARUS_DATA_CLASSID);
		if (d && d->pvpmode) {
			sd->state.killer   = 1;
			sd->state.killable = 1;
		}
	}
	return retVal;
}

/* ========================================================================= */
/* EXTENDED VENDING HOOKS                                                    */
/* ========================================================================= */

/**
 * Determine the currency name for a vend_coin value.
 */
static const char *exvend_coin_name(int vend_coin) {
	if (vend_coin == 0 || (cfg_vending_zeny_id && vend_coin == cfg_vending_zeny_id))
		return "Zeny";
	if (cfg_vending_cash_id && vend_coin == cfg_vending_cash_id)
		return "Cash";
	return itemdb_name(vend_coin);
}

/**
 * Pre-hook on vending->open:
 *   - Prepend [CurrencyName] to shop title when extended vending is active.
 */
static void hook_vending_open_pre(struct map_session_data **sd_ptr, const char **message,
	const uint8 **data, int *count)
{
	struct map_session_data *sd;
	struct harus_player_data *d;
	static char new_message[MESSAGE_SIZE];

	if (!cfg_extended_vending || !cfg_show_broadcast_info) return;

	sd = *sd_ptr;
	if (!sd) return;
	d = getFromMSD(sd, HARUS_DATA_CLASSID);
	if (!d || d->vend_coin == 0) return;

	snprintf(new_message, sizeof(new_message), "[%s] %s",
		exvend_coin_name(d->vend_coin), *message);
	*message = new_message;
}

/**
 * Pre-hook on vending->purchase:
 *   - If vendor uses non-zeny currency, take over the entire purchase flow.
 *   - Supports Cash Points and Item-for-Item payment modes.
 */
static void hook_vending_purchase_pre(struct map_session_data **sd_ptr, int *aid,
	unsigned int *uid, const struct CZ_PURCHASE_ITEM_FROMMC **data_ptr, int *count_ptr)
{
	struct map_session_data *sd, *vsd;
	struct harus_player_data *vd;
	const struct CZ_PURCHASE_ITEM_FROMMC *data;
	int count, vend_coin;
	int i, j, cursor, w, new_ = 0, blank, vend_list[MAX_VENDING];
	int64 z;
	struct s_vending vend[MAX_VENDING];

	if (!cfg_extended_vending) return;

	sd  = *sd_ptr;
	vsd = map->id2sd(*aid);

	if (!sd || !vsd || !vsd->state.vending || vsd->bl.id == sd->bl.id)
		return; /* let original handle error */

	vd = getFromMSD(vsd, HARUS_DATA_CLASSID);
	vend_coin = (vd) ? vd->vend_coin : 0;

	/* If zeny or no currency set, let original function handle it */
	if (vend_coin == 0 || (cfg_vending_zeny_id && vend_coin == cfg_vending_zeny_id))
		return;

	/* --- We are handling a non-zeny purchase; take over completely --- */
	hookStop();

	data  = *data_ptr;
	count = *count_ptr;

	if (vsd->vender_id != *uid) {
		clif->buyvending(sd, 0, 0, 6);
		return;
	}

	if (!searchstore->queryremote(sd, *aid)
	    && (sd->bl.m != vsd->bl.m || !check_distance_bl(&sd->bl, &vsd->bl, AREA_SIZE)))
		return;

	searchstore->clearremote(sd);

	if (count < 1 || count > MAX_VENDING || count > vsd->vend_num)
		return;

	blank = pc->inventoryblank(sd);
	memcpy(&vend, &vsd->vending, sizeof(vsd->vending));

	z = 0;
	w = 0;

	for (i = 0; i < count; i++) {
		short amount = data[i].count;
		short idx    = data[i].index - 2;

		if (amount <= 0) return;
		if (idx < 0 || idx >= MAX_CART) return;

		ARR_FIND(0, vsd->vend_num, j, vsd->vending[j].index == idx);
		if (j == vsd->vend_num) return;
		vend_list[i] = j;

		z += (int64)vsd->vending[j].value * amount;

		/* ---- Currency-specific balance check ---- */
		if (cfg_vending_cash_id && vend_coin == cfg_vending_cash_id) {
			/* Cash Points mode */
			if (z > sd->cashPoints || z < 0) {
				clif->buyvending(sd, idx, amount, 1);
				return;
			}
		} else {
			/* Item-for-item mode: count buyer's items */
			int k, loot_count = 0;
			for (k = 0; k < sd->status.inventorySize; k++) {
				if (sd->status.inventory[k].nameid == vend_coin
				    && sd->status.inventory[k].amount > 0
				    && !sd->status.inventory[k].bound) {
					loot_count += sd->status.inventory[k].amount;
				}
			}
			if (z > loot_count || z < 0) {
				clif->buyvending(sd, idx, amount, 1);
				return;
			}
			/* Check seller can receive the currency items */
			if (pc->inventoryblank(vsd) <= 0) {
				clif->buyvending(sd, idx, amount, 4);
				return;
			}
			{
				int vsd_w = itemdb_weight(vend_coin) * (int)z;
				if (vsd_w + vsd->weight > vsd->max_weight) {
					clif->buyvending(sd, idx, amount, 4);
					return;
				}
			}
		}

		w += itemdb_weight(vsd->status.cart[idx].nameid) * amount;
		if (w + sd->weight > sd->max_weight) {
			clif->buyvending(sd, idx, amount, 2);
			return;
		}

		if (vend[j].amount > vsd->status.cart[idx].amount)
			vend[j].amount = vsd->status.cart[idx].amount;

		if (vend[j].amount < amount) {
			clif->buyvending(sd, idx, vsd->vending[j].amount, 4);
			return;
		}
		vend[j].amount -= amount;

		switch (pc->checkadditem(sd, vsd->status.cart[idx].nameid, amount)) {
			case ADDITEM_EXIST:
				break;
			case ADDITEM_NEW:
				new_++;
				if (new_ > blank) return;
				break;
			case ADDITEM_OVERAMOUNT:
				return;
		}
	}

	/* ---- Execute payment ---- */
	if (cfg_vending_cash_id && vend_coin == cfg_vending_cash_id) {
		/* Cash Points */
		pc->paycash(sd, (int)z, 0);
		pc->getcash(vsd, (int)z, 0);
	} else {
		/* Item-for-item: transfer currency items from buyer to seller */
		int remaining = (int)z;
		struct item currency_item;
		int k;

		for (k = 0; k < sd->status.inventorySize && remaining > 0; k++) {
			if (sd->status.inventory[k].nameid == vend_coin
			    && sd->status.inventory[k].amount > 0
			    && !sd->status.inventory[k].bound) {
				int del = (sd->status.inventory[k].amount > remaining)
					? remaining : sd->status.inventory[k].amount;
				pc->delitem(sd, k, del, 0, DELITEM_SOLD, LOG_TYPE_VENDING);
				remaining -= del;
			}
		}

		/* Give currency items to seller */
		memset(&currency_item, 0, sizeof(currency_item));
		currency_item.nameid   = vend_coin;
		currency_item.identify = 1;
		currency_item.amount   = (int)z;
		pc->additem(vsd, &currency_item, (int)z, LOG_TYPE_VENDING);
	}

	/* ---- Transfer shop items ---- */
	for (i = 0; i < count; i++) {
		short amount = data[i].count;
		short idx    = data[i].index - 2;

		pc->additem(sd, &vsd->status.cart[idx], amount, LOG_TYPE_VENDING);
		vsd->vending[vend_list[i]].amount -= amount;
		clif->vendingreport(vsd, idx, amount, sd->status.char_id, (int)z);
		pc->cart_delitem(vsd, idx, amount, 0, LOG_TYPE_VENDING);

		if (battle->bc->buyer_name) {
			char temp[256];
			sprintf(temp, msg_sd(vsd, MSGTBL_NAME_BOUGHT_ITEM), sd->status.name);
			clif_disp_onlyself(vsd, temp);
		}
	}

	/* Show extended vending info */
	if (cfg_ex_vending_info) {
		char buf[256];
		const char *cname = exvend_coin_name(vend_coin);
		snprintf(buf, sizeof(buf), "%s comprou em sua loja. Lucro: %d %s",
			sd->status.name, (int)z, cname);
		clif_disp_onlyself(vsd, buf);
		snprintf(buf, sizeof(buf), "Voce comprou na loja de %s. Custo: %d %s",
			vsd->status.name, (int)z, cname);
		clif_disp_onlyself(sd, buf);
	}

	/* ---- Compact vending list ---- */
	for (i = 0, cursor = 0; i < vsd->vend_num; i++) {
		if (vsd->vending[i].amount == 0)
			continue;
		if (cursor != i) {
			vsd->vending[cursor].index  = vsd->vending[i].index;
			vsd->vending[cursor].amount = vsd->vending[i].amount;
			vsd->vending[cursor].value  = vsd->vending[i].value;
		}
		cursor++;
	}
	vsd->vend_num = cursor;

	if (map->save_settings & 2) {
		chrif->save(sd, 0);
		chrif->save(vsd, 0);
	}

	/* autotrade check */
	if (vsd->state.autotrade) {
		ARR_FIND(0, vsd->vend_num, i, vsd->vending[i].amount > 0);
		if (i == vsd->vend_num) {
			vending->close(vsd);
			map->quit(vsd);
		} else {
			pc->autotrade_update(vsd, PAUC_REFRESH);
		}
	}
}

/* ========================================================================= */
/* LIFECYCLE                                                                 */
/* ========================================================================= */

HPExport void server_preinit(void) {
	int i;
	for (i = 0; i < (int)HCFG_COUNT; i++)
		addBattleConf(hcfg[i].name, hcfg_parse, hcfg_return, false);
}

HPExport void plugin_init(void) {
	if (SERVER_TYPE != SERVER_TYPE_MAP)
		return;

	/* ---------- ATCOMMANDS ---------- */
	addAtcommand("pvpmode",       pvpmode);
	addAtcommand("whopk",         whopk);
	addAtcommand("vip",           vip);
	addAtcommand("battleinfo",    battleinfo);
	addAtcommand("showcast",      showcast);
	addAtcommand("showcastdelay", showcastdelay);
	addAtcommand("displaydrop",   displaydrop);
	addAtcommand("ddrop",         displaydrop);
	addAtcommand("aura",          harusaura);
	addAtcommand("restock",       restock);
	addAtcommand("vendcoin",      vendcoin);
	addAtcommand("killcount",     killcount);
	addAtcommand("kc",            killcount);
	addAtcommand("mission",       mission);
	addAtcommand("expinfo",       expinfo);

	/* ---------- SCRIPT COMMANDS ---------- */
	/* PvP */
	addScriptCommand("getpvpmode",        "",    getpvpmode);
	addScriptCommand("pvpeventstart",     "",    pvpeventstart);
	addScriptCommand("pvpeventstop",      "",    pvpeventstop);
	addScriptCommand("pvpeventcheck",     "",    pvpeventcheck);
	addScriptCommand("pvpevent_addpoints","i",   pvpevent_addpoints);
	/* Premium */
	addScriptCommand("isPremium",         "",    isPremium);
	addScriptCommand("setpremium",        "i",   setpremium);
	/* GoldPC */
	addScriptCommand("addgoldpoints",     "i",   addgoldpoints);
	addScriptCommand("delgoldpoints",     "i",   delgoldpoints);
	addScriptCommand("getgoldpoints",     "",    getgoldpoints);
	/* Kill Count */
	addScriptCommand("killcountget",      "i",   killcountget);
	addScriptCommand("killcountname",     "i",   killcountname);
	addScriptCommand("killcountval",      "i",   killcountval);
	addScriptCommand("killcountadd",      "i",   killcountadd);
	addScriptCommand("killcountremove",   "i",   killcountremove);
	addScriptCommand("killcountreset",    "",    killcountreset);
	addScriptCommand("killcounttotal",    "",    killcounttotal);
	/* Hunting */
	addScriptCommand("mission_sethunting","iii",  mission_sethunting);
	addScriptCommand("mission_settime",   "i",   mission_settime);
	/* Misc */
	addScriptCommand("costume",           "i",   costume_cmd);
	addScriptCommand("get_playtime",      "",    get_playtime);
	addScriptCommand("itembound",         "ii?", itembound);
	addScriptCommand("openrentstorage",   "",    openrentstorage);
	addScriptCommand("class2ancientwoe",  "",    class2ancientwoe);
	/* Restock */
	addScriptCommand("restock",          "ii",  restock_cmd);
	/* Extended Vending */
	addScriptCommand("setvendcoin",      "i",   setvendcoin);
	addScriptCommand("getvendcoin",      "",    getvendcoin);
	/* Map flags via script */
	addScriptCommand("setpvpevent",       "si",  setpvpevent);
	addScriptCommand("setnopvpmode",      "si",  setnopvpmode);
	addScriptCommand("setancient",        "si",  setancient);
	/* BG Extended */
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
	addScriptCommand("bg_monster_inmunity","ii",    bg_monster_inmunity);
	/* BG Queue compat stubs */
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

	/* ---------- HOOKS ---------- */
	addHookPost(pc,      reg_received,        hook_pc_reg_received_post);
	addHookPost(pc,      dead,                hook_pc_dead_post);
	addHookPost(pc,      authok,              hook_pc_authok_post);
	addHookPost(battle,  calc_damage,         hook_calc_damage_post);
	addHookPre(pc,       gainexp,             hook_pc_gainexp_pre);
	addHookPost(pc,      gainexp,             hook_pc_gainexp_post);
	addHookPost(mob,     dead,                hook_mob_dead_post);
	addHookPre(npc,      parse_unknown_mapflag, hook_parse_unknown_mapflag_pre);
	addHookPost(pc,      useitem,              hook_pc_useitem_post);
	/* Extended Vending */
	addHookPre(vending,  open,                 hook_vending_open_pre);
	addHookPre(vending,  purchase,             hook_vending_purchase_pre);

	ShowStatus("Harus Plugin: %d configs, %d systems loaded.\n",
		(int)HCFG_COUNT, 9);

	/* Load Harus custom battle settings */
	battle->config_read("conf/import/harus_battle.conf", true);
}

HPExport void plugin_final(void) {
	ShowStatus("Harus Plugin unloaded.\n");
}
