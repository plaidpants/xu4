/*
 * $Id$
 */

#include <stdlib.h>

#include "location.h"

#include "annotation.h"
#include "context.h"
#include "combat.h"
#include "game.h"
#include "monster.h"
#include "object.h"
#include "savegame.h"

Location *locationPush(Location *stack, Location *loc);
Location *locationPop(Location **stack);

/**
 * Add a new location to the stack, or 
 * start a new stack if 'prev' is NULL
 */
Location *locationNew(int x, int y, int z, Map *map, int viewmode, LocationContext ctx,
                      FinishTurnCallback finishTurnCallback, MoveCallback moveCallback,
                      TileAt tileAtCallback, Location *prev) {
    Location *newLoc = (Location *)malloc(sizeof(Location));

    newLoc->x = x;
    newLoc->y = y;
    newLoc->z = z;
    newLoc->map = map;
    newLoc->viewMode = viewmode;
    newLoc->context = ctx;
    newLoc->finishTurn = finishTurnCallback;
    newLoc->move = moveCallback;
    newLoc->tileAt = tileAtCallback;
    
    return locationPush(prev, newLoc);    
}

/**
 * Returns the visible tile at the given point on a map.  This
 * includes visual-only annotations like moongates and attack icons.
 */
unsigned char locationVisibleTileAt(Location *location, int x, int y, int z, int *focus) {
    unsigned char tile;
    const Annotation *a = annotationAt(x, y, z, location->map->id);
    const Object *obj = mapObjectAt(location->map, x, y, z);
    
    /* Do not return objects for VIEW_GEM mode, show only the avatar and tiles */
    if (location->viewMode == VIEW_GEM) {
        *focus = 0;
        if ((location->map->flags & SHOW_AVATAR) && location->x == x && location->y == y)            
            return (unsigned char)c->saveGame->transport;        
        else return mapGetTileFromData(location->map, x, y, z);
    }
    
    /* draw objects -- visual-only annotations go first */
    else if (a && a->visual) {
        *focus = 0;
        tile = a->tile;
    }        
    /* then the avatar is drawn (unless on a ship) */
    else if ((location->map->flags & SHOW_AVATAR) && (c->transportContext != TRANSPORT_SHIP) && 
        location->x == x && location->y == y) {
        *focus = 0;
        tile = (unsigned char)c->saveGame->transport;
    }
    /* then permanent annotations */
    else if (a) {
        *focus = 0;
        tile = a->tile;
    }
    /* then camouflaged monsters that have a disguise */
    else if (obj && (obj->objType == OBJECT_MONSTER) && !obj->isVisible && (obj->monster->camouflageTile > 0)) {
        *focus = obj->hasFocus;
        tile = obj->monster->camouflageTile;
    }        
    /* then visible monsters */
    else if (obj && (obj->objType != OBJECT_UNKNOWN) && obj->isVisible) {
        *focus = obj->hasFocus;
        tile = obj->tile;
    }
    /* then other visible objects */
    else if (obj && obj->isVisible) {
        *focus = obj->hasFocus;
        tile = obj->tile;
    }
    /* then the party's ship (because twisters and whirlpools get displayed on top of ships) */
    else if ((location->map->flags & SHOW_AVATAR) && location->x == x && location->y == y) {
        *focus = 0;
        tile = (unsigned char)c->saveGame->transport;
    }
    /* then the base tile */
    else {
        *focus = 0;
        tile = mapGetTileFromData(location->map, x, y, z);
        if (a)
            tile = a->tile;
    }
    
    return tile;
}

/**
 * Returns the current coordinates of the location given:
 *     If in combat - returns the coordinates of party member with focus
 *     If elsewhere - returns the coordinates of the avatar
 */
int locationGetCurrentPosition(Location *location, int *x, int *y, int *z) {
    if (c->location->context & CTX_COMBAT) {
        *x = combatInfo.party[FOCUS].obj->x;
        *y = combatInfo.party[FOCUS].obj->y;
        *z = combatInfo.party[FOCUS].obj->z;
    }
    else {
        *x = location->x;
        *y = location->y;
        *z = location->z;
    }

    return 1;
}

/**
 * Pop a location from the stack and free the memory
 */
void locationFree(Location **stack) {
    free(locationPop(stack));
}

/**
 * Push a location onto the stack
 */
Location *locationPush(Location *stack, Location *loc) {
    loc->prev = stack;
    return loc;
}

/**
 * Pop a location off the stack
 */
Location *locationPop(Location **stack) {
    Location *loc = *stack;    
    *stack = (*stack)->prev;
    loc->prev = NULL;
    return loc;
}
