#include <gb/gb.h>
#include <gb/cgb.h>
#include "ui.h"
#include "gfx.h"
#include "audio.h"

static volatile uint8_t key_queue[16];
volatile uint8_t input_read, input_write, ui_busy;
static uint8_t last_keys, held_frames;

static void vblank_tick(void) {
    gfx_vblank();
    gfx_shake_tick();
    uint8_t held = joypad();
    uint8_t edges = held & (uint8_t)~last_keys;
    uint8_t next = (input_write+1)&15;
    if (held != last_keys || !(held & 15)) held_frames = 0;
    else if (held_frames < 18) ++held_frames;
    if (held_frames == 18 && !ui_busy && input_read == input_write) {
        edges |= held & 15;
        held_frames = 13;
    }
    if (edges && next != input_read) {
        key_queue[input_write] = edges;
        input_write = next;
    }
    last_keys = held;
    audio_tick();
}

void main(void) {
    uint8_t keys;
    DISPLAY_OFF;
    audio_init();
    add_VBL(vblank_tick);
    cpu_fast();
    ui_busy = 1;
    ui_init();
    ui_busy = 0;
    SHOW_BKG;
    DISPLAY_ON;
    audio_sfx(SFX_BOOT);
    while (1) {
        vsync();
        __critical {
            keys = 0;
            if (input_read != input_write) {
                keys = key_queue[input_read];
                input_read = (input_read+1)&15;
            }
        }
        ui_busy = 1;
        ui_frame(keys);
        ui_busy = 0;
    }
}
