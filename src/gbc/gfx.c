#pragma bank 5
#include <gb/gb.h>
#include <gb/cgb.h>
#include <string.h>
#include "gfx.h"
#include "art_data.h"
#include "portrait_data.h"

static uint8_t bitmap[2880], attributes[360], tilemap[360];
static uint8_t buckets[256], next_tile[256], front, map_front;
static const uint8_t *tile_sources[256];
static uint8_t cursor_x, cursor_y, mono[8], pattern[16];
static uint16_t used;
static uint8_t tile_hash, tile_candidate;
typedef struct { uint8_t x, y, w, h, flip, bank, palette; const uint8_t *data, *objects; const uint16_t *colors; uint32_t mask; } Overlay;
static Overlay overlays[2];
static uint8_t overlay_count;
static uint8_t hand_page, hand_bank;
uint8_t staged_oam[160];
uint16_t staged_bg_colors[8], staged_obj_colors[8];
static uint8_t object_count;
uint8_t gfx_overflow;
uint16_t gfx_tiles_used, gfx_frames;
uint8_t gfx_title_page;
static const uint16_t title_palette[] = {RGB(31,31,31),RGB(31,31,31),RGB(4,0,0),RGB(4,0,0)};
static const uint16_t colors[] = {
    RGB(3,5,6),RGB(11,13,13),RGB(29,28,23),RGB(31,31,27),
    RGB(29,28,23),RGB(3,5,6),RGB(8,12,16),RGB(31,31,27),
    RGB(3,5,6),RGB(19,10,7),RGB(29,19,7),RGB(29,28,23),
    RGB(3,5,6),RGB(8,12,16),RGB(14,21,25),RGB(29,28,23),
    RGB(3,5,6),RGB(19,9,13),RGB(28,18,20),RGB(29,28,23)
};
static const uint8_t cursor_tile[32] = {128,0,64,0,32,0,16,0,32,0,64,0,128,0,0,0};
static const uint8_t blank_tile[16] = {0};
static const uint8_t paper_tile[16] = {0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255};
static const uint16_t cursor_palette[] = {RGB(29,28,23),RGB(3,5,6),RGB(3,5,6),RGB(3,5,6)};

void gfx_init(void) DG_BANKED {
    uint8_t i;
    LCDC_REG = 0x07;
    SCX_REG = 0; SCY_REG = 0;
    WX_REG = 7; WY_REG = 144;
    BGP_REG = 0xE4; OBP0_REG = 0xE4; OBP1_REG = 0xE4;
    for (i = 0; i < 2; ++i) {
        VBK_REG = i;
        memset((uint8_t *)0x9800,0,2048);
    }
    VBK_REG = 0;
    set_bkg_data(255,1,paper_tile);
    for (i = 0; i < 18; ++i) {
        ((uint8_t *)0x9800)[i*32+20] = ((uint8_t *)0x9800)[i*32+31] = 255;
        ((uint8_t *)0x9C00)[i*32+20] = ((uint8_t *)0x9C00)[i*32+31] = 255;
    }
    for (i = 0; i < 40; ++i) {
        move_sprite(i,0,0);
        set_sprite_prop(i,0);
        set_sprite_tile(i,126);
    }
    front = 0;
    map_front = 0;
    set_bkg_palette(0,5,colors);
    set_bkg_palette(6,1,agumon_palette);
    species_palettes_init();
    set_sprite_palette(0,1,cursor_palette);
    set_sprite_data(126,2,cursor_tile);
    set_sprite_tile(0,126);
    move_sprite(0,0,0);
}

void gfx_begin(void) DG_BANKED {
    if (hand_page && hand_bank != front) gfx_hand_present();
    memset(bitmap,0,sizeof(bitmap));
    memset(attributes,0,sizeof(attributes));
    overlay_count = 0;
    hand_page = 0;
    gfx_title_page = 0;
    cursor_x = 0; cursor_y = 0;
}

void gfx_cursor(uint8_t x, uint8_t y) DG_BANKED {
    cursor_x = x+8; cursor_y = y+16;
}

void gfx_cursor_present(void) DG_BANKED {
    vsync();
    move_sprite(0,cursor_x,cursor_y);
    refresh_OAM();
    ++gfx_frames;
}

void gfx_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t ink) DG_BANKED {
    uint8_t a, b, left, right, i;
    uint8_t *row;
    if (!w || !h || x >= 160 || y >= 144) return;
    if (w > 160-x) w = 160-x;
    if (h > 144-y) h = 144-y;
    a = x >> 3; b = (x+w-1) >> 3;
    left = 255 >> (x&7); right = 255 << (7-((x+w-1)&7));
    row = bitmap+(uint16_t)y*20;
    while (h--) {
        if (a == b) {
            if (ink) row[a] |= left & right;
            else row[a] &= ~(left & right);
        } else {
            if (ink) { row[a] |= left; row[b] |= right; }
            else { row[a] &= ~left; row[b] &= ~right; }
            for (i = a+1; i < b; ++i) row[i] = ink ? 255 : 0;
        }
        row += 20;
    }
}

void gfx_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h) DG_BANKED {
    gfx_rect(x,y,w,1,1);
    gfx_rect(x,y+h-1,w,1,1);
    gfx_rect(x,y,1,h,1);
    gfx_rect(x+w-1,y,1,h,1);
    if (w > 6 && h > 6) {
        gfx_rect(x+2,y+2,w-4,1,1);
        gfx_rect(x+2,y+h-3,w-4,1,1);
        gfx_rect(x+2,y+2,1,h-4,1);
        gfx_rect(x+w-3,y+2,1,h-4,1);
    }
}

void gfx_band(uint8_t y, uint8_t h, uint8_t palette) DG_BANKED {
    memset(attributes + (uint16_t)(y >> 3) * 20,palette,(uint16_t)(h >> 3) * 20);
}

static void glyph(uint8_t x, uint8_t y, uint8_t ch) OLDCALL __naked {
    x; y; ch;
    __asm
        ldhl sp, #4
        ld a, (hl)
        cp #97
        jr c, 001$
        cp #123
        jr nc, 001$
        sub #32
001$:
        sub #32
        cp #64
        jr c, 002$
        ld a, #31
002$:
        ld e, a
        ld d, #0
        ld l, a
        ld h, d
        add hl, hl
        add hl, hl
        add hl, hl
        ld a, l
        sub e
        ld l, a
        ld a, h
        sbc d
        ld h, a
        ld de, #_native_font
        add hl, de
        push hl
        ldhl sp, #4
        ld a, (hl+)
        ld c, a
        and #7
        rrca
        rrca
        rrca
        ld b, a
        ld a, (hl)
        ld l, a
        ld h, #0
        add hl, hl
        add hl, hl
        ld e, l
        ld d, h
        add hl, hl
        add hl, hl
        add hl, de
        ld a, c
        srl a
        srl a
        srl a
        add l
        ld l, a
        jr nc, 003$
        inc h
003$:
        ld de, #_bitmap
        add hl, de
        ld e, l
        ld d, h
        pop hl
        ld c, #7
004$:
        ld a, (hl+)
        or b
        push hl
        push bc
        ld c, a
        ld b, #0
        ld hl, #_font_left
        add hl, bc
        ld a, (de)
        or (hl)
        ld (de), a
        bit 7, c
        jr z, 005$
        ld hl, #_font_right
        add hl, bc
        inc de
        ld a, (de)
        or (hl)
        ld (de), a
        dec de
005$:
        ld a, e
        add #20
        ld e, a
        jr nc, 006$
        inc d
006$:
        pop bc
        pop hl
        dec c
        jr nz, 004$
        ret
    __endasm;
}

void gfx_text(uint8_t x, uint8_t y, uint8_t width, const char *s) DG_BANKED {
    uint8_t count;
    if (x >= 160 || y > 137) return;
    if (width > 160-x) width = 160-x;
    count = (width + 1) / 6;
    while (*s && count) {
        glyph(x,y,count == 1 && s[1] ? '>' : *s);
        ++s; x += 6; --count;
    }
}

void gfx_number(uint8_t x, uint8_t y, uint16_t n) DG_BANKED {
    char buffer[6];
    uint8_t i = 5;
    buffer[i] = 0;
    do { buffer[--i] = '0' + n % 10; n /= 10; } while (n && i);
    gfx_text(x,y,160-x,buffer+i);
}

void gfx_wrap(uint8_t x, uint8_t y, uint8_t width, uint8_t rows, const char *s) DG_BANKED {
    uint8_t max = (width+1)/6, n, end, i;
    while (*s && rows--) {
        while (*s == ' ') ++s;
        n = 0;
        while (s[n] && s[n] != '\n' && n < max) ++n;
        end = n;
        if (n == max && s[n] && s[n] != ' ' && s[n] != '\n') {
            for (i = n; i; --i) if (s[i] == ' ') { end = i; break; }
        }
        for (i = 0; i < end; ++i) glyph(x+i*6,y,s[i]);
        s += end;
        if (*s == '\n') ++s;
        y += 10;
    }
}

static void overlay(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal, const uint8_t *data) {
    uint8_t row;
    Overlay *o;
    if (overlay_count == 2) { gfx_overflow = 1; return; }
    o = &overlays[overlay_count++];
    o->x = x >> 3; o->y = y >> 3; o->w = w; o->h = h; o->data = data; o->flip = pal & 32;
    o->bank = 13; o->palette = pal & 7; o->objects = 0; o->colors = 0; o->mask = 0;
    for (row = 0; row < h; ++row) memset(attributes+(uint16_t)(o->y+row)*20+o->x,pal,w);
}

void gfx_portrait(uint8_t def, uint8_t x, uint8_t y, uint8_t facing_left) DG_BANKED {
    gfx_portrait_pose(def,x,y,facing_left,GFX_BATTLE);
}

void gfx_portrait_pose(uint8_t def, uint8_t x, uint8_t y, uint8_t facing_left, uint8_t pose) DG_BANKED {
    const CardDef *c;
    uint8_t art, pal, i;
    if (def == DG_NONE) return;
    if (overlay_count == 2) { gfx_overflow = 1; return; }
    c = &dg_cards[def];
    if (c->kind == DG_TAMER) pose = GFX_IDLE;
    if (c->kind != DG_OPTION) {
        for (i = 0; i < SPECIES_PORTRAIT_COUNT; ++i) {
            const SpeciesPortrait *portrait = &species_portraits[i];
            if (portrait->pose == pose && !strcmp(c->name,portrait->species)) {
                Overlay *o;
                pal = overlay_count+2;
                overlay(x,y,5,5,pal | (c->kind != DG_TAMER && portrait->facing_left != facing_left ? 32 : 0),portrait->tiles);
                o = &overlays[overlay_count-1];
                o->bank = portrait->bank; o->objects = portrait->objects;
                o->colors = portrait->colors; o->mask = portrait->mask;
                return;
            }
        }
    }
    pal = overlay_count+2;
    if (c->kind == DG_TAMER) art = 1;
    else if (c->kind == DG_OPTION) art = 2;
    else art = 0;
    if (!facing_left && (c->kind == DG_DIGIMON || c->kind == DG_EGG)) pal |= 32;
    overlay(x,y,5,5,pal,native_portraits+(uint16_t)art*400);
    overlays[overlay_count-1].colors = colors+(c->color ? 12 : 8);
}

static uint8_t hand_dynamic(uint8_t x, uint8_t y) {
    return (!y && x >= 14 && x < 19) ||
        (x >= 9 && (y == 2 || y == 3 || y == 4 || y == 6 || y == 7)) ||
        (x >= 1 && x < 6 && y >= 2 && y < 7);
}

void gfx_hand_begin(uint8_t patch) DG_BANKED {
    hand_page = 1;
    if (patch) {
        uint8_t y;
        overlay_count = 0;
        gfx_rect(112,0,40,8,0);
        for (y = 2; y < 8; ++y) if (y != 5) gfx_rect(72,y*8,88,8,0);
    }
}

void gfx_agumon(uint8_t x, uint8_t y) DG_BANKED {
    overlay(x,y,9,10,6,agumon_tiles);
}

void gfx_title(void) DG_BANKED {
    gfx_title_page = 1;
    gfx_band(0,144,7);
    set_bkg_palette(7,1,title_palette);
}

static uint8_t add_tile(uint8_t hash, const uint8_t *source, const uint8_t *data) {
    uint8_t tile;
    if (used >= 255) { gfx_overflow = 1; return 0; }
    tile = (uint8_t)used++;
    tile_sources[tile] = source;
    next_tile[tile] = buckets[hash];
    buckets[hash] = tile;
    set_bkg_data(tile,1,data);
    return tile;
}

static uint8_t match_mono(const uint8_t *bits) __naked {
    bits;
    __asm
        ld hl, #_mono
        ld b, #0
        .rept 8
        ld a, (de)
        ld (hl+), a
        xor b
        rlca
        ld b, a
        ld a, e
        add #20
        ld e, a
        ld a, d
        adc #0
        ld d, a
        .endm
        ld a, b
        ld (#_tile_hash), a
        ld e, a
        ld d, #0
        ld hl, #_buckets
        add hl, de
        ld a, (hl)
001$:
        cp #255
        ret z
        ld (#_tile_candidate), a
        ld l, a
        ld h, #0
        add hl, hl
        ld de, #_tile_sources
        add hl, de
        ld a, (hl+)
        ld e, a
        ld d, (hl)
        ld hl, #_mono
        .rept 8
        ld a, (de)
        cp (hl)
        jp nz, 002$
        inc hl
        ld a, e
        add #20
        ld e, a
        ld a, d
        adc #0
        ld d, a
        .endm
        ld a, (#_tile_candidate)
        ret
002$:
        ld a, (#_tile_candidate)
        ld e, a
        ld d, #0
        ld hl, #_next_tile
        add hl, de
        ld a, (hl)
        jp 001$
    __endasm;
}

static uint8_t mono_tile(const uint8_t *bits) {
    uint8_t row, tile = match_mono(bits);
    if (tile != 255) return tile;
    for (row = 0; row < 8; ++row) { pattern[row*2] = 0; pattern[row*2+1] = ~mono[row]; }
    return add_tile(tile_hash,bits,pattern);
}

static void mono_pattern(const uint8_t *bits) {
    uint8_t row;
    for (row = 0; row < 8; ++row, bits += 20) { pattern[row*2] = 0; pattern[row*2+1] = ~*bits; }
}

static void stage_objects(uint8_t bank) {
    uint8_t i, x, y;
    memset(staged_oam,0,sizeof(staged_oam));
    object_count = 1;
    for (i = 0; i < overlay_count; ++i) {
        const Overlay *o = &overlays[i];
        if (!o->objects) continue;
        for (y = 0; y < o->h; y += 2) for (x = 0; x < o->w; ++x) {
            uint8_t source = y*o->w+(o->flip ? o->w-1-x : x);
            uint8_t *sprite;
            if (!(o->mask & (1UL << source)) && (y+1 == o->h || !(o->mask & (1UL << (source+o->w))))) continue;
            if (object_count == 40) { gfx_overflow = 1; return; }
            gfx_upload_art(object_count*2,o->objects+(uint16_t)source*16,o->bank,1);
            if (y+1 < o->h) gfx_upload_art(object_count*2+1,o->objects+(uint16_t)(source+o->w)*16,o->bank,1);
            else set_sprite_data(object_count*2+1,1,blank_tile);
            sprite = staged_oam+(uint16_t)object_count*4;
            sprite[0] = (o->y+y)*8+16;
            sprite[1] = (o->x+x)*8+8;
            sprite[2] = object_count*2;
            sprite[3] = (o->palette-1) | o->flip | (bank << 3);
            ++object_count;
        }
    }
}

static void stage_art(void) {
    uint8_t i;
    for (i = 0; i < overlay_count; ++i) {
        const Overlay *o = &overlays[i];
        if (o->colors) {
            memcpy(staged_bg_colors+(o->palette-2)*4,o->colors,8);
            if (o->objects) memcpy(staged_obj_colors+(o->palette-2)*4,o->colors+4,8);
        } else if (o->palette == 6) set_bkg_palette(6,1,agumon_palette);
    }
    staged_oam[0] = cursor_y; staged_oam[1] = cursor_x;
    staged_oam[2] = 126; staged_oam[3] = 0;
}

static void present_art(uint8_t map_back) {
    gfx_next_lcdc = (LCDC_REG & ~0x08) | (map_back << 3);
    gfx_commit_pending = 1;
    if (LCDC_REG & 0x80) {
        do { vsync(); } while (gfx_commit_pending);
    } else gfx_vblank();
}

static void write_map(uint8_t *map, const uint8_t *data) {
    uint8_t row;
    for (row = 0; row < 18; ++row, map += 32, data += 20) set_data(map,data,20);
}

void gfx_present(void) DG_BANKED {
    uint8_t back = front ^ 1, map_back = map_front ^ 1, tx, ty, i;
    uint16_t at = 0;
    const uint8_t *bits = bitmap;
    uint8_t *dest = tilemap;
    uint8_t *map = map_back ? (uint8_t *)0x9C00 : (uint8_t *)0x9800;
    used = 0;
    memset(buckets,255,sizeof(buckets));
    VBK_REG = back;
    for (ty = 0; ty < 18; ++ty) {
        for (tx = 0; tx < 20; ++tx) {
            if (hand_page && hand_dynamic(tx,ty)) {
                mono_pattern(bits);
                if (used >= 255) { gfx_overflow = 1; *dest++ = 0; }
                else { *dest++ = (uint8_t)used; set_bkg_data((uint8_t)used++,1,pattern); }
            } else *dest++ = mono_tile(bits);
            ++bits;
        }
        bits += 140;
    }
    if (back) for (at = 0; at < 360; ++at) attributes[at] |= 8;
    memset(buckets,255,sizeof(buckets));
    for (i = 0; i < overlay_count; ++i) {
        const Overlay *o = &overlays[i];
        at = (uint16_t)o->y*20+o->x;
        for (ty = 0; ty < o->h; ++ty, at += 20-o->w)
            for (tx = 0; tx < o->w; ++tx, ++at) {
                const uint8_t *data = o->data+((uint16_t)ty*o->w+(o->flip ? o->w-1-tx : tx))*16;
                if (hand_page && hand_dynamic(o->x+tx,o->y+ty)) gfx_upload_art(tilemap[at],data,o->bank,0);
                else if (used < 255) {
                    tilemap[at] = (uint8_t)used;
                    gfx_upload_art((uint8_t)used++,data,o->bank,0);
                } else gfx_overflow = 1;
            }
    }
    stage_objects(back);
    stage_art();
    VBK_REG = 0;
    write_map(map,tilemap);
    VBK_REG = 1;
    write_map(map,attributes);
    VBK_REG = 0;
    present_art(map_back);
    map_front = map_back;
    front = back;
    hand_bank = back;
    gfx_tiles_used = used;
    ++gfx_frames;
}

void gfx_hand_present(void) DG_BANKED {
    uint8_t x, y, bank = hand_bank ^ 1, map_back = map_front ^ 1;
    uint16_t at;
    uint8_t *map = map_back ? (uint8_t *)0x9C00 : (uint8_t *)0x9800;
    const Overlay *o = &overlays[0];
    VBK_REG = bank;
    for (y = 0; y < 18; ++y) for (x = 0; x < 20; ++x) {
        const uint8_t *data;
        if (!hand_dynamic(x,y)) continue;
        at = (uint16_t)y*20+x;
        if (x >= 1 && x < 6 && y >= 2 && y < 7) {
            uint8_t col = x-1;
            data = o->data+((uint16_t)(y-2)*5+(o->flip ? 4-col : col))*16;
            gfx_upload_art(tilemap[at],data,o->bank,0);
        } else {
            mono_pattern(bitmap+(uint16_t)y*160+x);
            set_bkg_data(tilemap[at],1,pattern);
        }
        attributes[at] = (attributes[at] & ~8) | (bank << 3);
    }
    stage_objects(bank);
    stage_art();
    VBK_REG = 0;
    write_map(map,tilemap);
    VBK_REG = 1;
    write_map(map,attributes);
    VBK_REG = 0;
    present_art(map_back);
    map_front = map_back;
    hand_bank = bank;
    ++gfx_frames;
}
