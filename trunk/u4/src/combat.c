/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "combat.h"
#include "context.h"
#include "ttype.h"
#include "map.h"
#include "object.h"
#include "annotation.h"
#include "event.h"
#include "savegame.h"
#include "game.h"
#include "area.h"
#include "monster.h"
#include "screen.h"
#include "names.h"

extern Map brick_map;
extern Map bridge_map;
extern Map brush_map;
extern Map camp_map;
extern Map dng0_map;
extern Map dng1_map;
extern Map dng2_map;
extern Map dng3_map;
extern Map dng4_map;
extern Map dng5_map;
extern Map dng6_map;
extern Map dungeon_map;
extern Map forest_map;
extern Map grass_map;
extern Map hill_map;
extern Map inn_map;
extern Map marsh_map;
extern Map shipsea_map;
extern Map shipship_map;
extern Map shipshor_map;
extern Map shore_map;
extern Map shorship_map;

int focus;
Object *party[8];
Object *monsters[AREA_MONSTERS];

int combatBaseKeyHandler(int key, void *data);
int combatIsWon(void);
int combatIsLost(void);
void combatEnd(void);
int movePartyMember(Direction dir, int member);

void combatBegin(unsigned char partytile, unsigned short transport) {
    int i;
    int nmonsters;

    annotationClear();

    c = gameCloneContext(c);

    gameSetMap(c, getCombatMapForTile(partytile, transport), 1);
    musicPlay();

    for (i = 0; i < c->saveGame->members; i++)
        party[i] = mapAddObject(c->map, tileForClass(c->saveGame->players[i].klass), tileForClass(c->saveGame->players[i].klass), c->map->area->player_start[i].x, c->map->area->player_start[i].y);
    for (; i < 8; i++)
        party[i] = NULL;
    focus = 0;
    party[focus]->hasFocus = 1;

    nmonsters = (rand() % (AREA_MONSTERS - 1)) + 1;
    for (i = 0; i < nmonsters; i++)
        monsters[i] = mapAddObject(c->map, ORC_TILE, ORC_TILE + 1, c->map->area->monster_start[i].x, c->map->area->monster_start[i].y);
    for (; i < AREA_MONSTERS; i++)
        monsters[i] = NULL;

    eventHandlerPushKeyHandler(&combatBaseKeyHandler);
}


Map *getCombatMapForTile(unsigned char partytile, unsigned short transport) {
    int i;
    static const struct {
        unsigned char tile;
        Map *map;
    } tileToMap[] = {
        { SWAMP_TILE, &marsh_map },
        { GRASS_TILE, &grass_map },
        { BRUSH_TILE, &brush_map },
        { FOREST_TILE, &forest_map },
        { HILLS_TILE, &hill_map },
        { BRIDGE_TILE, &bridge_map },
        { NORTHBRIDGE_TILE, &bridge_map },
        { SOUTHBRIDGE_TILE, &bridge_map },
        { CHEST_TILE, &brick_map },
        { BRICKFLOOR_TILE, &brick_map },
        { MOONGATE0_TILE, &grass_map },
        { MOONGATE1_TILE, &grass_map },
        { MOONGATE2_TILE, &grass_map },
        { MOONGATE3_TILE, &grass_map }
    };
    
    for (i = 0; i < sizeof(tileToMap) / sizeof(tileToMap[0]); i++) {
        if (tileToMap[i].tile == partytile)
            return tileToMap[i].map;
    }

    return &brick_map;
}

int combatBaseKeyHandler(int key, void *data) {
    int valid = 1;

    switch (key) {
    case U4_UP:
        movePartyMember(DIR_NORTH, focus);
        break;

    case U4_DOWN:
        movePartyMember(DIR_SOUTH, focus);
        break;

    case U4_LEFT:
        movePartyMember(DIR_WEST, focus);
        break;

    case U4_RIGHT:
        movePartyMember(DIR_EAST, focus);
        break;

    case U4_ESC:
        eventHandlerPopKeyHandler();
        combatEnd();
        break;
        
    default:
        valid = 0;
        break;
    }

    if (combatIsWon() || combatIsLost()) {
        eventHandlerPopKeyHandler();
        combatEnd();
    }
    else if (valid) {
        if (party[focus])
            party[focus]->hasFocus = 0;
        do {
            focus++;
            if (focus >= c->saveGame->members)
                focus = 0;
        } while (!party[focus]);
        party[focus]->hasFocus = 1;
    }

    return valid;
}

/**
 * Returns true if the player has won.
 */
int combatIsWon() {
    int i, activeMonsters;

    activeMonsters = 0;
    for (i = 0; i < AREA_MONSTERS; i++) {
        if (monsters[i])
            activeMonsters++;
    }

    return activeMonsters == 0;
}

/**
 * Returns true if the player has lost.
 */
int combatIsLost() {
    int i, activePlayers;

    activePlayers = 0;
    for (i = 0; i < c->saveGame->members; i++) {
        if (party[i])
            activePlayers++;
    }

    return activePlayers == 0;
}

void combatEnd() {
    if (c->parent != NULL) {
        Context *t = c;
        annotationClear();
        mapClearObjects(c->map);
        c->parent->saveGame->x = c->saveGame->dngx;
        c->parent->saveGame->y = c->saveGame->dngy;
        c->parent->line = c->line;
        c->parent->moonPhase = c->moonPhase;
        c->parent->windDirection = c->windDirection;
        c->parent->windCounter = c->windCounter;
        c = c->parent;
        c->col = 0;
        free(t);
                
        musicPlay();
    }
    
    gameFinishTurn();
}

int movePartyMember(Direction dir, int member) {
    int result = 1;
    int newx, newy;

    newx = party[member]->x;
    newy = party[member]->y;
    dirMove(dir, &newx, &newy);

    screenMessage("%s\n", getDirectionName(dir));

    if (MAP_IS_OOB(c->map, newx, newy)) {
        mapRemoveObject(c->map, party[member]);
        party[member] = NULL;
        return result;
    }

#if 0
    if (!gameCanMoveOntoTile(c->map, newx, newy)) {
        screenMessage("Blocked!\n");
        result = 0;
        goto done;
    }
#endif

    party[member]->x = newx;
    party[member]->y = newy;

 done:

    return result;
}
