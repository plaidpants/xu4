/*
 * $Id$
 */

#ifndef GAME_H
#define GAME_H

struct _Context;

typedef struct AlphaActionInfo {
    char lastValidLetter;
    int (*handleAlpha)(int, void *);
    const char *prompt;
    void *data;
} AlphaActionInfo;

typedef struct DirectedActionInfo {
    int (*handleAtCoord)(int, int);
    int range;
    int (*blockedPredicate)(unsigned char tile);
    const char *failedMessage;
} DirectedActionInfo;

void gameUpdateScreen(void);
int gameBaseKeyHandler(int key, void *data);
int gameGetPlayerNoKeyHandler(int key, void *data);
int gameGetDirectionKeyHandler(int key, void *data);
int gameZtatsKeyHandler(int key, void *data);
int gameZtatsKeyHandler2(int key, void *data);
int gameSpecialCmdKeyHandler(int key, void *data);
void gameTimer(void);
void gameFinishTurn(void);
void gameLostEighth(int eighths);
struct _Context *gameCloneContext(struct _Context *ctx);

#endif
