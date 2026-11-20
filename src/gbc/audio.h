#ifndef DIGITCGB_AUDIO_H
#define DIGITCGB_AUDIO_H
#include <stdint.h>
enum { SFX_NONE, SFX_PLAY, SFX_ATTACK, SFX_BLOCK, SFX_EVOLVE, SFX_SECURITY, SFX_DELETE, SFX_WIN, SFX_LOSE, SFX_GAIA, SFX_BOOT, SFX_COUNT };
void audio_init(void);
void audio_start(void);
void audio_tick(void);
void audio_play(uint8_t enabled);
void audio_toggle(uint8_t channel);
void audio_click(void);
void audio_sfx(uint8_t effect);
extern volatile uint8_t audio_sfx_active;
extern volatile uint8_t audio_enabled, audio_channels, audio_speed;
extern volatile uint16_t audio_position;
extern volatile uint8_t audio_loops;
#endif
