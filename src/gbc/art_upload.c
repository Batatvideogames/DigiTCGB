#include <gb/gb.h>
#include "gfx.h"

volatile uint8_t gfx_commit_pending;
uint8_t gfx_next_lcdc;
extern uint8_t staged_oam[160];
extern uint16_t staged_bg_colors[8], staged_obj_colors[8];
extern uint8_t gfx_title_page;
static uint8_t title_active, title_blink_frames;
static uint8_t shake_frames;
static int8_t shake_x;

void gfx_shake(void) { shake_frames = 12; }

void gfx_shake_tick(void) {
    uint8_t i;
    int8_t x = 0, shift;
    if (!shake_frames && !shake_x) return;
    if (shake_frames) {
        --shake_frames;
        if (shake_frames > 2) x = (shake_frames / 3) & 1 ? 1 : -1;
    }
    shift = shake_x-x;
    SCX_REG = (uint8_t)x;
    for (i = 0; i < 40; ++i) if (shadow_OAM[i].y) shadow_OAM[i].x += shift;
    shake_x = x;
    refresh_OAM();
}

void gfx_vblank(void) __naked {
    __asm
        ld a, (#_gfx_commit_pending)
        or a
        jr nz, 003$
        ld a, (#_title_active)
        or a
        ret z
        ld hl, #_title_blink_frames
        inc (hl)
        ld a, (hl)
        cp #60
        jr z, 001$
        cp #120
        ret nz
        ld (hl), #0
        ld bc, #0x7fff
        jr 002$
001$:
        ld bc, #0x0004
002$:
        ld a, #0xb8
        ldh (#0x68), a
        ld a, c
        ldh (#0x69), a
        ld a, b
        ldh (#0x69), a
        ret
003$:
        xor a
        ld (#_shake_frames), a
        ld (#_shake_x), a
        ldh (#0x43), a
        ld a, #0x82
        ldh (#0x6a), a
        ld a, #0xa3
        ldh (#0x6b), a
        ld a, #0x18
        ldh (#0x6b), a
        ld hl, #_staged_bg_colors
        ld a, #0x90
        ldh (#0x68), a
        .rept 16
        ld a, (hl+)
        ldh (#0x69), a
        .endm
        ld hl, #_staged_obj_colors
        ld a, #0x88
        ldh (#0x6a), a
        .rept 16
        ld a, (hl+)
        ldh (#0x6b), a
        .endm
        ld hl, #_staged_oam
        ld de, #_shadow_OAM
        .rept 160
        ld a, (hl+)
        ld (de), a
        inc de
        .endm
        call _refresh_OAM
        ld a, (#_gfx_next_lcdc)
        ldh (#0x40), a
        ld a, (#_gfx_title_page)
        ld (#_title_active), a
        xor a
        ld (#_title_blink_frames), a
        ld (#_gfx_commit_pending), a
        ret
    __endasm;
}

void gfx_upload_art(uint8_t tile, const uint8_t *data, uint8_t bank, uint8_t object) {
    uint8_t saved = CURRENT_BANK;
    SWITCH_ROM(bank);
    if (object) set_sprite_data(tile,1,data);
    else set_bkg_data(tile,1,data);
    SWITCH_ROM(saved);
}
