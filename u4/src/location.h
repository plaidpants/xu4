/*
 * $Id$
 */

#ifndef LOCATION_H
#define LOCATION_H

#include "map.h"

typedef enum {
    CTX_WORLDMAP    = 0x0001,
    CTX_COMBAT      = 0x0002,
    CTX_CITY        = 0x0004,
    CTX_DUNGEON     = 0x0008
} LocationContext;

#define CTX_ANY         (0xffff)
#define CTX_NORMAL      (CTX_WORLDMAP | CTX_CITY)
#define CTX_NON_COMBAT  (CTX_ANY & ~CTX_COMBAT)

typedef void (*FinishTurnCallback)(void);

typedef struct _Location {
    int x;
    int y;
    int z;	
	Map *map;
    int viewMode;
    LocationContext context;
    FinishTurnCallback finishTurn;
    struct _Location *prev;
} Location;

Location *locationNew(int x, int y, int z, Map *map, int viewmode, LocationContext ctx, FinishTurnCallback callback, Location *prev);
void locationFree(Location **stack);

#endif
