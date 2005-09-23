/*
 * $Id$
 */

#include "vc6.h" // Fixes things if you're using VC6, does nothing if otherwise

#include <ctime>
#include "u4.h"

#include "combat.h"

#include "annotation.h"
#include "context.h"
#include "creature.h"
#include "death.h"
#include "debug.h"
#include "dungeon.h"
#include "event.h"
#include "game.h"
#include "item.h"
#include "location.h"
#include "mapmgr.h"
#include "movement.h"
#include "names.h"
#include "object.h"
#include "player.h"
#include "portal.h"
#include "screen.h"
#include "settings.h"
#include "spell.h"
#include "stats.h"
#include "tileset.h"
#include "utils.h"
#include "weapon.h"

/**
 * Returns true if 'map' points to a Combat Map
 */ 
bool isCombatMap(Map *punknown) {
    CombatMap *ps;
    if ((ps = dynamic_cast<CombatMap*>(punknown)) != NULL)
        return true;
    else
        return false;
}

/**
 * Returns a CombatMap pointer to the map
 * passed, or a CombatMap pointer to the current map
 * if no arguments were passed.
 *
 * Returns NULL if the map provided (or current map)
 * is not a combat map.
 */
CombatMap *getCombatMap(Map *punknown) {
    Map *m = punknown ? punknown : c->location->map;
    if (!isCombatMap(m))
        return NULL;
    else return dynamic_cast<CombatMap*>(m);
}

/**
 * CombatController class implementation
 */
CombatController::CombatController() : map(NULL) {
    c->party->addObserver(this);
}

CombatController::CombatController(CombatMap *m) : map(m) {
    game->setMap(map, true, NULL);    
    c->party->addObserver(this);
}
CombatController::CombatController(MapId id) {
    map = getCombatMap(mapMgr->get(id));
    game->setMap(map, true, NULL);
    c->party->addObserver(this);
    forceStandardEncounterSize = false;
}

CombatController::~CombatController() {
    c->party->deleteObserver(this);
}

// Accessor Methods    
bool CombatController::isCamping() const                    { return camping; }
bool CombatController::isWinOrLose() const                  { return winOrLose; }
Direction CombatController::getExitDir() const              { return exitDir; }
unsigned char CombatController::getFocus() const            { return focus; }
CombatMap *CombatController::getMap() const                 { return map; }
Creature *CombatController::getCreature() const               { return creature; }
PartyMemberVector *CombatController::getParty()             { return &party; }
PartyMember* CombatController::getCurrentPlayer()           { return party[focus]; }

void CombatController::setExitDir(Direction d)              { exitDir = d; }
void CombatController::setCreature(Creature *m)               { creature = m; }
void CombatController::setWinOrLose(bool worl)              { winOrLose = worl; }
void CombatController::showCombatMessage(bool show)         { showMessage = show; }

/**
 * Initializes the combat controller with combat information
 */
void CombatController::init(class Creature *m) {
    int i;
    
    creature = m;    
    placeCreaturesOnMap = (m == NULL) ? false : true;
    placePartyOnMap = true;    
    winOrLose = true;
    map->setDungeonRoom(false);
    map->setAltarRoom(VIRT_NONE);
    showMessage = true;
    camping = false;

    /* initialize creature info */
    for (i = 0; i < AREA_CREATURES; i++) {
        creatureTable[i] = NULL;        
    }

    for (i = 0; i < AREA_PLAYERS; i++) {
        party.push_back(NULL);
    }

    /* fill the creature table if a creature was provided to create */    
    fillCreatureTable(m);

    /* initialize focus */
    focus = 0; 
}

/**
 * Initializes dungeon room combat
 */
void CombatController::initDungeonRoom(int room, Direction from) {
    int offset, i;
    init(NULL);

    ASSERT(c->location->prev->context & CTX_DUNGEON, "Error: called initDungeonRoom from non-dungeon context");
    {
        Dungeon *dng = dynamic_cast<Dungeon*>(c->location->prev->map);
        unsigned char
            *party_x = &dng->rooms[room].party_north_start_x[0], 
            *party_y = &dng->rooms[room].party_north_start_y[0];

        /* load the dungeon room properties */
        winOrLose = false;
        map->setDungeonRoom(true);
        exitDir = DIR_NONE;
        
        /* FIXME: this probably isn't right way to see if you're entering an altar room... but maybe it is */
        if ((c->location->prev->map->id != MAP_ABYSS) && (room == 0xF)) {            
            /* figure out which dungeon room they're entering */
            if (c->location->prev->coords.x == 3)
                map->setAltarRoom(VIRT_LOVE);
            else if (c->location->prev->coords.x <= 2)
                map->setAltarRoom(VIRT_TRUTH);
            else map->setAltarRoom(VIRT_COURAGE);
        }        
        
        /* load in creatures and creature start coordinates */
        for (i = 0; i < AREA_CREATURES; i++) {
            if (dng->rooms[room].creature_tiles[i] > 0) {
                placeCreaturesOnMap = true;
                creatureTable[i] = creatureMgr->getByTile(dng->rooms[room].creature_tiles[i]);
            }
            map->creature_start[i].x = dng->rooms[room].creature_start_x[i];
            map->creature_start[i].y = dng->rooms[room].creature_start_y[i];            
        }
        
        /* figure out party start coordinates */
        switch(from) {
        case DIR_WEST: offset = 3; break;
        case DIR_NORTH: offset = 0; break;
        case DIR_EAST: offset = 1; break;
        case DIR_SOUTH: offset = 2; break;
        case DIR_ADVANCE:
        case DIR_RETREAT:
        default: 
            ASSERT(0, "Invalid 'from' direction passed to initDungeonRoom()");
        }

        for (i = 0; i < AREA_PLAYERS; i++) {
            map->player_start[i].x = *(party_x + (offset * AREA_PLAYERS * 2) + i);
            map->player_start[i].y = *(party_y + (offset * AREA_PLAYERS * 2) + i);
        }
    }
}

/**
 * Apply tile effects to all creatures depending on what they're standing on
 */
void CombatController::applyCreatureTileEffects() {
    CreatureVector creatures = map->getCreatures();
    CreatureVector::iterator i;

    for (i = creatures.begin(); i != creatures.end(); i++) {        
        Creature *m = *i;
        TileEffect effect = map->tileAt(m->getCoords(), WITH_GROUND_OBJECTS)->getEffect();
        m->applyTileEffect(effect);
    }
}

/**
 * Begin combat
 */
void CombatController::begin() {
    bool partyIsReadyToFight = false;    
    
    /* place party members on the map */    
    if (placePartyOnMap)        
        placePartyMembers();    

    /* place creatures on the map */
    if (placeCreaturesOnMap)
        placeCreatures();

    /* if we entered an altar room, show the name */
    if (map->isAltarRoom()) {
        screenMessage("\nThe Altar Room of %s\n", getBaseVirtueName(map->getAltarRoom()));    
        c->location->context = static_cast<LocationContext>(c->location->context | CTX_ALTAR_ROOM);
    }    
 
    /* if there are creatures around, start combat! */    
    if (showMessage && placeCreaturesOnMap && winOrLose)
        screenMessage("\n**** COMBAT ****\n\n");
    
    /* FIXME: there should be a better way to accomplish this */
    if (!camping) {
        musicMgr->play();
    }

    /* Set focus to the first active party member, if there is one */ 
    for (int i = 0; i < AREA_PLAYERS; i++) {
        if (setActivePlayer(i)) {
            partyIsReadyToFight = true;
            break;
        }
    }    

    if (!camping && !partyIsReadyToFight)
        (*c->location->finishTurn)();

    eventHandler->pushController(this);
}

void CombatController::end(bool adjustKarma) {
    eventHandler->popController();

    /* The party is dead -- start the death sequence */
    if (c->party->isDead()) {
        /* remove the creature */
        if (creature)
            c->location->map->removeObject(creature);

        deathStart(5);
        return;
    }
    
    /* need to get this here because when we exit to the parent map, all the monsters are cleared */
    bool won = isWon();    
    
    game->exitToParentMap();
    musicMgr->play();
    
    if (winOrLose) {
        if (won) {
            if (creature) {
                if (creature->isEvil())
                    c->party->adjustKarma(KA_KILLED_EVIL);
                awardLoot();
            }
            
            screenMessage("\nVictory!\n");
        }
        else if (!c->party->isDead()) {
            /* minus points for fleeing from evil creatures */
            if (adjustKarma && creature && creature->isEvil()) {
                screenMessage("Battle is lost!\n");
                c->party->adjustKarma(KA_FLED_EVIL);
            }
            else if (adjustKarma && creature && creature->isGood())
                c->party->adjustKarma(KA_FLED_GOOD);
        }
    }

    /* exiting a dungeon room */
    if (map->isDungeonRoom()) {
        screenMessage("Leave Room!\n");
        if (map->isAltarRoom()) {            
            PortalTriggerAction action = ACTION_NONE;

            /* when exiting altar rooms, you exit to other dungeons.  Here it goes... */
            switch(exitDir) {
            case DIR_NORTH: action = ACTION_EXIT_NORTH; break;
            case DIR_EAST:  action = ACTION_EXIT_EAST; break;
            case DIR_SOUTH: action = ACTION_EXIT_SOUTH; break;
            case DIR_WEST:  action = ACTION_EXIT_WEST; break;            
            case DIR_NONE:  break;
            case DIR_ADVANCE:
            case DIR_RETREAT:
            default: ASSERT(0, "Invalid exit dir %d", exitDir); break;
            }

            if (action != ACTION_NONE)
                usePortalAt(c->location, c->location->coords, action);
        }
        else screenMessage("\n");

        if (exitDir != DIR_NONE) {
            c->saveGame->orientation = exitDir;  /* face the direction exiting the room */
            c->location->move(DIR_NORTH, false);  /* advance 1 space outside of the room */
        }
    }

    /* remove the creature */
    if (creature)
        c->location->map->removeObject(creature);

    /* reset our combat variables */
    camping = false;
    
    /* Make sure finishturn only happens if a new combat has not begun */
    if (eventHandler->getController() != this)
        (*c->location->finishTurn)();
}

/**
 * Fills the combat creature table with the creatures that the party will be facing.
 * The creature table only contains *which* creatures will be encountered and
 * *where* they are placed (by position in the table).  Information like
 * hit points and creature status will be created when the creature is actually placed
 */
void CombatController::fillCreatureTable(const Creature *creature) {
    int i, j;
    
    if (creature != NULL) { 
        const Creature *baseCreature = creature, *current;
        int numCreatures = initialNumberOfCreatures(creature);

        if (baseCreature->getId() == PIRATE_ID)
            baseCreature = creatureMgr->getById(ROGUE_ID);

        for (i = 0; i < numCreatures; i++) {
            current = baseCreature;

            /* find a free spot in the creature table */
            do {j = xu4_random(AREA_CREATURES) ;} while (creatureTable[j] != NULL);
            
            /* see if creature is a leader or leader's leader */
            if (creatureMgr->getById(baseCreature->getLeader()) != baseCreature && /* leader is a different creature */
                i != (numCreatures - 1)) { /* must have at least 1 creature of type encountered */
                
                if (xu4_random(32) == 0)       /* leader's leader */
                    current = creatureMgr->getById(creatureMgr->getById(baseCreature->getLeader())->getLeader());
                else if (xu4_random(8) == 0)   /* leader */
                    current = creatureMgr->getById(baseCreature->getLeader());
            }

            /* place this creature in the creature table */
            creatureTable[j] = current;
        }
    }
}

/**
 * Generate the number of creatures in a group.
 */
int  CombatController::initialNumberOfCreatures(const Creature *creature) const {
    int ncreatures;
    Map *map = c->location->prev ? c->location->prev->map : c->location->map;

    /* if in an unusual combat situation, generally we stick to normal encounter sizes,
       (such as encounters from sleeping in an inn, etc.) */
    if (forceStandardEncounterSize || map->isWorldMap() || (c->location->prev && c->location->prev->context & CTX_DUNGEON)) {
        ncreatures = xu4_random(8) + 1;
        
        if (ncreatures == 1) {
            if (creature && creature->getEncounterSize() > 0)
                ncreatures = xu4_random(creature->getEncounterSize()) + creature->getEncounterSize() + 1;
            else
                ncreatures = 8;
        }

        while (ncreatures > 2 * c->saveGame->members) {
            ncreatures = xu4_random(16) + 1;
        }
    } else {
        if (creature && creature->getId() == GUARD_ID)
            ncreatures = c->saveGame->members * 2;
        else
            ncreatures = 1;
    }

    return ncreatures;
}

/**
 * Returns true if the player has won.
 */
bool CombatController::isWon() const {
    CreatureVector creatures = map->getCreatures();    
    if (creatures.size())
        return false;    
    return true;
}

/**
 * Returns true if the player has lost.
 */
bool CombatController::isLost() const {
    PartyMemberVector party = map->getPartyMembers();
    if (party.size())
        return false;
    return true;
}

/**
 * Performs all of the creature's actions
 */
void CombatController::moveCreatures() {
    Creature *m;

    // XXX: this iterator is rather complex; but the vector::iterator can
    // break and crash if we delete elements while iterating it, which we do
    // if a jinxed monster kills another
    for (unsigned int i = 0; i < map->getCreatures().size(); i++) {
        m = map->getCreatures().at(i);
        m->act();
        if (i < map->getCreatures().size() && map->getCreatures().at(i) != m) {
            // don't skip a later creature when an earlier one flees
            i--;
        }
    }
}

/**
 * Places creatures on the map from the creature table and from the creature_start coords
 */
void CombatController::placeCreatures() {
    int i;    

    for (i = 0; i < AREA_CREATURES; i++) {        
        const Creature *m = creatureTable[i];
        if (m)
            map->addCreature(m, map->creature_start[i]);
    }
}

/**
 * Places the party members on the map
 */
void CombatController::placePartyMembers() {
    int i;
    party.clear();
    
    for (i = 0; i < c->party->size(); i++) {
        PartyMember *p = c->party->member(i);        
        p->setFocus(false); // take the focus off of everyone        

        /* don't place dead party members */
        if (p->getStatus() != STAT_DEAD) {
            /* add the party member to the map */
            p->setCoords(map->player_start[i]);
            p->setMap(map);
            map->objects.push_back(p);
            party[i] = p;
        }
    }
}

/**
 * Sets the active player for combat, showing which weapon they're weilding, etc.
 */
bool CombatController::setActivePlayer(int player) {
    PartyMember *p = party[player];
    
    if (p && !p->isDisabled()) {        
        if (party[focus])
            party[focus]->setFocus(false);        

        p->setFocus();
        focus = player;

        screenMessage("%s with %s\n\020", p->getName().c_str(), Weapon::get(p->getWeapon())->getName().c_str());        
        c->stats->highlightPlayer(focus);        
        return true;
    }
    
    return false;
}

void CombatController::awardLoot() {
    Coords coords = creature->getCoords();
    MapTile *ground = c->location->map->tileAt(coords, WITHOUT_OBJECTS);

    /* add a chest, if the creature leaves one */
    if (creature->leavesChest() && 
        ground->isCreatureWalkable() &&
        (!(c->location->context & CTX_DUNGEON) || ground->isDungeonFloor())) {
        MapTile chest = Tileset::findTileByName("chest")->id;
        c->location->map->addObject(chest, chest, coords);
    }
    /* add a ship if you just defeated a pirate ship */
    else if (creature->getTile().isPirateShip()) {
        MapTile ship = Tileset::findTileByName("ship")->id;
        ship.setDirection(creature->getTile().getDirection());
        c->location->map->addObject(ship, ship, coords);
    }
}

bool CombatController::attackAt(const Coords &coords, PartyMember *attacker, int dir, int range, int distance) {
    const Weapon *weapon = Weapon::get(attacker->getWeapon());
    bool wrongRange = weapon->rangeAbsolute() && (distance != range);
    int attackdelay = MAX_BATTLE_SPEED - settings.battleSpeed;    

    MapTile hittile = weapon->getHitTile();
    MapTile misstile = weapon->getMissTile();

    // Check to see if something hit
    Creature *creature = map->creatureAt(coords);

    /* If we haven't hit a creature, or the weapon's range is absolute
       and we're testing the wrong range, stop now! */
    if (!creature || wrongRange) {
        
        /* If the weapon is shown as it travels, show it now */
        if (weapon->showTravel()) {
            map->annotations->add(coords, misstile, true);
            gameUpdateScreen();
        
            /* Based on attack speed setting in setting struct, make a delay for
               the attack annotation */
            if (attackdelay > 0)
                EventHandler::wait_msecs(attackdelay * 2);

            map->annotations->remove(coords, misstile);
        }

        // no target found
        return false;
    }
    
    /* Did the weapon miss? */
    if ((c->location->prev->map->id == MAP_ABYSS && !weapon->isMagic()) || /* non-magical weapon in the Abyss */
        !attacker->attackHit(creature)) { /* player naturally missed */
        screenMessage("Missed!\n");
        
        /* show the 'miss' tile */
        attackFlash(coords, misstile, 3);
        soundPlay(SOUND_MISSED, false);

    } else { /* The weapon hit! */

        /* show the 'hit' tile */
        attackFlash(coords, hittile, 3);            

        /* apply the damage to the creature */
        if (!attacker->dealDamage(creature, attacker->getDamage()))
            creature = NULL;

        /* creature is still alive and has the chance to divide - xu4 enhancement */
        if (xu4_random(2) == 0 && creature && creature->divides())
            creature->divide();
    }

    return true;
}

bool CombatController::rangedAttack(const Coords &coords, Creature *attacker) {
    int attackdelay = MAX_BATTLE_SPEED - settings.battleSpeed;    
    
    MapTile hittile = attacker->getHitTile();
    MapTile misstile = attacker->getMissTile();

    Creature *target = isCreature(attacker) ? map->partyMemberAt(coords) : map->creatureAt(coords);

    /* If we haven't hit something valid, stop now */
    if (!target) {            
        map->annotations->add(coords, misstile, true);            
        gameUpdateScreen();            
    
        /* Based on attack speed setting in setting struct, make a delay for
           the attack annotation */
        if (attackdelay > 0)
            EventHandler::wait_msecs(attackdelay * 2);

        map->annotations->remove(coords, misstile);

        return false;
    }

    /* Get the effects of the tile the creature is using */
    TileEffect effect = hittile.getEffect();
  
    /* Monster's ranged attacks never miss */

    /* show the 'hit' tile */
    attackFlash(coords, hittile, 4);        

    /* These effects happen whether or not the opponent was hit */
    switch(effect) {
        
    case EFFECT_ELECTRICITY:
        /* FIXME: are there any special effects here? */
        screenMessage("\n%s Electrified!\n", target->getName().c_str());
        attacker->dealDamage(target, attacker->getDamage());
        break;
        
    case EFFECT_POISON:
    case EFFECT_POISONFIELD:
            
        screenMessage("\n%s Poisoned!\n", target->getName().c_str());

        /* see if the player is poisoned */
        if ((xu4_random(2) == 0) && (target->getStatus() != STAT_POISONED)) {
            target->addStatus(STAT_POISONED);
            soundPlay(SOUND_PLAYERHIT, false);
        }
        else screenMessage("Failed.\n");
        break;
        
    case EFFECT_SLEEP:

        screenMessage("\n%s Slept!\n", target->getName().c_str());
        soundPlay(SOUND_PLAYERHIT, false);

        /* see if the player is put to sleep */
        if (xu4_random(2) == 0)
            target->putToSleep();
        else screenMessage("Failed.\n");
        break;

    case EFFECT_LAVA:
    case EFFECT_FIRE:
        /* FIXME: are there any special effects here? */            
        screenMessage("\n%s %s Hit!\n", target->getName().c_str(),
                      effect == EFFECT_LAVA ? "Lava" : "Fiery");
        attacker->dealDamage(target, attacker->getDamage());
        break;
                
    default: 
        /* show the appropriate 'hit' message */
        if (hittile == Tileset::findTileByName("magic_flash")->id)
            screenMessage("\n%s Magical Hit!\n", target->getName().c_str());
        else screenMessage("\n%s Hit!\n", target->getName().c_str());
        attacker->dealDamage(target, attacker->getDamage());
        break;
    }       

    return true;
}

void CombatController::rangedMiss(const Coords &coords, Creature *attacker) {
    soundPlay(SOUND_MISSED, false);

    /* If the creature leaves a tile behind, do it here! (lava lizard, etc) */
    MapTile *groundTile = map->tileAt(coords, WITH_GROUND_OBJECTS);
    if (attacker->leavesTile() && groundTile->isWalkable())
        map->annotations->add(coords, attacker->getHitTile());
}

bool CombatController::returnWeaponToOwner(const Coords &coords, int distance, int dir, const Weapon *weapon) {
    int attackdelay = MAX_BATTLE_SPEED - settings.battleSpeed;
    MapCoords new_coords = coords;

    MapTile misstile = weapon->getMissTile();

    /* reverse the direction of the weapon */
    Direction returnDir = dirReverse(dirFromMask(dir));

    for (int i = distance; i > 1; i--) {
        new_coords.move(returnDir, map);        
        
        map->annotations->add(new_coords, misstile, true);
        gameUpdateScreen();

        /* Based on attack speed setting in setting struct, make a delay for
           the attack annotation */
        if (attackdelay > 0)
            EventHandler::wait_msecs(attackdelay * 2);
        
        map->annotations->remove(new_coords, misstile);
    }
    gameUpdateScreen();

    return true;
}

/**
 * Show an attack flash at x, y on the current map.
 * This is used for 'being hit' or 'being missed' 
 * by weapons, cannon fire, spells, etc.
 */
void CombatController::attackFlash(Coords coords, MapTile tile, int timeFactor) {
    int i;
    int divisor = settings.battleSpeed;
    
    c->location->map->annotations->add(coords, tile, true);
    for (i = 0; i < timeFactor; i++) {        
        /* do screen animations while we're pausing */
        if (i % divisor == 1)
            screenCycle();

        gameUpdateScreen();       
        EventHandler::wait_msecs(eventTimerGranularity/divisor);
    }
    c->location->map->annotations->remove(coords, tile);
}

void CombatController::finishTurn(void) {
    CombatController *ct = c->combat;    
    PartyMember *player = ct->getCurrentPlayer();
    int quick;

    /* return to party overview */
    c->stats->setView(STATS_PARTY_OVERVIEW);

    if (ct->isWon() && ct->winOrLose) {        
        ct->end(true);
        return;
    }
    
    /* make sure the player with the focus is still in battle (hasn't fled or died) */
    if (player) {
        /* apply effects from tile player is standing on */
        player->applyEffect(c->location->map->tileAt(player->getCoords(), WITH_GROUND_OBJECTS)->getEffect());
    }

    quick = (*c->aura == Aura::QUICKNESS) && player && (xu4_random(2) == 0) ? 1 : 0;

    /* check to see if the player gets to go again (and is still alive) */
    if (!quick || player->isDisabled()){    

        do {
            c->location->map->annotations->passTurn();

            /* put a sleeping person in place of the player,
               or restore an awakened member to their original state */            
            if (player) {                
                if (player->getStatus() == STAT_SLEEPING && (xu4_random(8) == 0))
                    player->wakeUp();                

                /* remove focus from the current party member */
                player->setFocus(false);

                /* eat some food */
                c->party->adjustFood(-1);
            }

            /* put the focus on the next party member */
            ct->focus++;
            player = ct->getCurrentPlayer();

            /* move creatures and wrap around at end */
            if (ct->focus >= c->party->size()) {   
                
                /* reset the focus to the avatar and start the party's turn over again */
                ct->focus = 0;
                player = ct->getCurrentPlayer();

                gameUpdateScreen();
                EventHandler::sleep(50); /* give a slight pause in case party members are asleep for awhile */

                /* adjust moves */
                c->party->endTurn();

                /* count down our aura (if we have one) */
                c->aura->passTurn();                

                /** 
                 * ====================
                 * HANDLE CREATURE STUFF
                 * ====================
                 */
            
                /* first, move all the creatures */
                ct->moveCreatures();

                /* then, apply tile effects to creatures */
                ct->applyCreatureTileEffects();                

                /* check to see if combat is over */
                if (ct->isLost()) {                    
                    ct->end(true);
                    return;
                }

                /* end combat immediately if the enemy has fled */
                else if (ct->isWon() && ct->winOrLose) {                    
                    ct->end(true);
                    return;
                }
            }
        } while (!player || 
                  player->isDisabled() || /* dead or sleeping */                 
                 ((c->party->getActivePlayer() >= 0) && /* active player is set */
                  (ct->party[c->party->getActivePlayer()]) && /* and the active player is still in combat */
                  !ct->party[c->party->getActivePlayer()]->isDisabled() && /* and the active player is not disabled */
                  (c->party->getActivePlayer() != ct->focus)));
    }
    else c->location->map->annotations->passTurn();
    
#if 0
    if (ct->focus != 0) {
        ct->getCurrentPlayer()->act();
        ct->finishTurn();
    }
    else ct->setActivePlayer(ct->focus);
#else
    /* display info about the current player */
    ct->setActivePlayer(ct->focus);
#endif
}

/**
 * Move a party member during combat and display the appropriate messages
 */
void CombatController::movePartyMember(MoveEvent &event) {
    /* active player left/fled combat */
    if ((event.result & MOVE_EXIT_TO_PARENT) && (c->party->getActivePlayer() == focus)) {
        c->party->setActivePlayer(-1);
        /* assign active player to next available party member */
        for (int i = 0; i < c->party->size(); i++) {
            if (party[i] && !party[i]->isDisabled()) {
                c->party->setActivePlayer(i);
                break;
            }
        }
    }

    screenMessage("%s\n", getDirectionName(event.dir));
    if (event.result & MOVE_MUST_USE_SAME_EXIT)
        screenMessage("All must use same exit!\n");
    else if (event.result & MOVE_BLOCKED)
        screenMessage("Blocked!\n");
    else if (event.result & MOVE_SLOWED)
        screenMessage("Slow progress!\n"); 
    else if (winOrLose && getCreature()->isEvil() && (event.result & (MOVE_EXIT_TO_PARENT | MOVE_MAP_CHANGE)))
        soundPlay(SOUND_FLEE);
}

// Key handlers
bool CombatController::keyPressed(int key) {
    bool valid = true;
    bool endTurn = true;

    switch (key) {
    case U4_UP:
    case U4_DOWN:
    case U4_LEFT:
    case U4_RIGHT:
        c->location->move(keyToDirection(key), true);
        break;

    case U4_ESC:
        if (settings.debug)            
            end(false);         /* don't adjust karma */        
        else screenMessage("Bad command\n");        

        break;
        
    case ' ':
        screenMessage("Pass\n");
        break;

    case U4_FKEY:
        {
            extern void gameDestroyAllCreatures();

            if (settings.debug)
                gameDestroyAllCreatures();
            else valid = false;
            break;
        }

    // Change the speed of battle
    case '+':
    case '-':
    case U4_KEYPAD_ENTER:
        {
            int old_speed = settings.battleSpeed;
            if (key == '+' && ++settings.battleSpeed > MAX_BATTLE_SPEED)
                settings.battleSpeed = MAX_BATTLE_SPEED;        
            else if (key == '-' && --settings.battleSpeed == 0)
                settings.battleSpeed = 1;
            else if (key == U4_KEYPAD_ENTER)
                settings.battleSpeed = DEFAULT_BATTLE_SPEED;

            if (old_speed != settings.battleSpeed) {        
                if (settings.battleSpeed == DEFAULT_BATTLE_SPEED)
                    screenMessage("Battle Speed:\nNormal\n");
                else if (key == '+')
                    screenMessage("Battle Speed:\nUp (%d)\n", settings.battleSpeed);
                else screenMessage("Battle Speed:\nDown (%d)\n", settings.battleSpeed);
            }
            else if (settings.battleSpeed == DEFAULT_BATTLE_SPEED)
                screenMessage("Battle Speed:\nNormal\n");
        }        

        valid = false;
        break;

    case 'a':
        attack();
        break;

    case 'c':
        screenMessage("Cast Spell!\n");
        castSpell(focus);
        endTurn = false; /* gameCastForPlayer calls finishTurn() */
        break;

    case 'g':
        screenMessage("Get Chest!\n");
        getChest(focus);
        break;

    case 'l':
        if (settings.debug) {
            Coords coords = getCurrentPlayer()->getCoords();
            screenMessage("\nLocation:\nx:%d\ny:%d\nz:%d\n", coords.x, coords.y, coords.z);
            screenPrompt();
            valid = false;
        }
        else
            screenMessage("Not here!\n");
        break;            

    case 'r':
        readyWeapon(getFocus());
        break;

    case 't':
        if (settings.debug && map->isDungeonRoom()) {
            Dungeon *dungeon = dynamic_cast<Dungeon*>(c->location->prev->map);
            Trigger *triggers = dungeon->rooms[dungeon->currentRoom].triggers;
            int i;

            screenMessage("Triggers!\n");

            for (i = 0; i < 4; i++) {
                screenMessage("%.1d)xy tile xy xy\n", i+1);
                screenMessage("  %.1X%.1X  %.3d %.1X%.1X %.1X%.1X\n",
                    triggers[i].x, triggers[i].y,
                    triggers[i].tile,
                    triggers[i].change_x1, triggers[i].change_y1,
                    triggers[i].change_x2, triggers[i].change_y2);                
            }
            screenPrompt();
            valid = false;
            
        }
        else
            screenMessage("Not here!\n");
        break;

    case 'u':
        screenMessage("Use which item:\n");
        c->stats->setView(STATS_ITEMS);
        itemUse(gameGetInput().c_str());
        break;

    case 'v':
        if (musicMgr->toggle())
            screenMessage("Volume On!\n");
        else
            screenMessage("Volume Off!\n");
        break;

    case 'v' + U4_ALT:
        screenMessage("XU4 %s\n", VERSION);        
        break;

    case 'z': 
        {            
            c->stats->setView(StatsView(STATS_CHAR1 + getFocus()));

            /* reset the spell mix menu and un-highlight the current item,
               and hide reagents that you don't have */            
            c->stats->resetReagentsMenu();

            screenMessage("Ztats\n");        
            ZtatsController ctrl;
            eventHandler->pushController(&ctrl);
            ctrl.waitFor();
        }
        break;    

    case 'b':
    case 'e':
    case 'd':
    case 'f':    
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 's':    
    case 'w':
    case 'x':   
    case 'y':
        screenMessage("Not here!\n");
        break;

    case '0':        
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (settings.enhancements && settings.enhancementsOptions.activePlayer)
            gameSetActivePlayer(key - '1');            
        else screenMessage("Bad command\n");

        break;    

    default:
        valid = false;
        break;
    }

    if (valid) {
        c->lastCommandTime = time(NULL);
        if (endTurn && (eventHandler->getController() == this))
            (*c->location->finishTurn)();
    }

    return valid;
}

/**
 * Key handler for choosing an attack direction
 */
void CombatController::attack() {
    screenMessage("Dir: ");

    ReadDirController dirController;
    eventHandler->pushController(&dirController);
    Direction dir = dirController.waitFor();
    if (dir == DIR_NONE)
        return;
    screenMessage("%s\n", getDirectionName(dir));

    PartyMember *attacker = getCurrentPlayer();

    const Weapon *weapon = Weapon::get(attacker->getWeapon());
    int range = weapon->getRange();
    if (weapon->canChooseDistance()) {
        screenMessage("Range: ");
        int choice = ReadChoiceController::get("123456789");
        if ((choice - '0') >= 1 && (choice - '0') <= weapon->getRange()) {
            range = choice - '0';
            screenMessage("%d\n", range);
        } else {
            return;
        }
    }

    vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, 
                                                       attacker->getCoords(),
                                                       1, range, 
                                                       weapon->canAttackThroughObjects() ? NULL : &MapTile::canAttackOverTile,
                                                       false);

    bool foundTarget = false;
    int targetDistance = path.size();
    Coords targetCoords = path[path.size() - 1];

    int distance = 1;
    for (vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (attackAt(*i, attacker, MASK_DIR(dir), range, distance)) {
            foundTarget = true;
            targetDistance = distance;
            targetCoords = *i;
            break;
        }
        distance++;
    }

    // is weapon lost? (e.g. dagger)
    if (weapon->loseWhenUsed() ||
        (weapon->loseWhenRanged() && (!foundTarget || targetDistance > 1))) {
        if (!attacker->loseWeapon())
            screenMessage("Last One!\n");
    }

    // does weapon leaves a tile behind? (e.g. flaming oil)
    MapTile *groundTile = map->tileAt(targetCoords, WITHOUT_OBJECTS);
    if (weapon->leavesTile().id > 0 && groundTile->isWalkable() &&
        (!foundTarget || targetDistance == range))
        map->annotations->add(path[path.size() - 1], weapon->leavesTile());

    /* show the 'miss' tile */
    if (!foundTarget) {
        attackFlash(targetCoords, weapon->getMissTile(), 3);
        soundPlay(SOUND_MISSED, false);

        /* This goes here so messages are shown in the original order */
        screenMessage("Missed!\n");
    }

    // does weapon returns to its owner? (e.g. magic axe)
    if (weapon->returns())
        returnWeaponToOwner(targetCoords, targetDistance, MASK_DIR(dir), weapon);
}

void CombatController::update(Party *party, PartyEvent &event) {
    if (event.type == PartyEvent::PLAYER_KILLED)
        screenMessage("%s is Killed!\n", event.player->getName().c_str());
}


/**
 * CombatMap class implementation
 */ 
CombatMap::CombatMap() : Map(), dungeonRoom(false), altarRoom(VIRT_NONE), contextual(false) {}

/**
 * Returns a vector containing all of the creatures on the map
 */ 
CreatureVector CombatMap::getCreatures() {
    ObjectDeque::iterator i;
    CreatureVector creatures;
    for (i = objects.begin(); i != objects.end(); i++) {
        if (isCreature(*i) && !isPartyMember(*i))
            creatures.push_back(dynamic_cast<Creature*>(*i));
    }
    return creatures;
}

/**
 * Returns a vector containing all of the party members on the map
 */ 
PartyMemberVector CombatMap::getPartyMembers() {
    ObjectDeque::iterator i;
    PartyMemberVector party;
    for (i = objects.begin(); i != objects.end(); i++) {
        if (isPartyMember(*i))
            party.push_back(dynamic_cast<PartyMember*>(*i));
    }
    return party;
}

/**
 * Returns the party member at the given coords, if there is one,
 * NULL if otherwise.
 */ 
PartyMember *CombatMap::partyMemberAt(Coords coords) {
    PartyMemberVector party = getPartyMembers();
    PartyMemberVector::iterator i;
    
    for (i = party.begin(); i != party.end(); i++) {
        if ((*i)->getCoords() == coords)
            return *i;
    }
    return NULL;
}

/**
 * Returns the creature at the given coords, if there is one,
 * NULL if otherwise.
 */ 
Creature *CombatMap::creatureAt(Coords coords) {
    CreatureVector creatures = getCreatures();
    CreatureVector::iterator i;

    for (i = creatures.begin(); i != creatures.end(); i++) {
        if ((*i)->getCoords() == coords)            
            return *i;
    }
    return NULL;
}

/**
 * Returns a valid combat map given the provided information
 */ 
MapId CombatMap::mapForTile(MapTile groundTile, MapTile transport, Object *obj) {
    bool fromShip = false,
        toShip = false;
    Object *objUnder = c->location->map->objectAt(c->location->coords);

    static std::map<MapTile, MapId> tileMap;
    if (!tileMap.size()) {        
        tileMap[Tileset::findTileByName("horse")->id] = MAP_GRASS_CON;        
        tileMap[Tileset::findTileByName("swamp")->id] = MAP_MARSH_CON;
        tileMap[Tileset::findTileByName("grass")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("brush")->id] = MAP_BRUSH_CON;
        tileMap[Tileset::findTileByName("forest")->id] = MAP_FOREST_CON;
        tileMap[Tileset::findTileByName("hills")->id] = MAP_HILL_CON;
        tileMap[Tileset::findTileByName("dungeon")->id] = MAP_DUNGEON_CON;
        tileMap[Tileset::findTileByName("city")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("castle")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("town")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("lcb_entrance")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("bridge")->id] = MAP_BRIDGE_CON;
        tileMap[Tileset::findTileByName("balloon")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("bridge_pieces")->id] = MAP_BRIDGE_CON;        
        tileMap[Tileset::findTileByName("shrine")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("chest")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("brick_floor")->id] = MAP_BRICK_CON;
        tileMap[Tileset::findTileByName("moongate")->id] = MAP_GRASS_CON;
        tileMap[Tileset::findTileByName("moongate_opening")->id] = MAP_GRASS_CON;        
        tileMap[Tileset::findTileByName("dungeon_floor")->id] = MAP_GRASS_CON;        
    }
    static std::map<MapTile, MapId> dungeontileMap;
    if (!dungeontileMap.size()) {               
        dungeontileMap[Tileset::findTileByName("brick_floor")->id] = MAP_DNG0_CON;
        dungeontileMap[Tileset::findTileByName("up_ladder")->id] = MAP_DNG1_CON;
        dungeontileMap[Tileset::findTileByName("down_ladder")->id] = MAP_DNG2_CON;
        dungeontileMap[Tileset::findTileByName("up_down_ladder")->id] = MAP_DNG3_CON;
        // dungeontileMap[Tileset::findTileByName("chest")->id] = MAP_DNG4_CON; 
        // chest tile doesn't work that well
        dungeontileMap[Tileset::findTileByName("dungeon_door")->id] = MAP_DNG5_CON;
        dungeontileMap[Tileset::findTileByName("secret_door")->id] = MAP_DNG6_CON;
    }

    if (c->location->context & CTX_DUNGEON) {
        if (dungeontileMap.find(groundTile) != dungeontileMap.end())
            return dungeontileMap[groundTile];    

        return MAP_DNG0_CON;
    }

    if (transport.isShip() || (objUnder && objUnder->getTile().isShip()))
        fromShip = true;
    if (obj->getTile().isPirateShip())
        toShip = true;

    if (fromShip && toShip)
        return MAP_SHIPSHIP_CON;

    /* We can fight creatures and townsfolk */       
    if (obj->getType() != Object::UNKNOWN) {
        MapTile *tileUnderneath = c->location->map->tileAt(obj->getCoords(), WITHOUT_OBJECTS);

        if (toShip)
            return MAP_SHORSHIP_CON;
        else if (fromShip && tileUnderneath->isWater())
            return MAP_SHIPSEA_CON;
        else if (tileUnderneath->isWater())
            return MAP_SHORE_CON;
        else if (fromShip && !tileUnderneath->isWater())
            return MAP_SHIPSHOR_CON;        
    }

    if (tileMap.find(groundTile) != tileMap.end())
        return tileMap[groundTile];    

    return MAP_BRICK_CON;
}

bool CombatMap::isDungeonRoom() const {
    return dungeonRoom;
}

bool CombatMap::isAltarRoom() const {
    return (altarRoom != VIRT_NONE) ? true : false;
}

bool CombatMap::isContextual() const {
    return contextual;
}

BaseVirtue CombatMap::getAltarRoom() const {
    return altarRoom;
}

void CombatMap::setAltarRoom(BaseVirtue ar) {
    altarRoom = ar;
}

void CombatMap::setDungeonRoom(bool d) {
    dungeonRoom = d;
}

void CombatMap::setContextual(bool c) {
    contextual = c;
}
