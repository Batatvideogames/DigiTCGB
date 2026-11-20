#include <gb/gb.h>
#include "audio.h"
#include "music_data.h"
#include "sfx_data.h"

volatile uint8_t audio_enabled = 0, audio_channels = 15, audio_speed = 8;
volatile uint16_t audio_position;
volatile uint8_t audio_loops;
volatile uint8_t audio_sfx_active;
static const SoundStep *sound_step;
static uint8_t sound_frames, sound_mask;
static uint8_t sound_lead;
static const uint8_t *score;
static uint8_t wait_frames, tempo_phase, click_frames, lead_age, lead_phase;
static uint8_t playing[3];
static uint16_t lead_period;
static const uint8_t wave[] = {0x89,0xBC,0xDD,0xEF,0xEE,0xDD,0xBA,0x98,0x76,0x54,0x32,0x11,0x01,0x12,0x34,0x67};
static const uint8_t drum_period[] = {0,0x65,0x35,0x15,0x55,0x25};
static const uint8_t drum_volume[] = {0,0x91,0x81,0x31,0x71,0x62};
static const uint8_t drum_length[] = {0,40,36,56,40,24};
static const int8_t vibrato[] = {0,1,1,0,0,-1,-1,0};

static void silence(uint8_t ch) {
    switch (ch) {
    case 0: NR12_REG = 0x08; NR14_REG = 0x80; break;
    case 1: NR22_REG = 0x08; NR24_REG = 0x80; break;
    case 2: NR32_REG = 0; break;
    case 3: NR42_REG = 0x08; NR44_REG = 0x80; break;
    }
}

static void tone(uint8_t ch, uint8_t value) {
    uint8_t note = value & 127;
    uint16_t period;
    if (sound_mask & (1 << ch)) return;
    if (!note || !(audio_channels & (1 << ch))) { silence(ch); return; }
    period = audio_periods[note-36+(ch == 2 ? 12 : 0)];
    switch (ch) {
    case 0:
        lead_period = period; lead_age = 0; lead_phase = 0;
        NR10_REG = 0;
        NR11_REG = value & 128 ? 0x80 : 0x40;
        NR12_REG = 0xA0;
        NR13_REG = (uint8_t)period;
        NR14_REG = 0x80 | (uint8_t)(period >> 8);
        break;
    case 1:
        NR21_REG = value & 128 ? 0x00 : 0x40;
        NR22_REG = value & 128 ? 0x60 : 0x42;
        NR23_REG = (uint8_t)period;
        NR24_REG = 0x80 | (uint8_t)(period >> 8);
        break;
    case 2:
        NR32_REG = 0x40;
        NR33_REG = (uint8_t)period;
        NR34_REG = 0x80 | (uint8_t)(period >> 8);
        break;
    }
}

static void drum(uint8_t value) {
    if (!(audio_channels & 8) || click_frames || (sound_mask & 8)) return;
    NR41_REG = drum_length[value];
    NR42_REG = drum_volume[value];
    NR43_REG = drum_period[value];
    NR44_REG = 0xC0;
}

static void sound_finish(void) {
    uint8_t ch, mask = sound_mask;
    sound_mask = 0; audio_sfx_active = SFX_NONE; sound_frames = 0;
    for (ch = 0; ch < 4; ++ch) if (mask & (1 << ch)) {
        silence(ch);
        if (ch < 3 && audio_enabled) tone(ch,playing[ch]);
    }
}

static void sound_next(void) {
    const SoundStep *s = sound_step++;
    uint16_t period;
    if (!s->frames) { sound_finish(); return; }
    sound_frames = s->frames;
    if (s->note) {
        period = audio_periods[s->note-36];
        NR21_REG = s->duty; NR22_REG = s->volume;
        NR23_REG = (uint8_t)period; NR24_REG = 0x80 | (uint8_t)(period >> 8);
    } else silence(1);
    if (s->noise_volume) {
        NR41_REG = 0; NR42_REG = s->noise_volume;
        NR43_REG = s->noise; NR44_REG = 0x80;
    } else silence(3);
    if (sound_mask & 1) {
        if (s->harmony) {
            period = audio_periods[s->harmony-36];
            NR10_REG = 0; NR11_REG = 0x80; NR12_REG = 0x23;
            NR13_REG = (uint8_t)period; NR14_REG = 0x80 | (uint8_t)(period >> 8);
        } else silence(0);
    }
    if (sound_mask & 4) {
        NR32_REG = s->wave_level;
        if (s->lead && s->lead != sound_lead) {
            period = audio_periods[s->lead-24];
            NR33_REG = (uint8_t)period;
            NR34_REG = 0x80 | (uint8_t)(period >> 8);
        }
        sound_lead = s->lead;
    }
}

void audio_sfx(uint8_t effect) {
    uint8_t ch;
    if (effect >= SFX_COUNT) return;
    __critical {
        if (!effect) sound_finish();
        else if (sound_priorities[effect] >= sound_priorities[audio_sfx_active]) {
            sound_finish();
            audio_sfx_active = effect; click_frames = 0;
            sound_mask = effect >= SFX_WIN ? 15 : 10;
            if (effect >= SFX_WIN) {
                if (effect != SFX_GAIA) audio_enabled = 0;
                for (ch = 0; ch < 4; ++ch) silence(ch);
            }
            sound_lead = 0;
            sound_step = sound_effects[effect];
            sound_next();
        }
    }
}

void audio_init(void) {
    uint8_t i;
    NR52_REG = 0;
    NR52_REG = 0x80;
    NR50_REG = 0x55;
    NR51_REG = 0xFF;
    NR30_REG = 0;
    for (i = 0; i < 16; ++i) ((volatile uint8_t *)0xFF30)[i] = wave[i];
    NR30_REG = 0x80;
    score = audio_song;
    for (i = 0; i < 4; ++i) silence(i);
}

void audio_start(void) {
    __critical {
        audio_enabled = 0;
        score = audio_song;
        wait_frames = 0; tempo_phase = 0; click_frames = 0;
        audio_position = 0; audio_loops = 0;
        playing[0] = playing[1] = playing[2] = 0;
        audio_play(1);
    }
}

void audio_play(uint8_t enabled) {
    uint8_t ch;
    __critical {
        sound_mask = 0; audio_sfx_active = SFX_NONE; sound_frames = 0;
        audio_enabled = enabled;
        for (ch = 0; ch < 4; ++ch) silence(ch);
        click_frames = 0;
        if (enabled) for (ch = 0; ch < 3; ++ch) tone(ch,playing[ch]);
    }
}

void audio_toggle(uint8_t channel) {
    __critical {
        audio_channels ^= (1 << channel);
        if (!(audio_channels & (1 << channel))) silence(channel);
        else if (channel < 3 && audio_enabled) tone(channel,playing[channel]);
    }
}

void audio_click(void) {
    __critical {
        if (!audio_sfx_active) {
            NR41_REG = 0x3C;
            NR42_REG = 0x31;
            NR43_REG = 0x25;
            NR44_REG = 0xC0;
            click_frames = 2;
        }
    }
}

static void advance(void) {
    uint8_t mask, ch, value;
    if (wait_frames) --wait_frames;
    if (!wait_frames) {
        wait_frames = *score++;
        mask = *score++;
        if (mask & 128) {
            score = audio_song;
            audio_position = 0;
            ++audio_loops;
            wait_frames = *score++;
            mask = *score++;
        }
        for (ch = 0; ch < 4; ++ch) {
            if (mask & (1 << ch)) {
                value = *score++;
                if (ch == 3) drum(value);
                else { playing[ch] = value; tone(ch,value); }
            }
        }
    }
    ++audio_position;
}

void audio_tick(void) {
    uint8_t saved_bank;
    uint16_t period;
    if (click_frames) --click_frames;
    if (audio_sfx_active && sound_frames && !--sound_frames) sound_next();
    if (!audio_enabled || audio_sfx_active >= SFX_WIN) return;
    if (playing[0] && (audio_channels & 1) && !(sound_mask & 1)) {
        if (lead_age < 18) ++lead_age;
        else {
            lead_phase = (lead_phase+1)&31;
            period = lead_period+vibrato[lead_phase>>2];
            NR13_REG = (uint8_t)period;
            NR14_REG = (uint8_t)(period>>8);
        }
    }
    saved_bank = CURRENT_BANK;
    SWITCH_ROM(AUDIO_SONG_BANK);
    tempo_phase += 8;
    while (tempo_phase >= audio_speed) {
        tempo_phase -= audio_speed;
        advance();
    }
    SWITCH_ROM(saved_bank);
}
