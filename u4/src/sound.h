/*
 * $Id$
 */

#ifndef SOUND_H
#define SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOUND_WALK,
    SOUND_BLOCKED,
    SOUND_ERROR,
    SOUND_CANNON,
    SOUND_MISSED,
    SOUND_MONSTERATTACK,
    SOUND_RUMBLE,
    SOUND_PLAYERHIT,
    SOUND_MAGIC,
    SOUND_LBHEAL,
    SOUND_WHIRLPOOL,
    SOUND_STORM,
    SOUND_MOONGATE,
    SOUND_FLEE,
    SOUND_MAX
} Sound;

int soundInit(void);
void soundDelete(void);
void soundPlay(Sound sound);

#ifdef __cplusplus
}
#endif

#endif /* SOUND_H */