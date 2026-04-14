/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2012-2025 Hercules Dev Team
 * Copyright (C) Athena Dev Teams
 *
 * Hercules is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define HERCULES_CORE

#include "vending.h"

#include "map/achievement.h"
#include "map/atcommand.h"
#include "map/battle.h"
#include "map/chrif.h"
#include "map/clif.h"
#include "map/itemdb.h"
#include "map/intif.h"
#include "map/log.h"
#include "map/map.h"
#include "map/npc.h"
#include "map/packets_struct.h"
#include "map/path.h"
#include "map/pc.h"
#include "map/skill.h"
#include "common/msgtable.h"
#include "common/nullpo.h"
#include "common/strlib.h"
#include "common/utils.h"

#include <stdio.h>
#include <string.h>

static struct vending_interface vending_s;
struct vending_interface *vending;

/// Returns an unique vending shop id.
static inline unsigned int getid(void)
{
	return vending->next_id++;
}

static int vending_resolve_currency_item(int nameid)
{
	return nameid; // Extended Vending [Lilith] - vend_loot is stored directly as nameid
}

/*==========================================
 * Close shop
 *------------------------------------------*/
static void vending_closevending(struct map_session_data *sd)
{
	nullpo_retv(sd);

	if( sd->state.vending ) {
		sd->state.vending = 0;
		clif->closevendingboard(&sd->bl, 0);
		idb_remove(vending->db, sd->status.char_id);
	}
}

/*==========================================
 * Request a shop's item list
 *------------------------------------------*/
static void vending_vendinglistreq(struct map_session_data *sd, unsigned int id)
{
	struct map_session_data* vsd;
	nullpo_retv(sd);

	if( (vsd = map->id2sd(id)) == NULL )
		return;
	if( !vsd->state.vending )
		return; // not vending

	if (!pc_can_give_items(sd) || !pc_can_give_items(vsd)) { //check if both GMs are allowed to trade
		clif->message(sd->fd, msg_sd(sd, MSGTBL_CANT_GIVE_ITEMS)); // Your GM level doesn't authorize you to perform this action.
		return;
	}

	/**
	 * Extended Vending system [Lilith]
	 **/
	if (battle_config.extended_vending && vsd->vend_loot) {
		char output[CHAT_SIZE_MAX];
		sprintf(output, msg_sd(sd, 1595), vsd->status.name, itemdb_name(vsd->vend_loot));
		if (battle_config.show_broadcast_info)
			clif->broadcast(&sd->bl, output, (int)strlen(output) + 1, BC_BLUE, SELF);
		else
			clif->messagecolor_self(sd->fd, COLOR_CYAN, output);
	}

	sd->vended_id = vsd->vender_id;  // register vending uid

	clif->vendinglist(sd, id, vsd->vending);
}

/*==========================================
 * Purchase item(s) from a shop
 *------------------------------------------*/
static void vending_purchasereq(struct map_session_data *sd, int aid, unsigned int uid, const struct CZ_PURCHASE_ITEM_FROMMC *data, int count)
{
	int i, j, cursor, w, new_ = 0, blank, vend_list[MAX_VENDING];
	int64 z;
	struct s_vending vend[MAX_VENDING]; // against duplicate packets
	struct map_session_data* vsd = map->id2sd(aid);

	nullpo_retv(sd);
	nullpo_retv(data);
	if( vsd == NULL || !vsd->state.vending || vsd->bl.id == sd->bl.id )
		return; // invalid shop

	if( vsd->vender_id != uid ) { // shop has changed
		clif->buyvending(sd, 0, 0, 6);  // store information was incorrect
		return;
	}

	if( !searchstore->queryremote(sd, aid) && ( sd->bl.m != vsd->bl.m || !check_distance_bl(&sd->bl, &vsd->bl, AREA_SIZE) ) )
		return; // shop too far away

	searchstore->clearremote(sd);

	if( count < 1 || count > MAX_VENDING || count > vsd->vend_num )
		return; // invalid amount of purchased items

	blank = pc->inventoryblank(sd); //number of free cells in the buyer's inventory

	// duplicate item in vending to check hacker with multiple packets
	memcpy(&vend, &vsd->vending, sizeof(vsd->vending)); // copy vending list

	// some checks
	z = 0; // zeny/item counter
	w = 0;  // weight counter
	for (i = 0; i < count; i++) {
		short amount = data[i].count;
		short idx    = data[i].index - 2;

		if( amount <= 0 )
			return;

		// check of item index in the cart
		if( idx < 0 || idx >= MAX_CART )
			return;

		ARR_FIND( 0, vsd->vend_num, j, vsd->vending[j].index == idx );
		if( j == vsd->vend_num )
			return; //picked non-existing item
		else
			vend_list[i] = j;

		z += (int64)vsd->vending[j].value * amount;

		/**
		 * Extended Vending system [Lilith]
		 **/
		if (battle_config.extended_vending) {
			if (vsd->vend_loot == battle_config.item_zeny || !vsd->vend_loot) {
				if (z > (int64)sd->status.zeny || z < 0 || z > (int64)MAX_ZENY) {
					return;
				}
				if (z + (int64)vsd->status.zeny > (int64)MAX_ZENY && !battle_config.vending_over_max) {
					clif->buyvending(sd, idx, vsd->vending[j].amount, 4);
					return;
				}
			} else if (vsd->vend_loot == battle_config.item_cash) {
				if (z > sd->cashPoints || z < 0 || z > (int64)MAX_ZENY) {
					clif->messagecolor_self(sd->fd, COLOR_CYAN, msg_sd(sd, 1590));
					return;
				}
			} else {
				int k, loot_count = 0, vsd_w = 0;
				if (!battle_config.ex_buying_bound) {
					for (k = 0; k < sd->status.inventorySize; k++) {
						if (sd->status.inventory[k].nameid == vsd->vend_loot) {
							if (sd->status.inventory[k].bound) {
								clif->message(sd->fd, msg_sd(sd, 1604));
								return;
							}
							loot_count += sd->status.inventory[k].amount;
						}
					}
				} else {
					for (k = 0; k < sd->status.inventorySize; k++) {
						if (sd->status.inventory[k].nameid == vsd->vend_loot)
							loot_count += sd->status.inventory[k].amount;
					}
				}
				if (z > loot_count || z < 0) {
					clif->messagecolor_self(sd->fd, COLOR_CYAN, msg_sd(sd, 1591));
					return;
				}
				if (pc->inventoryblank(vsd) <= 0) {
					clif->messagecolor_self(sd->fd, COLOR_CYAN, msg_sd(sd, 1592));
					return;
				}
				vsd_w += itemdb_weight(vsd->vend_loot) * (int)z;
				if (vsd_w + vsd->weight > vsd->max_weight) {
					clif->messagecolor_self(sd->fd, COLOR_CYAN, msg_sd(sd, 1593));
					return;
				}
				for (k = 0; k < vsd->status.inventorySize; k++) {
					if (vsd->status.inventory[k].nameid == vsd->vend_loot) {
						if ((vsd->status.inventory[k].amount + loot_count) > MAX_AMOUNT) {
							clif->message(sd->fd, msg_sd(sd, 1605));
							return;
						}
					}
				}
			}
		} else {
			if (z > (int64)sd->status.zeny || z < 0 || z > (int64)MAX_ZENY) {
				clif->buyvending(sd, idx, amount, 1);
				return;
			}
			if (z + (int64)vsd->status.zeny > (int64)MAX_ZENY && !battle_config.vending_over_max) {
				clif->buyvending(sd, idx, vsd->vending[j].amount, 4);
				return;
			}
		}

		w += itemdb_weight(vsd->status.cart[idx].nameid) * amount;
		if( w + sd->weight > sd->max_weight ) {
			clif->buyvending(sd, idx, amount, 2); // you can not buy, because overweight
			return;
		}

		//Check to see if cart/vend info is in sync.
		if( vend[j].amount > vsd->status.cart[idx].amount )
			vend[j].amount = vsd->status.cart[idx].amount;

		// if they try to add packets (example: get twice or more 2 apples if marchand has only 3 apples).
		// here, we check cumulative amounts
		if( vend[j].amount < amount ) {
			// send more quantity is not a hack (an other player can have buy items just before)
			clif->buyvending(sd, idx, vsd->vending[j].amount, 4); // not enough quantity
			return;
		}

		vend[j].amount -= amount;

		switch( pc->checkadditem(sd, vsd->status.cart[idx].nameid, amount) ) {
			case ADDITEM_EXIST:
				break; //We'd add this item to the existing one (in buyers inventory)
			case ADDITEM_NEW:
				new_++;
				if (new_ > blank)
					return; //Buyer has no space in his inventory
				break;
			case ADDITEM_OVERAMOUNT:
				return; //too many items
		}
	}

	/**
	 * Extended Vending system [Lilith] - Payment processing
	 **/
	if (battle_config.extended_vending) {
		if (vsd->vend_loot == battle_config.item_zeny || !vsd->vend_loot) {
			pc->payzeny(sd, (int)z, LOG_TYPE_VENDING, vsd);
			achievement->validate_zeny(sd, (int)z);
			if( battle_config.vending_tax )
				z -= apply_percentrate64(z, battle_config.vending_tax, 10000);
			pc->getzeny(vsd, (int)z, LOG_TYPE_VENDING, sd);
		} else if (vsd->vend_loot == battle_config.item_cash) {
			pc->paycash(sd, (int)z, 0);
			pc->getcash(vsd, (int)z, 0);
		} else {
			for (i = 0; i < sd->status.inventorySize; i++) {
				if (sd->status.inventory[i].nameid == vsd->vend_loot) {
					struct item it;
					memcpy(&it, &sd->status.inventory[i], sizeof(struct item));
					it.amount = (int)z;
					pc->additem(vsd, &it, (int)z, LOG_TYPE_VENDING);
					break;
				}
			}
			pc->delitem(sd, pc->search_inventory(sd, vsd->vend_loot), (int)z, 0, DELITEM_NORMAL, LOG_TYPE_VENDING);
		}
	} else {
		pc->payzeny(sd, (int)z, LOG_TYPE_VENDING, vsd);
		achievement->validate_zeny(sd, (int)z);
		if( battle_config.vending_tax )
			z -= apply_percentrate64(z, battle_config.vending_tax, 10000);
		pc->getzeny(vsd, (int)z, LOG_TYPE_VENDING, sd);
	}

	{
		int mailprofit = 0;

		for (i = 0; i < count; i++) {
			short amount = data[i].count;
			short idx    = data[i].index - 2;

			// vending item
			pc->additem(sd, &vsd->status.cart[idx], amount, LOG_TYPE_VENDING);
			vsd->vending[vend_list[i]].amount -= amount;
			clif->vendingreport(vsd, idx, amount, sd->status.char_id, (int)z);
			pc->cart_delitem(vsd, idx, amount, 0, LOG_TYPE_VENDING);

			//print buyer's name
			if( battle_config.buyer_name ) {
				char temp[256];
				if (battle_config.ex_vending_info) {
					double rev = ((double)vsd->vending[vend_list[i]].value * (double)amount);
					if (vsd->vend_loot == ITEMID_ZENY || !vsd->vend_loot)
						sprintf(temp, msg_sd(sd, 1597), sd->status.name, itemdb_name(vsd->status.cart[idx].nameid), amount, (int)(rev - rev * (battle_config.vending_tax / 10000.)), "Zeny");
					else
						sprintf(temp, msg_sd(sd, 1597), sd->status.name, itemdb_name(vsd->status.cart[idx].nameid), amount, (int)rev, itemdb_name(vsd->vend_loot));
				} else {
					sprintf(temp, msg_sd(vsd, MSGTBL_NAME_BOUGHT_ITEM), sd->status.name);
				}
				clif->message(vsd->fd, temp);
			}
			if (battle_config.ex_vending_info) {
				char temp[256];
				double frev = ((double)vsd->vending[vend_list[i]].value * (double)amount);
				sprintf(temp, msg_sd(sd, 1598), sd->status.name, (int)frev, vsd->vend_loot ? itemdb_name(vsd->vend_loot) : "Zeny");
				clif->message(vsd->fd, temp);
			}

			// Vending Report MailBox [Easycore]
			if (battle_config.ex_vending_report) {
				mailprofit += vsd->vending[vend_list[i]].value * amount;
				if (i == count - 1) {
					struct rodex_message msg = { 0 };
					char message[MAIL_BODY_LENGTH];
					memset(message, 0, sizeof(message));

					sprintf(message, msg_sd(vsd, 1599), sd->status.name);
					strcat(message, "\n");
					{
						int k;
						for (k = 0; k < count && strlen(message) < MAIL_BODY_LENGTH - 100; k++) {
							short a2 = data[k].count;
							short x2 = data[k].index - 2;
							sprintf(message + strlen(message), msg_sd(vsd, 1600), itemdb_name(vsd->status.cart[x2].nameid), a2);
							strcat(message, "\n");
						}
						if (k < count)
							strcat(message, msg_sd(vsd, 1601));
					}
					strcat(message, "\n");
					if (vsd->vend_loot == ITEMID_ZENY || !vsd->vend_loot)
						sprintf(message + strlen(message), msg_sd(vsd, 1602), GetComma((unsigned long)(mailprofit - mailprofit * (int)(battle_config.vending_tax / 10000.))));
					else
						sprintf(message + strlen(message), msg_sd(vsd, 1603), itemdb_name(vsd->vend_loot), mailprofit);

					msg.receiver_id = vsd->status.char_id;
					msg.type = MAIL_TYPE_TEXT;
					msg.send_date = (int)time(NULL);
					msg.expire_date = (int)time(NULL) + RODEX_EXPIRE;
					safestrncpy(msg.sender_name, msg_sd(vsd, 1606), NAME_LENGTH);
					safestrncpy(msg.title, msg_sd(vsd, 1607), RODEX_TITLE_LENGTH);
					safestrncpy(msg.body, message, MAIL_BODY_LENGTH);
					intif->rodex_sendmail(&msg);
				}
			}
		}
	}

	// compact the vending list
	for( i = 0, cursor = 0; i < vsd->vend_num; i++ ) {
		if( vsd->vending[i].amount == 0 )
			continue;

		if( cursor != i ) { // speedup
			vsd->vending[cursor].index = vsd->vending[i].index;
			vsd->vending[cursor].amount = vsd->vending[i].amount;
			vsd->vending[cursor].value = vsd->vending[i].value;
		}

		cursor++;
	}
	vsd->vend_num = cursor;

	//Always save BOTH: buyer and customer
	if( map->save_settings&2 ) {
		chrif->save(sd,0);
		chrif->save(vsd,0);
	}

	//check for @AUTOTRADE users [durf]
	if( vsd->state.autotrade ) {
		//see if there is anything left in the shop
		ARR_FIND( 0, vsd->vend_num, i, vsd->vending[i].amount > 0 );
		if( i == vsd->vend_num ) {
			//Close Vending (this was automatically done by the client, we have to do it manually for autovenders) [Skotlex]
			vending->close(vsd);
			map->quit(vsd); //They have no reason to stay around anymore, do they?
		} else
			pc->autotrade_update(vsd,PAUC_REFRESH);
	}
}

/*==========================================
 * Open shop
 * data := {<index>.w <amount>.w <value>.l}[count]
 *------------------------------------------*/
static void vending_openvending(struct map_session_data *sd, const char *message, const uint8 *data, int count)
{
	int i, j;
	int vending_skill_lvl;
	nullpo_retv(sd);

	if (pc_isdead(sd) || sd->state.prevend == 0 || pc_istrading_except_npc(sd)
	    || (sd->npc_id != 0 && sd->state.using_megaphone == 0)) {
		return; // Can't open vendings lying dead. || Didn't use via the skill. (wpe/hack) || Can't have 2 shops at once.
	}

	vending_skill_lvl = pc->checkskill(sd, MC_VENDING);
	// skill level and cart check
	if( !vending_skill_lvl || !pc_iscarton(sd) ) {
		clif->skill_fail(sd, MC_VENDING, USESKILL_FAIL_LEVEL, 0, 0);
		return;
	}

	// check number of items in shop
	if( count < 1 || count > MAX_VENDING || count > 2 + vending_skill_lvl ) {
		// invalid item count
		clif->skill_fail(sd, MC_VENDING, USESKILL_FAIL_LEVEL, 0, 0);
		return;
	}

	// filter out invalid items
	i = 0;
	for (j = 0; j < count; j++) {
		short index        = *(const uint16*)(data + 8*j + 0);
		short amount       = *(const uint16*)(data + 8*j + 2);
		unsigned int value = *(const uint32*)(data + 8*j + 4);

		index -= 2; // offset adjustment (client says that the first cart position is 2)

		if( index < 0 || index >= MAX_CART // invalid position
		 || pc->cartitem_amount(sd, index, amount) < 0 // invalid item or insufficient quantity
		//NOTE: official server does not do any of the following checks!
		 || !sd->status.cart[index].identify // unidentified item
		 || (sd->status.cart[index].attribute & ATTR_BROKEN) != 0 // broken item
		 || sd->status.cart[index].expire_time // It should not be in the cart but just in case
		 || (sd->status.cart[index].bound && !pc_can_give_bound_items(sd)) // can't trade bound items w/o permission
		 || !itemdb_cantrade(&sd->status.cart[index], pc_get_group_level(sd), pc_get_group_level(sd)) ) // untradeable item
			continue;

		sd->vending[i].index = index;
		sd->vending[i].amount = amount;
		sd->vending[i].value = cap_value(value, 0, (unsigned int)battle_config.vending_max_value);

		i++; // item successfully added
	}

	if( i != j )
		clif->message (sd->fd, msg_sd(sd, MSGTBL_ITEMS_REMOVED_FROM_SHOP)); //"Some of your items cannot be vended and were removed from the shop."

	if( i == 0 ) { // no valid item found
		clif->skill_fail(sd, MC_VENDING, USESKILL_FAIL_LEVEL, 0, 0); // custom reply packet
		return;
	}
	sd->state.prevend = sd->state.workinprogress = 0;
	sd->state.vending = true;
	sd->vender_id = getid();
	sd->vend_num = i;
	safestrncpy(sd->message, message, MESSAGE_SIZE);

	clif->openvending(sd,sd->bl.id,sd->vending);
	clif->showvendingboard(&sd->bl,message,0);

	idb_put(vending->db, sd->status.char_id, sd);
}


/// Checks if an item is being sold in given player's vending.
static bool vending_search(struct map_session_data *sd, int nameid)
{
	int i;

	nullpo_retr(false, sd);
	if( !sd->state.vending ) { // not vending
		return false;
	}

	ARR_FIND(0, sd->vend_num, i, sd->status.cart[sd->vending[i].index].nameid == nameid);
	if( i == sd->vend_num ) { // not found
		return false;
	}

	return true;
}


/// Searches for all items in a vending, that match given ids, price and possible cards.
/// @return Whether or not the search should be continued.
static bool vending_searchall(struct map_session_data *sd, const struct s_search_store_search *s)
{
	int i, c, slot;
	unsigned int idx, cidx;
	struct item* it;

	nullpo_retr(false, sd);
	nullpo_retr(false, s);
	if( !sd->state.vending ) // not vending
		return true;

	for( idx = 0; idx < s->item_count; idx++ ) {
		ARR_FIND(0, sd->vend_num, i, sd->status.cart[sd->vending[i].index].nameid == s->itemlist[idx]);
		if( i == sd->vend_num ) {// not found
			continue;
		}
		it = &sd->status.cart[sd->vending[i].index];

		if( s->min_price && s->min_price > sd->vending[i].value ) {// too low price
			continue;
		}

		if( s->max_price && s->max_price < sd->vending[i].value ) {// too high price
			continue;
		}

		if( s->card_count ) {// check cards
			if( itemdb_isspecial(it->card[0]) ) {// something, that is not a carded
				continue;
			}
			slot = itemdb_slot(it->nameid);

			for( c = 0; c < slot && it->card[c]; c ++ ) {
				ARR_FIND( 0, s->card_count, cidx, s->cardlist[cidx] == it->card[c] );
				if( cidx != s->card_count )
				{// found
					break;
				}
			}

			if( c == slot || !it->card[c] ) {// no card match
				continue;
			}
		}

		if (!searchstore->result(s->search_sd, sd->vender_id, sd->status.account_id, sd->message, it->nameid, sd->vending[i].amount, sd->vending[i].value, it->card, it->refine, it->grade, it->option))
		{// result set full
			return false;
		}
	}

	return true;
}

static void final(void)
{
	db_destroy(vending->db);
}

static void init(bool minimal)
{
	vending->db = idb_alloc(DB_OPT_BASE);
	vending->next_id = 0;
}

void vending_defaults(void)
{
	vending = &vending_s;

	vending->init = init;
	vending->final = final;

	vending->close = vending_closevending;
	vending->open = vending_openvending;
	vending->list = vending_vendinglistreq;
	vending->purchase = vending_purchasereq;
	vending->resolve_currency_item = vending_resolve_currency_item;
	vending->search = vending_search;
	vending->searchall = vending_searchall;
}
