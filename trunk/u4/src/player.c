/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "player.h"

#include "armor.h"
#include "context.h"
#include "debug.h"
#include "game.h"
//#include "u4.h"
#include "utils.h"
#include "weapon.h"

LostEighthCallback lostEighthCallback = NULL;
AdvanceLevelCallback advanceLevelCallback = NULL;
ItemStatsChangedCallback itemStatsChangedCallback = NULL;
PartyStarvingCallback partyStarvingCallback = NULL;
SetTransportCallback setTransportCallback = NULL;

/**
 * Sets up a callback to handle player losing an eighth of his or her
 * avatarhood.
 */
void playerSetLostEighthCallback(LostEighthCallback callback) {
    lostEighthCallback = callback;
}

void playerSetAdvanceLevelCallback(AdvanceLevelCallback callback) {
    advanceLevelCallback = callback;
}

void playerSetItemStatsChangedCallback(ItemStatsChangedCallback callback) {
    itemStatsChangedCallback = callback;
}

void playerSetPartyStarvingCallback(PartyStarvingCallback callback) {
    partyStarvingCallback = callback;
}

void playerSetSetTransportCallback(SetTransportCallback callback) {
    setTransportCallback = callback;
}

/**
 * Applies damage to a player, and changes status to dead if hit
 * points drop to zero or below.
 */
void playerApplyDamage(SaveGamePlayerRecord *player, int damage) {
    int newHp = player->hp;

    if (player->status == STAT_DEAD)
        return;

    newHp -= damage;

    if (newHp < 0) {
        player->status = STAT_DEAD;
        newHp = 0;
    }
    player->hp = newHp;
}

/**
 * Determine what level a character has.
 */
int playerGetRealLevel(const SaveGamePlayerRecord *player) {
    return player->hpMax / 100;
}

/**
 * Determine the highest level a character could have with the number
 * of experience points he has.
 */
int playerGetMaxLevel(const SaveGamePlayerRecord *player) {
    int level = 1;
    int next = 100;

    while (player->xp >= next && level < 8) {
        level++;
        next *= 2;
    }

    return level;
}

void playerAdvanceLevel(SaveGamePlayerRecord *player) {
    if (playerGetRealLevel(player) == playerGetMaxLevel(player))
        return;
    player->status = STAT_GOOD;
    player->hpMax = playerGetMaxLevel(player) * 100;
    player->hp = player->hpMax;

    /* improve stats by 1-8 each */
    player->str   += (rand() % 8) + 1;
    player->dex   += (rand() % 8) + 1;
    player->intel += (rand() % 8) + 1;

    if (player->str > 50) player->str = 50;
    if (player->dex > 50) player->dex = 50;
    if (player->intel > 50) player->intel = 50;

    if (advanceLevelCallback)
        (*advanceLevelCallback)(player);
}

/**
 * Award a player experience points.  Maxs out the players xp at 9999.
 */
void playerAwardXp(SaveGamePlayerRecord *player, int xp) {
    player->xp += xp;
    if (player->xp > 9999)
        player->xp = 9999;
}

/**
 * Determine the most magic points a character could given his class
 * and intelligence.
 */
int playerGetMaxMp(const SaveGamePlayerRecord *player) {
    int max_mp = -1;

    switch (player->klass) {
    case CLASS_MAGE:            /*  mage: 200% of int */
        max_mp = player->intel * 2;
        break;

    case CLASS_DRUID:           /* druid: 150% of int */
        max_mp = player->intel * 3 / 2;
        break;

    case CLASS_BARD:            /* bard, paladin, ranger: 100% of int */
    case CLASS_PALADIN:
    case CLASS_RANGER:
        max_mp = player->intel;
        break;

    case CLASS_TINKER:          /* tinker: 50% of int */
        max_mp = player->intel / 2;
        break;

    case CLASS_FIGHTER:         /* fighter, shepherd: no mp at all */
    case CLASS_SHEPHERD:
        max_mp = 0;
        break;

    default:
        ASSERT(0, "invalid player class: %d", player->klass);
    }

    /* mp always maxes out at 99 */
    if (max_mp > 99)
        max_mp = 99;

    return max_mp;
}

int playerCanEnterShrine(const SaveGame *saveGame, Virtue virtue) {
    if (saveGame->runes & (1 << (int) virtue))
        return 1;
    else
        return 0;
}

/**
 * Adjusts the avatar's karma level for the given action.  Activate
 * the lost eighth callback if the player has lost avatarhood.
 */
void playerAdjustKarma(SaveGame *saveGame, KarmaAction action) {
    int timeLimited = 0;
    int v, newKarma[VIRT_MAX], maxVal[VIRT_MAX];

    /*
     * make a local copy of all virtues, and adjust it according to
     * the game rules
     */
    for (v = 0; v < VIRT_MAX; v++) {
        newKarma[v] = saveGame->karma[v] == 0 ? 100 : saveGame->karma[v];
        maxVal[v] = saveGame->karma[v] == 0 ? 100 : 99;
    }

    switch (action) {
    case KA_FOUND_ITEM:
        AdjustValueMax(newKarma[VIRT_HONOR], 5, maxVal[VIRT_HONOR]);
        break;
    case KA_STOLE_CHEST:
        AdjustValueMin(newKarma[VIRT_HONESTY], -1, 1);
        AdjustValueMin(newKarma[VIRT_JUSTICE], -1, 1);
        AdjustValueMin(newKarma[VIRT_HONOR], -1, 1);
        break;
    case KA_GAVE_TO_BEGGAR:
        timeLimited = 1;
        AdjustValueMax(newKarma[VIRT_COMPASSION], 2, maxVal[VIRT_COMPASSION]);
        AdjustValueMax(newKarma[VIRT_HONOR], 3, maxVal[VIRT_HONOR]); /* FIXME: verify if honor goes up */
        break;
    case KA_BRAGGED:
        AdjustValueMin(newKarma[VIRT_HUMILITY], -5, 1);
        break;
    case KA_HUMBLE:
        timeLimited = 1;
        AdjustValueMax(newKarma[VIRT_HUMILITY], 10, maxVal[VIRT_HUMILITY]);
        break;
    case KA_HAWKWIND:
    case KA_MEDITATION:
        AdjustValueMax(newKarma[VIRT_SPIRITUALITY], 3, maxVal[VIRT_SPIRITUALITY]);
        break;
    case KA_BAD_MANTRA:
        AdjustValueMin(newKarma[VIRT_SPIRITUALITY], -3, 1);
        break;
    case KA_ATTACKED_GOOD:
        AdjustValueMin(newKarma[VIRT_COMPASSION], -5, 1);
        AdjustValueMin(newKarma[VIRT_JUSTICE], -5, 1);
        AdjustValueMin(newKarma[VIRT_HONOR], -5, 1);
        break;
    case KA_FLED_EVIL:
        AdjustValueMin(newKarma[VIRT_VALOR], -2, 1);
        break;
    case KA_HEALTHY_FLED_EVIL:
        AdjustValueMin(newKarma[VIRT_VALOR], -2, 1);
        AdjustValueMin(newKarma[VIRT_SACRIFICE], -2, 1);
        break;
    case KA_KILLED_EVIL:
        AdjustValueMax(newKarma[VIRT_VALOR], rand() % 2, maxVal[VIRT_VALOR]); /* gain one valor half the time, zero the rest */
        break;
    case KA_SPARED_GOOD:        
        AdjustValueMax(newKarma[VIRT_COMPASSION], 1, maxVal[VIRT_COMPASSION]);
        AdjustValueMax(newKarma[VIRT_JUSTICE], 1, maxVal[VIRT_JUSTICE]);
        break;    
    case KA_DONATED_BLOOD:
        AdjustValueMax(newKarma[VIRT_SACRIFICE], 5, maxVal[VIRT_SACRIFICE]);
        break;
    case KA_DIDNT_DONATE_BLOOD:
        AdjustValueMin(newKarma[VIRT_SACRIFICE], -5, 1);
        break;
    case KA_CHEAT_REAGENTS:
        AdjustValueMin(newKarma[VIRT_HONESTY], -10, 1);
        AdjustValueMin(newKarma[VIRT_JUSTICE], -10, 1);
        AdjustValueMin(newKarma[VIRT_HONOR], -10, 1);
        break;
    case KA_DIDNT_CHEAT_REAGENTS:
        timeLimited = 1;
        AdjustValueMax(newKarma[VIRT_HONESTY], 2, maxVal[VIRT_HONESTY]);
        AdjustValueMax(newKarma[VIRT_JUSTICE], 2, maxVal[VIRT_JUSTICE]);
        AdjustValueMax(newKarma[VIRT_HONOR], 2, maxVal[VIRT_HONOR]);
        break;
    case KA_USED_SKULL:
        /* using the skull is very, very bad... */
        for (v = 0; v < VIRT_MAX; v++)
            AdjustValueMin(newKarma[v], -5, -1);
        break;
    case KA_DESTROYED_SKULL:
        /* ...but destroying it is very, very good */
        for (v = 0; v < VIRT_MAX; v++)
            AdjustValueMax(newKarma[v], 10, maxVal[v]);
        break;
    }

    /*
     * check if enough time has passed since last virtue award if
     * action is time limited -- if not, throw away new values
     */
    if (timeLimited) {
        if (((saveGame->moves / 16) & 0xFFFF) != saveGame->lastvirtue)
            saveGame->lastvirtue = (saveGame->moves / 16) & 0xFFFF;
        else
            return;
    }

    /*
     * return to u4dos compatibility and handle losing of eighths
     */
    for (v = 0; v < VIRT_MAX; v++) {
        if (maxVal[v] == 100) { /* already an avatar */
            if (newKarma[v] < 100) { /* but lost it */
                if (lostEighthCallback)
                    (*lostEighthCallback)(v);
                c->saveGame->karma[v] = newKarma[v];
            }
            else c->saveGame->karma[v] = 0; /* return to u4dos compatibility */
        }
        else c->saveGame->karma[v] = newKarma[v];
    }
}

int playerAttemptElevation(SaveGame *saveGame, Virtue virtue) {
    if (saveGame->karma[virtue] == 99) {
        saveGame->karma[virtue] = 0;
        return 1;
    } else
        return 0;
}

int playerGetChest(SaveGame *saveGame) {
    int gold = (rand() % 50) + (rand() % 8) + 10;
    playerAdjustGold(saveGame, gold);    

    return gold;
}

int playerDonate(SaveGame *saveGame, int quantity) {
    if (quantity > saveGame->gold)
        return 0;

    playerAdjustGold(saveGame, -quantity);
    playerAdjustKarma(saveGame, KA_GAVE_TO_BEGGAR);

    if (itemStatsChangedCallback)
        (*itemStatsChangedCallback)();

    return 1;
}

int playerCanPersonJoin(SaveGame *saveGame, const char *name, Virtue *v) {
    int i;

    if (!name)
        return 0;    

    for (i = 1; i < 8; i++) {
        if (strcmp(saveGame->players[i].name, name) == 0) {
            if (v)
                *v = (Virtue) saveGame->players[i].klass;
            return 1;
        }
    }
    return 0;
}

int playerIsPersonJoined(SaveGame *saveGame, const char *name) {
    int i;

    if (!name)
        return 0;

    for (i = 1; i < saveGame->members; i++) {
        if (strcmp(saveGame->players[i].name, name) == 0)
            return 1;
    }
    return 0;
}

int playerJoin(SaveGame *saveGame, const char *name) {
    int i;
    SaveGamePlayerRecord tmp;

    for (i = saveGame->members; i < 8; i++) {
        if (strcmp(saveGame->players[i].name, name) == 0) {

            /* ensure avatar is experienced enough */
            if (saveGame->members + 1 > (saveGame->players[0].hpMax / 100))
                return JOIN_NOT_EXPERIENCED;

            /* ensure character has enough karma */
            if ((saveGame->karma[saveGame->players[i].klass] > 0) &&
                (saveGame->karma[saveGame->players[i].klass] < 40))
                return JOIN_NOT_VIRTUOUS;

            tmp = saveGame->players[saveGame->members];
            saveGame->players[saveGame->members] = saveGame->players[i];
            saveGame->players[i] = tmp;
            saveGame->members++;
            return JOIN_SUCCEEDED;
        }
    }

    return JOIN_NOT_EXPERIENCED;
}

void playerEndTurn(void) {
    int i;        
    SaveGame *saveGame = c->saveGame;
    
    saveGame->moves++;   
   
    for (i = 0; i < saveGame->members; i++) {

        /* Handle player status (only for non-combat turns) */
        if ((c->location->context & CTX_NON_COMBAT) == c->location->context) {
            
            /* party members eat food (also non-combat) */
            if (saveGame->players[i].status != STAT_DEAD)
                playerAdjustFood(saveGame, -1);

            switch (saveGame->players[i].status) {
            case STAT_SLEEPING:            
                if (rand() % 5 == 0)
                    saveGame->players[i].status = STAT_GOOD;
                break;

            case STAT_POISONED:            
                playerApplyDamage(&saveGame->players[i], 2);
                break;

            default:
                break;
            }
        }

        /* regenerate magic points */
        if (!playerIsDisabled(saveGame, i) &&        
            saveGame->players[i].mp < playerGetMaxMp(&(saveGame->players[i])))
            saveGame->players[i].mp++;        
    }

    /* The party is starving! */
    if ((saveGame->food == 0) && ((c->location->context & CTX_NON_COMBAT) == c->location->context))
        (*partyStarvingCallback)();
    
    /* heal ship (25% chance it is healed each turn) */
    if ((c->location->context == CTX_WORLDMAP) && (saveGame->shiphull < 50) && (rand() % 4 == 0))
        saveGame->shiphull++;
}

void playerApplyEffect(SaveGame *saveGame, TileEffect effect, int player) {
    int i;
    
    for (i = 0; i < saveGame->members; i++) {

        if (i != player && player != ALL_PLAYERS)
            continue;

        if (saveGame->players[i].status == STAT_DEAD)
            continue;

        switch (effect) {
        case EFFECT_NONE:
            break;
        case EFFECT_LAVA:
        case EFFECT_FIRE:
            if (i == player)
                playerApplyDamage(&(saveGame->players[i]), 16 + (rand() % 32));                
            else if (player == ALL_PLAYERS && rand() % 2 == 0)
                playerApplyDamage(&(saveGame->players[i]), 10 + (rand() % 25));
            break;
        case EFFECT_SLEEP:
            if (i == player || rand() % 5 == 0)
                saveGame->players[i].status = STAT_SLEEPING;
            break;
        case EFFECT_POISONFIELD:
        case EFFECT_POISON:
            if (i == player || rand() % 5 == 0)
                saveGame->players[i].status = STAT_POISONED;
            break;
        case EFFECT_ELECTRICITY: break;
        default:
            ASSERT(0, "invalid effect: %d", effect);
        }
    }
}

/**
 * Whether or not the party can make an action.
 */
int playerPartyImmobilized(const SaveGame *saveGame) {
    int i, immobile = 1;

    for (i = 0; i < saveGame->members; i++) {
        if (!playerIsDisabled(saveGame, i))        
            immobile = 0;        
    }

    return immobile;
}

/**
 * Whether or not all the party members are dead.
 */
int playerPartyDead(const SaveGame *saveGame) {
    int i, dead = 1;

    for (i = 0; i < saveGame->members; i++) {
        if (saveGame->players[i].status != STAT_DEAD) {
            dead = 0;
        }
    }

    return dead;
}

/**
 * Applies a sleep spell to the party.
 */
void playerApplySleepSpell(SaveGamePlayerRecord *player) {
    if (player->status != STAT_DEAD && (rand() % 2) == 0)
        player->status = STAT_SLEEPING;
}

int playerHeal(SaveGame *saveGame, HealType type, int player) {
    if (player >= saveGame->members)
        return 0;

    switch(type) {

    case HT_NONE:
        return 1;

    case HT_CURE:
        if (saveGame->players[player].status != STAT_POISONED)
            return 0;
        saveGame->players[player].status = STAT_GOOD;
        break;

    case HT_FULLHEAL:
        if (saveGame->players[player].status == STAT_DEAD ||
            saveGame->players[player].hp == saveGame->players[player].hpMax)
            return 0;
        saveGame->players[player].hp = saveGame->players[player].hpMax;
        break;

    case HT_RESURRECT:
        if (saveGame->players[player].status != STAT_DEAD)
            return 0;
        saveGame->players[player].status = STAT_GOOD;        
        break;

    case HT_HEAL:
        if (saveGame->players[player].status == STAT_DEAD ||
            saveGame->players[player].hp == saveGame->players[player].hpMax)
            return 0;        

        saveGame->players[player].hp += 75 + ((rand() & 0xFF) % 0x19);
        break;

    case HT_RESTHEAL:
        if (saveGame->players[player].status == STAT_DEAD ||
            saveGame->players[player].hp == saveGame->players[player].hpMax)
            return 0;        
        saveGame->players[player].hp += 75 + (rand() % 75) + (rand() % 75);
        break;
    default:
        return 0;
    }

    if (saveGame->players[player].hp > saveGame->players[player].hpMax)
        saveGame->players[player].hp = saveGame->players[player].hpMax;
    
    return 1;
}

void playerReviveParty(SaveGame *saveGame) {
    int i;

    for (i = 0; i < saveGame->members; i++) {
        saveGame->players[i].status = STAT_GOOD;
        saveGame->players[i].hp = saveGame->players[i].hpMax;
    }

    saveGame->food = 20099;
    saveGame->gold = 200;    
    (*setTransportCallback)(AVATAR_TILE);
}

/**
 * Returns the maximum number of items of a given price the player can
 * afford.
 */
int playerCanAfford(SaveGame *saveGame, int price) {
    if (price == 0)
        return 9999;
    return saveGame->gold / price;
}

/**
 * Attempt to purchase a given quantity of an item for a specified
 * price.  INV_NONE can be used to indicate a service is being
 * purchased instead of an item.  If successful, the inventory will be
 * updated and 1 returned.  Zero is returned on failure.
 */
int playerPurchase(SaveGame *saveGame, InventoryItem item, int type, int quantity, int price) {

    if (price > saveGame->gold)
        return 0;
        
    playerAdjustGold(saveGame, -price);

    switch (item) {
    case INV_NONE:
        /* nothing */
        break;
    case INV_WEAPON:
        AdjustValueMax(saveGame->weapons[type], quantity, 99);        
        break;
    case INV_ARMOR:
        AdjustValueMax(saveGame->armor[type], quantity, 99);        
        break;
    case INV_FOOD:
        playerAdjustFood(saveGame, quantity * 100);        
        break;
    case INV_REAGENT:
        AdjustValueMax(saveGame->reagents[type], quantity, 99);
        gameResetSpellMixing();
        break;
    case INV_GUILDITEM:
        if (type == 0)
            AdjustValueMax(saveGame->torches, quantity, 99);            
        else if (type == 1)
            AdjustValueMax(saveGame->gems, quantity, 99);            
        else if (type == 2)
            AdjustValueMax(saveGame->keys, quantity, 99);            
        else if (type == 3)
            AdjustValueMax(saveGame->sextants, quantity, 99);            
        break;
    case INV_HORSE:
        (*setTransportCallback)(tileGetHorseBase());
        break;
    }

    if (itemStatsChangedCallback)
        (*itemStatsChangedCallback)();

    return 1;
}

int playerCanSell(SaveGame *saveGame, InventoryItem item, int type, int quantity) {
    switch (item) {
    case INV_WEAPON:
        return (saveGame->weapons[type] >= quantity);

    case INV_ARMOR:
        return (saveGame->armor[type] >= quantity);

    default:
        return 0;
    }
}

/**
 * Attempt to sell a given quantity of an item for a specified price.
 * If successful, the inventory will be updated and 1 returned.  Zero
 * is returned on failure.
 */
int playerSell(SaveGame *saveGame, InventoryItem item, int type, int quantity, int price) {

    switch (item) {
    case INV_NONE:
        ASSERT(0, "invalid item: %d", item);
        break;
    case INV_WEAPON:
        if (saveGame->weapons[type] < quantity)
            return 0;
        saveGame->weapons[type] -= quantity;
        break;
    case INV_ARMOR:
        if (saveGame->armor[type] < quantity)
            return 0;
        saveGame->armor[type] -= quantity;
        break;
    default:
        return 0;
    }

    playerAdjustGold(saveGame, price);    

    if (itemStatsChangedCallback)
        (*itemStatsChangedCallback)();

    return 1;
}

/**
 * Determine whether a players attack hits or not.
 */
int playerAttackHit(const SaveGamePlayerRecord *player) {   
    if (weaponAlwaysHits(player->weapon) || player->dex >= 40)
        return 1;

    if ((player->dex + 128) >= (rand() & 0xff))
        return 1;
    else
        return 0;
}

/**
 * Calculate damage for an attack.
 */
int playerGetDamage(const SaveGamePlayerRecord *player) {
    int maxDamage;

    maxDamage = weaponGetDamage(player->weapon);
    maxDamage += player->str;
    if (maxDamage > 255)
        maxDamage = 255;

    return rand() % maxDamage;
}

/**
 * Determine whether a player is hit by a melee attack.
 */
int playerIsHitByAttack(const SaveGamePlayerRecord *player) {
    return (rand() % 256) > armorGetDefense(player->armor);
}

/**
 * Lose the equipped weapon for the player (flaming oil, ranged daggers, etc.)
 * Returns the number of weapons left of that type, including the one in
 * the players hand
 */

int playerLoseWeapon(SaveGame *saveGame, int player) {
    int weapon = saveGame->players[player].weapon;
    if (saveGame->weapons[weapon] > 0)
        return (--saveGame->weapons[weapon]) + 1;
    else {
        saveGame->players[player].weapon = WEAP_HANDS;    
        return 0;
    }
}

void playerAdjustGold(SaveGame *saveGame, int gold) {
    saveGame->gold += gold;
    if (saveGame->gold > 9999)
        saveGame->gold = (gold > 0) ? 9999 : 0;    
}

void playerAdjustFood(SaveGame *saveGame, int food) {
    saveGame->food += food;
    if (saveGame->food > 999900)
        saveGame->food = (food > 0) ? 999900 : 0;
}

int playerIsDisabled(const SaveGame *saveGame, int player) {
    return (saveGame->players[player].status == STAT_GOOD ||
        saveGame->players[player].status == STAT_POISONED) ? 0 : 1;
}