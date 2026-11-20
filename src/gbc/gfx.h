#ifndef DIGITCGB_GFX_H
#define DIGITCGB_GFX_H
#include "../core/game.h"
enum { GFX_IDLE, GFX_BATTLE, GFX_ATTACK, GFX_HURT };
void gfx_init(void) DG_BANKED;
void gfx_begin(void) DG_BANKED;
void gfx_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) DG_BANKED;
void gfx_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t ink) DG_BANKED;
void gfx_band(uint8_t y, uint8_t h, uint8_t palette) DG_BANKED;
void gfx_text(uint8_t x, uint8_t y, uint8_t width, const char *s) DG_BANKED;
void gfx_number(uint8_t x, uint8_t y, uint16_t n) DG_BANKED;
void gfx_wrap(uint8_t x, uint8_t y, uint8_t width, uint8_t rows, const char *s) DG_BANKED;
void gfx_portrait(uint8_t def, uint8_t x, uint8_t y, uint8_t facing_left) DG_BANKED;
void gfx_portrait_pose(uint8_t def, uint8_t x, uint8_t y, uint8_t facing_left, uint8_t pose) DG_BANKED;
void gfx_hand_begin(uint8_t patch) DG_BANKED;
void gfx_hand_present(void) DG_BANKED;
void gfx_agumon(uint8_t x, uint8_t y) DG_BANKED;
void gfx_title(void) DG_BANKED;
void gfx_present(void) DG_BANKED;
void gfx_upload_art(uint8_t tile, const uint8_t *data, uint8_t bank, uint8_t object);
extern volatile uint8_t gfx_commit_pending;
extern uint8_t gfx_next_lcdc;
void gfx_vblank(void);
void gfx_shake(void);
void gfx_shake_tick(void);
void gfx_cursor(uint8_t x, uint8_t y) DG_BANKED;
void gfx_cursor_present(void) DG_BANKED;
extern uint8_t gfx_overflow;
extern uint16_t gfx_tiles_used, gfx_frames;
#endif
