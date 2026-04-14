/**
 * Harus WhoSell/WhoBuy Plugin
 * Adds @ws and @wb commands using searchstore UI.
 */
#include "common/hercules.h"
#include "common/memmgr.h"
#include "common/nullpo.h"
#include "common/showmsg.h"
#include "common/strlib.h"

#include "map/atcommand.h"
#include "map/battle.h"
#include "map/clif.h"
#include "map/itemdb.h"
#include "map/map.h"
#include "map/pc.h"
#include "map/searchstore.h"

#include "common/HPMDataCheck.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

HPExport struct hplugin_info pinfo = {
	"Harus_WhoSellWhoBuy", SERVER_TYPE_MAP, "1.0", HPM_VERSION,
};

static int cmp_price_asc(const void *a, const void *b)
{
	const struct s_search_store_info_item *ia = (const struct s_search_store_info_item *)a;
	const struct s_search_store_info_item *ib = (const struct s_search_store_info_item *)b;
	if (ia->price < ib->price)
		return -1;
	if (ia->price > ib->price)
		return 1;
	return 0;
}

static int cmp_price_desc(const void *a, const void *b)
{
	const struct s_search_store_info_item *ia = (const struct s_search_store_info_item *)a;
	const struct s_search_store_info_item *ib = (const struct s_search_store_info_item *)b;
	if (ia->price > ib->price)
		return -1;
	if (ia->price < ib->price)
		return 1;
	return 0;
}

static bool parse_item_query(const char *message, int *ids, int *ids_count)
{
	char *endptr = NULL;
	int i;
	struct item_data *matches[MAX_SEARCH];

	nullpo_retr(false, message);
	nullpo_retr(false, ids);
	nullpo_retr(false, ids_count);

	*ids_count = 0;

	/* Numeric id */
	if (*message >= '0' && *message <= '9') {
		long parsed = strtol(message, &endptr, 10);
		if (endptr != NULL && *endptr == '\0' && parsed > 0 && parsed <= INT_MAX && itemdb->exists((int)parsed)) {
			ids[0] = (int)parsed;
			*ids_count = 1;
			return true;
		}
	}

	/* Name search */
	*ids_count = itemdb->search_name_array(matches, MAX_SEARCH, message, IT_SEARCH_NAME_PARTIAL);
	if (*ids_count <= 0)
		return false;

	for (i = 0; i < *ids_count; i++)
		ids[i] = matches[i]->nameid;

	return true;
}

static bool begin_search(struct map_session_data *sd, int fd, unsigned char type)
{
	time_t querytime;

	nullpo_retr(false, sd);

	if (!battle->bc->feature_search_stores) {
		clif->message(fd, "Searchstore esta desativado no servidor.");
		return false;
	}

	time(&querytime);
	if (sd->searchstore.nextquerytime > querytime) {
		clif->search_store_info_failed(sd, SSI_FAILED_LIMIT_SEARCH_TIME);
		return false;
	}

	/* Open with one use each call. */
	if (!sd->searchstore.open)
		searchstore->open(sd, 1, EFFECTTYPE_NORMAL);
	else
		sd->searchstore.uses = 1;

	sd->searchstore.uses--;
	sd->searchstore.type = type;
	sd->searchstore.nextquerytime = querytime + battle->bc->searchstore_querydelay;

	searchstore->clear(sd);
	sd->searchstore.items = (struct s_search_store_info_item *)aMalloc(sizeof(struct s_search_store_info_item) * battle->bc->searchstore_maxresults);

	return true;
}

static void finish_search(struct map_session_data *sd, bool asc)
{
	nullpo_retv(sd);

	if (sd->searchstore.count) {
		qsort(sd->searchstore.items,
		      sd->searchstore.count,
		      sizeof(struct s_search_store_info_item),
		      asc ? cmp_price_asc : cmp_price_desc);

		sd->searchstore.items = (struct s_search_store_info_item *)aRealloc(sd->searchstore.items,
		                                                                      sizeof(struct s_search_store_info_item) * sd->searchstore.count);
		clif->search_store_info_ack(sd);
		sd->searchstore.pages++;
	} else {
		searchstore->clear(sd);
		clif->search_store_info_ack(sd);
		clif->search_store_info_failed(sd, SSI_FAILED_NOTHING_SEARCH_ITEM);
	}
}

ACMD(ws)
{
	int ids[MAX_SEARCH];
	int ids_count = 0;
	int i, j;
	int sat_num = 0;
	struct s_mapiterator *iter;
	struct map_session_data *pl_sd;

	if (!message || !*message) {
		clif->message(fd, "Uso: @ws <item id|nome>");
		return false;
	}

	if (!parse_item_query(message, ids, &ids_count)) {
		clif->message(fd, "Item nao encontrado.");
		return false;
	}

	if (!begin_search(sd, fd, SEARCHTYPE_VENDING))
		return false;

	iter = mapit_getallusers();
	for (pl_sd = BL_UCAST(BL_PC, mapit->first(iter)); mapit->exists(iter);
	     pl_sd = BL_UCAST(BL_PC, mapit->next(iter))) {
		bool matched_vendor = false;

		if (!pl_sd->state.vending || pl_sd == sd)
			continue;

		for (j = 0; j < pl_sd->vend_num; j++) {
			int idx = pl_sd->vending[j].index;
			int nameid = pl_sd->status.cart[idx].nameid;
			for (i = 0; i < ids_count; i++) {
				if (nameid != ids[i])
					continue;

				if (!searchstore->result(sd,
				                         pl_sd->vender_id,
				                         pl_sd->status.account_id,
				                         pl_sd->message,
				                         nameid,
				                         pl_sd->vending[j].amount,
				                         pl_sd->vending[j].value,
				                         pl_sd->status.cart[idx].card,
				                         pl_sd->status.cart[idx].refine,
				                         0,
				                         pl_sd->status.cart[idx].option)) {
					mapit->free(iter);
					finish_search(sd, true);
					return true;
				}

				matched_vendor = true;
				break;
			}
		}

		if (matched_vendor && pl_sd->mapindex == sd->mapindex)
			clif->viewpoint(sd, 1, 1, pl_sd->bl.x, pl_sd->bl.y, ++sat_num, 0xFFFFFF);
	}
	mapit->free(iter);

	finish_search(sd, true);
	return true;
}

ACMD(wb)
{
	int ids[MAX_SEARCH];
	int ids_count = 0;
	int i, j;
	int sat_num = 0;
	int cards[MAX_SLOTS] = { 0 };
	struct item_option options[MAX_ITEM_OPTIONS];
	struct s_mapiterator *iter;
	struct map_session_data *pl_sd;

	memset(options, 0, sizeof(options));

	if (!message || !*message) {
		clif->message(fd, "Uso: @wb <item id|nome>");
		return false;
	}

	if (!parse_item_query(message, ids, &ids_count)) {
		clif->message(fd, "Item nao encontrado.");
		return false;
	}

	if (!begin_search(sd, fd, SEARCHTYPE_BUYING_STORE))
		return false;

	iter = mapit_getallusers();
	for (pl_sd = BL_UCAST(BL_PC, mapit->first(iter)); mapit->exists(iter);
	     pl_sd = BL_UCAST(BL_PC, mapit->next(iter))) {
		bool matched_buyer = false;

		if (!pl_sd->state.buyingstore || pl_sd == sd)
			continue;

		for (j = 0; j < pl_sd->buyingstore.slots; j++) {
			int nameid = pl_sd->buyingstore.items[j].nameid;
			for (i = 0; i < ids_count; i++) {
				if (nameid != ids[i])
					continue;

				if (!searchstore->result(sd,
				                         pl_sd->buyer_id,
				                         pl_sd->status.account_id,
				                         pl_sd->message,
				                         nameid,
				                         pl_sd->buyingstore.items[j].amount,
				                         pl_sd->buyingstore.items[j].price,
				                         cards,
				                         0,
				                         0,
				                         options)) {
					mapit->free(iter);
					finish_search(sd, false);
					return true;
				}

				matched_buyer = true;
				break;
			}
		}

		if (matched_buyer && pl_sd->mapindex == sd->mapindex)
			clif->viewpoint(sd, 1, 1, pl_sd->bl.x, pl_sd->bl.y, ++sat_num, 0xFFFFFF);
	}
	mapit->free(iter);

	finish_search(sd, false);
	return true;
}

HPExport void plugin_init(void)
{
	if (SERVER_TYPE != SERVER_TYPE_MAP)
		return;

	addAtcommand("ws", ws);
	addAtcommand("wb", wb);
	ShowStatus("Harus WhoSellWhoBuy Plugin loaded.\n");
}

HPExport void plugin_final(void) {}
