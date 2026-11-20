#pragma bank 6
#include <gb/gb.h>
#include <string.h>
#include "ui_internal.h"
#include "audio.h"
#include "gfx.h"

Game digi_game;
uint8_t ui_screen, ui_auto, title_selection, command, browse;
uint8_t help_open;
uint8_t inspector_owner, inspector_root, inspector_card, card_page, card_return;
uint8_t selected_card, action_mode, log_scroll, board_owner;
uint8_t board_selection[2], event_index, event_count;
uint8_t event_attack_target;
uint8_t evolution_drew, evolution_draw_card;
LogEntry ui_events[12];
static uint8_t event_return;
static uint16_t event_frames;
uint8_t hand_play_allowed, hand_evolve_allowed;
uint16_t ui_selection;
Choice pending_choice;
char notice[96];
static uint8_t cooldown, confirm_return, notice_return;
static uint16_t entropy = 713, saved_selection, confirm_selection, hand_selection;
static uint8_t resume_hand;
static uint8_t pending_sound, sound_music_enabled;
uint8_t ui_choice_cache_mode = DG_NONE;
static uint8_t choice_cache_card;
static uint16_t choice_cache_count;
static Choice choice_cache[64];

uint8_t ui_text_page(uint8_t def, uint8_t page, char *out) DG_BANKED {
    const char *s = dg_cards[def].text;
    uint8_t current = 0, row = 0, n, end, i;
    uint8_t rows = dg_cards[def].kind >= DG_TAMER ? 6 : 7;
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;
        n = 0;
        while (s[n] && s[n] != '\n' && n < 24) ++n;
        end = n;
        if (n == 24 && s[n] && s[n] != ' ' && s[n] != '\n')
            for (i = n; i; --i) if (s[i] == ' ') { end = i; break; }
        if (out && current == page) {
            for (i = 0; i < end; ++i) *out++ = s[i];
            *out++ = '\n';
        }
        s += end;
        if (*s == '\n') ++s;
        else while (*s == ' ') ++s;
        if (++row == rows) {
            while (*s == ' ' || *s == '\n') ++s;
            if (*s) { ++current; row = 0; }
        }
    }
    if (out) *out = 0;
    return current+1;
}

uint16_t ui_choices(uint8_t mode, uint16_t wanted, Choice *out) DG_BANKED {
    uint16_t n, i, count = 0;
    uint8_t last = DG_NONE;
    Choice candidates[16];
    if (mode == ui_choice_cache_mode && selected_card == choice_cache_card) {
        if (!out) return choice_cache_count;
        if (wanted >= choice_cache_count) return 0;
        *out = choice_cache[wanted];
        return 1;
    }
    if (mode == M_OPTION_TARGETS) {
        uint8_t def = digi_game.players[0].cards[selected_card].def;
        for (i = 0; i < DG_CARDS; ++i) if (dg_option_target_legal(&digi_game,0,def,i)) {
            choice_cache[count].kind = A_TARGET;
            choice_cache[count].a = i;
            choice_cache[count++].b = DG_NONE;
        }
        if (!count || def == C_GIGA) {
            choice_cache[count].kind = A_DONE;
            choice_cache[count].a = DG_NONE;
            choice_cache[count++].b = DG_NONE;
        }
        ui_choice_cache_mode = mode; choice_cache_card = selected_card; choice_cache_count = count;
        if (out && wanted < count) *out = choice_cache[wanted];
        return out ? wanted < count : count;
    }
    n = dg_choice_count(&digi_game);
    for (i = 0; i < n; ++i) {
        const Choice *c;
        if (!(i & 15)) dg_choice_window(&digi_game,i,16,candidates);
        c = &candidates[i & 15];
        if (mode == M_PLAY && (c->kind != A_PLAY || c->a != selected_card)) continue;
        if (mode == M_EVOLVE && (c->kind != A_EVOLVE || c->a != selected_card)) continue;
        if (mode == M_ATTACK_TARGETS && (c->kind != A_ATTACK || c->a != selected_card)) continue;
        if (mode == M_ATTACKERS) {
            if (c->kind != A_ATTACK || c->a == last) continue;
            last = c->a;
        }
        if (count < 64) choice_cache[count] = *c;
        if (out && wanted == count) *out = *c;
        ++count;
    }
    ui_choice_cache_mode = count <= 64 ? mode : DG_NONE;
    choice_cache_card = selected_card;
    choice_cache_count = count;
    return out ? wanted < count : count;
}

uint8_t ui_target_owner(void) DG_BANKED {
    if (digi_game.effect == E_DP || digi_game.effect == E_KAISER) return digi_game.effect_owner;
    return digi_game.effect_owner ^ 1;
}

uint8_t ui_option_owner(void) DG_BANKED {
    uint8_t def = digi_game.players[0].cards[selected_card].def;
    return def == C_SHADOW || def == C_KAISER ? 0 : 1;
}

uint8_t ui_choice_def(const Choice *c) DG_BANKED {
    uint8_t owner = digi_game.active;
    if (c->kind == A_EFFECT) return digi_game.players[owner].cards[digi_game.pending[c->a]].def;
    if (c->kind == A_TARGET) owner = ui_target_owner();
    if (c->kind == A_BLOCK) owner ^= 1;
    if (c->a == DG_NONE) return DG_NONE;
    return digi_game.players[owner].cards[c->a].def;
}

uint8_t ui_hand_actions(uint8_t index) DG_BANKED {
    if (hand_play_allowed) { if (!index) return A_PLAY; --index; }
    if (hand_evolve_allowed) { if (!index) return A_EVOLVE; --index; }
    return index ? DG_NONE : 100;
}

uint8_t ui_cost(const Choice *c) DG_BANKED {
    uint8_t def = ui_choice_def(c);
    if (c->kind == A_PASS) return 0;
    if (c->kind == A_EVOLVE) return dg_cards[def].evo;
    if (c->kind == A_PLAY) return dg_cards[def].cost;
    if (c->kind == A_ATTACK && (def == C_COREDRAMON || def == C_GRIZZLYMON)) return 2;
    return 0;
}

static void change_selection(uint16_t count, uint8_t keys) {
    if (!count) { ui_selection = 0; return; }
    if (keys & J_DOWN) ui_selection = (ui_selection+1)%count;
    if (keys & J_UP) ui_selection = ui_selection ? ui_selection-1 : count-1;
}

static void inspect(uint8_t owner, uint8_t id) {
    if (id == DG_NONE) return;
    card_return = ui_screen; saved_selection = ui_selection;
    inspector_owner = owner; inspector_root = id; inspector_card = id;
    card_page = 0; ui_screen = S_CARD;
}

static void commit(const Choice *choice) {
    pending_choice = *choice;
    confirm_return = ui_screen; confirm_selection = ui_selection;
    ui_screen = S_COMMIT;
}

static void apply_targeted(const Choice *choice, const Choice *target) {
    uint8_t head = digi_game.log_head, opponent = digi_game.controller || ui_auto;
    uint8_t target_owner = ui_target_owner(), target_def = DG_NONE;
    pending_sound = choice->kind == A_EVOLVE ? SFX_EVOLVE : choice->kind == A_PLAY || choice->kind == A_HATCH || choice->kind == A_RAISE ? SFX_PLAY : SFX_NONE;
    if (choice->kind == A_TARGET && choice->a != DG_NONE) target_def = digi_game.players[target_owner].cards[choice->a].def;
    if (choice->kind == A_ATTACK) event_attack_target = choice->b == DG_NONE ? DG_NONE : digi_game.players[digi_game.active^1].cards[choice->b].def;
    if (!ui_auto && (choice->kind == A_PLAY || choice->kind == A_EVOLVE)) resume_hand = 1;
    if (choice->kind == A_ATTACK) command = 2;
    dg_apply(&digi_game,choice);
    if (target && digi_game.phase == P_TARGET && !digi_game.controller) dg_apply(&digi_game,target);
    ui_choice_cache_mode = DG_NONE;
    ui_selection = 0; cooldown = 0; ui_screen = S_DUEL;
    if (digi_game.active || digi_game.phase == P_OVER) resume_hand = 0;
    if (resume_hand && digi_game.phase == P_MAIN && !digi_game.controller) {
        resume_hand = 0;
        if (digi_game.players[0].hand_count) {
            ui_screen = S_HAND;
            ui_selection = hand_selection < digi_game.players[0].hand_count ? hand_selection : digi_game.players[0].hand_count-1;
        }
    }
    event_count = 0; event_index = 0; event_frames = 0;
    evolution_drew = 0; evolution_draw_card = DG_NONE;
    while (head != digi_game.log_head) {
        const LogEntry *e = &digi_game.log[head];
        if (e->kind == L_DRAW && digi_game.log[(head+11)%12].kind == L_EVOLVE) {
            evolution_drew = 1;
            if (!e->player) {
                const Player *p = &digi_game.players[0];
                evolution_draw_card = p->cards[p->hand[p->hand_count-1]].def;
                hand_selection = p->hand_count-1;
                if (ui_screen == S_HAND) ui_selection = hand_selection;
            }
        }
        if (e->kind != L_DRAW && (opponent || (e->kind >= L_EVOLVE && e->kind <= L_EFFECT) || e->kind == L_TURN || e->kind == L_WIN))
            ui_events[event_count++] = *e;
        head = (head+1)%12;
    }
    if (!event_count && target_def != DG_NONE) {
        ui_events[0].kind = L_COUNT;
        ui_events[0].player = target_owner;
        ui_events[0].card = target_def;
        event_count = 1;
    }
    if (event_count) { event_return = ui_screen; ui_screen = S_EVENT; }
}

static void apply(const Choice *choice) { apply_targeted(choice,0); }

static void play_action_sound(void) {
    uint8_t effect = pending_sound;
    pending_sound = SFX_NONE;
    if (ui_screen == S_EVENT && ui_events[event_index].kind == L_CHECK) gfx_shake();
    if (ui_screen == S_EVENT && ui_events[event_index].kind == L_EFFECT && ui_events[event_index].card == 15) effect = SFX_GAIA;
    if (ui_screen == S_EVENT && !effect) {
        switch (ui_events[event_index].kind) {
        case L_PLAY: case L_HATCH: case L_RAISE: case L_EFFECT: case L_RETURN: effect = SFX_PLAY; break;
        case L_EVOLVE: effect = SFX_EVOLVE; break;
        case L_ATTACK: effect = SFX_ATTACK; break;
        case L_BLOCK: effect = SFX_BLOCK; break;
        case L_CHECK: effect = SFX_SECURITY; break;
        case L_DELETE: case L_STRIP: effect = SFX_DELETE; break;
        case L_WIN: effect = ui_events[event_index].player ? SFX_LOSE : SFX_WIN; break;
        }
    }
    if (effect) audio_sfx(effect);
}

uint8_t ui_board_count(uint8_t owner) DG_BANKED {
    return 1 + dg_count_zone(&digi_game,owner,Z_FIELD) + dg_count_zone(&digi_game,owner,Z_TAMER);
}

uint8_t ui_board_card(uint8_t owner, uint8_t index) DG_BANKED {
    uint8_t count;
    if (!index) return dg_nth_zone(&digi_game,owner,Z_BREED,0);
    count = dg_count_zone(&digi_game,owner,Z_FIELD);
    if (index <= count) return dg_nth_zone(&digi_game,owner,Z_FIELD,index-1);
    return dg_nth_zone(&digi_game,owner,Z_TAMER,index-count-1);
}

static void open_board(void) {
    uint8_t p, count;
    for (p = 0; p < 2; ++p) {
        count = ui_board_count(p)-1;
        if (board_selection[p] > count) board_selection[p] = count;
        if (!board_selection[p] && count && dg_nth_zone(&digi_game,p,Z_BREED,0) == DG_NONE) board_selection[p] = 1;
    }
    ui_screen = S_BOARD;
    ui_selection = board_selection[board_owner];
}

static void tell(const char *message) {
    notice_return = ui_screen;
    strcpy(notice,message);
    ui_screen = S_NOTICE;
}

static void title(void) {
    audio_play(0);
    ui_screen = S_MENU; ui_selection = 0;
}

void ui_init(void) DG_BANKED {
    gfx_init();
    ui_screen = S_TITLE;
    ui_draw();
}

void ui_frame(uint8_t keys) DG_BANKED {
    uint16_t count;
    uint8_t id, owner, kind, cursor_only, old_screen = ui_screen;
    uint16_t old_selection = ui_selection;
    Choice choice;
    ++entropy;
    if (ui_screen == S_TITLE && !(keys & (J_A|J_START))) return;
    if (ui_screen == S_EVENT) {
        if (++event_frames >= (ui_events[event_index].kind == L_CHECK || ui_events[event_index].kind == L_EFFECT ? 240 : 120) || (keys & (J_A|J_B))) {
            event_frames = 0;
            if (++event_index == event_count) ui_screen = event_return;
            ui_draw();
            play_action_sound();
        }
        return;
    }
    if (ui_screen == S_DUEL && digi_game.phase != P_OVER && (ui_auto || digi_game.controller == 1)) {
        if (++cooldown >= 12) {
            cooldown = 0;
            if (dg_ai_choose(&digi_game,&choice)) apply(&choice);
            ui_draw();
            play_action_sound();
            return;
        }
    }
    if (!keys) return;
    cursor_only = !(keys & (J_A|J_B|J_START|J_SELECT)) &&
        (ui_screen == S_MENU || ui_screen == S_ACTION || ui_screen == S_SFX ||
        (ui_screen == S_SOUND && !(keys & (J_LEFT|J_RIGHT))) || (ui_screen == S_HELP && !help_open) ||
        ui_screen == S_TARGET || ui_screen == S_ATTACK || ui_screen == S_FIELD ||
        (ui_screen == S_DUEL && !ui_auto && !digi_game.controller && digi_game.phase != P_OVER));
    if (ui_screen != S_SOUND && ui_screen != S_SFX) audio_click();
    switch (ui_screen) {
    case S_TITLE:
        if (keys & (J_A|J_START)) { audio_play(0); ui_screen = S_MENU; }
        break;
    case S_MENU:
        if (keys & J_B) { ui_screen = S_TITLE; break; }
        if (keys & J_UP) title_selection = (title_selection+4)%5;
        if (keys & J_DOWN) title_selection = (title_selection+1)%5;
        if (keys & (J_A|J_START)) {
            ui_selection = 0; ui_auto = title_selection == 3;
            if (title_selection < 2 || ui_auto) {
                dg_init(&digi_game,entropy ^ ((uint16_t)DIV_REG << 8),ui_auto ? 0 : title_selection,entropy & 1);
                ui_choice_cache_mode = DG_NONE;
                command = 0; cooldown = 0; resume_hand = 0; hand_selection = 0; ui_screen = S_DUEL;
                board_owner = 0; board_selection[0] = 0; board_selection[1] = 0;
                audio_channels = 15; audio_speed = 8; audio_start();
            } else if (title_selection == 2) { ui_screen = S_SOUND; audio_start(); }
            else { ui_screen = S_HELP; help_open = 0; }
        }
        break;
    case S_DUEL:
        if (keys & J_SELECT) { open_board(); break; }
        if (keys & J_B) { ui_screen = S_QUIT; break; }
        if (keys & J_START) {
            if (!ui_auto && digi_game.controller == 0 && (digi_game.phase == P_TARGET || digi_game.phase == P_ORDER || digi_game.phase == P_BLOCK)) {
                if (ui_choices(M_ALL,ui_selection,&choice) && choice.a != DG_NONE) {
                    owner = choice.kind == A_TARGET ? ui_target_owner() : choice.kind == A_BLOCK ? digi_game.active^1 : digi_game.active;
                    id = choice.kind == A_EFFECT ? digi_game.pending[choice.a] : choice.a;
                    inspect(owner,id);
                }
            } else { ui_screen = S_LOG; log_scroll = 0; }
            break;
        }
        if (digi_game.phase == P_OVER) { if (keys & J_A) title(); break; }
        if (ui_auto || digi_game.controller != 0) break;
        if (digi_game.phase != P_MAIN) {
            count = ui_choices(M_ALL,0,0);
            change_selection(count,keys);
            if ((keys & J_A) && ui_choices(M_ALL,ui_selection,&choice)) apply(&choice);
            break;
        }
        if (keys & (J_UP|J_DOWN)) command = (command+2)%4;
        if (keys & (J_LEFT|J_RIGHT)) command ^= 1;
        if (keys & J_A) {
            ui_selection = 0;
            switch (command) {
            case 0:
                ui_screen = S_HAND;
                count = digi_game.players[0].hand_count;
                ui_selection = !count ? 0 : hand_selection < count ? hand_selection : count-1;
                break;
            case 1: open_board(); break;
            case 2: ui_screen = S_ATTACK; break;
            case 3: choice.kind = A_PASS; choice.a = DG_NONE; choice.b = DG_NONE; commit(&choice); break;
            }
        }
        break;
    case S_HAND:
        if (keys & J_B) { ui_screen = S_DUEL; break; }
        count = digi_game.players[0].hand_count;
        change_selection(count,keys);
        if (!count) break;
        if (keys & J_LEFT) ui_selection = ui_selection >= 6 ? ui_selection-6 : 0;
        if (keys & J_RIGHT) ui_selection = ui_selection+6 < count ? ui_selection+6 : count-1;
        hand_selection = ui_selection;
        id = digi_game.players[0].hand[ui_selection];
        if (keys & J_START) inspect(0,id);
        else if (keys & J_A) {
            selected_card = id; hand_selection = ui_selection;
            hand_play_allowed = ui_choices(M_PLAY,0,0) != 0;
            hand_evolve_allowed = ui_choices(M_EVOLVE,0,0) != 0;
            ui_selection = 0; ui_screen = S_ACTION;
        }
        break;
    case S_ACTION:
        if (keys & J_B) { ui_screen = S_HAND; ui_selection = hand_selection; break; }
        count = 1 + hand_play_allowed + hand_evolve_allowed;
        change_selection(count,keys);
        if (keys & J_START) inspect(0,selected_card);
        else if (keys & J_A) {
            kind = ui_hand_actions(ui_selection);
            if (kind == 100) inspect(0,selected_card);
            else if (kind == A_PLAY) {
                if (dg_option_effect(digi_game.players[0].cards[selected_card].def)) { action_mode = M_OPTION_TARGETS; ui_selection = 0; ui_screen = S_TARGET; }
                else { ui_choices(M_PLAY,0,&choice); apply(&choice); }
            }
            else { action_mode = M_EVOLVE; ui_selection = 0; ui_screen = S_TARGET; }
        }
        break;
    case S_ATTACK:
        if (keys & J_B) { ui_screen = S_DUEL; break; }
        count = ui_choices(M_ATTACKERS,0,0);
        if (!count) cursor_only = 0;
        change_selection(count,keys);
        if (count && ui_choices(M_ATTACKERS,ui_selection,&choice)) {
            if (keys & J_START) inspect(0,choice.a);
            else if (keys & J_A) { selected_card = choice.a; action_mode = M_ATTACK_TARGETS; ui_screen = S_TARGET; ui_selection = 0; }
        }
        break;
    case S_TARGET:
        if (keys & J_B) {
            ui_screen = action_mode == M_OPTION_TARGETS ? S_HAND : action_mode == M_EVOLVE ? S_ACTION : S_ATTACK;
            ui_selection = action_mode == M_OPTION_TARGETS ? hand_selection : 0;
            break;
        }
        count = ui_choices(action_mode,0,0);
        if (!count) cursor_only = 0;
        change_selection(count,keys);
        if (count && ui_choices(action_mode,ui_selection,&choice)) {
            if (action_mode == M_OPTION_TARGETS) {
                if ((keys & J_START) && choice.a != DG_NONE) inspect(ui_option_owner(),choice.a);
                else if (keys & J_A) {
                    Choice play;
                    play.kind = A_PLAY; play.a = selected_card; play.b = DG_NONE;
                    apply_targeted(&play,&choice);
                }
            }
            else if ((keys & J_START) && choice.b != DG_NONE) inspect(action_mode == M_EVOLVE ? 0 : 1,choice.b);
            else if (keys & J_A) apply(&choice);
        }
        break;
    case S_COMMIT:
        if (keys & J_B) { ui_screen = confirm_return; ui_selection = confirm_selection; }
        else if (keys & J_A) apply(&pending_choice);
        break;
    case S_BOARD:
        if (keys & (J_B|J_SELECT)) { ui_screen = S_DUEL; ui_selection = 0; break; }
        if (keys & (J_UP|J_DOWN)) { board_owner ^= 1; ui_selection = board_selection[board_owner]; }
        count = ui_board_count(board_owner);
        if (keys & J_RIGHT) ui_selection = (ui_selection+1)%count;
        if (keys & J_LEFT) ui_selection = ui_selection ? ui_selection-1 : count-1;
        board_selection[board_owner] = ui_selection;
        id = ui_board_card(board_owner,ui_selection);
        if (keys & J_START) {
            ui_screen = S_FIELD;
            browse = board_owner + (id != DG_NONE && digi_game.players[board_owner].cards[id].zone == Z_TAMER ? 7 : 0);
            ui_selection = 0;
        } else if (keys & J_A) inspect(board_owner,id);
        break;
    case S_FIELD:
        if (keys & (J_B|J_SELECT)) { open_board(); break; }
        if (keys & (J_LEFT|J_RIGHT)) { browse = (browse+(keys & J_RIGHT ? 1 : 8))%9; ui_selection = 0; }
        owner = browse_owners[browse];
        count = dg_count_zone(&digi_game,owner,browse_zones[browse]);
        if (!count) cursor_only = 0;
        change_selection(count,keys);
        if ((keys & (J_A|J_START)) && count) inspect(owner,dg_nth_zone(&digi_game,owner,browse_zones[browse],ui_selection));
        break;
    case S_CARD:
        if (keys & J_B) { ui_screen = card_return; ui_selection = saved_selection; break; }
        id = digi_game.players[inspector_owner].cards[inspector_card].def;
        count = ui_text_page(id,255,0);
        if (dg_cards[id].kind < DG_TAMER) count += 1 + (digi_game.players[inspector_owner].cards[inspector_root].under != DG_NONE);
        if (keys & J_RIGHT) card_page = (card_page+1)%count;
        if (keys & J_LEFT) card_page = (card_page+count-1)%count;
        if (keys & J_DOWN) {
            id = digi_game.players[inspector_owner].cards[inspector_card].under;
            inspector_card = id == DG_NONE ? inspector_root : id;
            card_page = 0;
        }
        if (keys & J_UP) { inspector_card = inspector_root; card_page = 0; }
        break;
    case S_LOG:
        if (keys & (J_B|J_START)) { ui_screen = S_DUEL; ui_selection = 0; }
        if ((keys & J_DOWN) && log_scroll+5 < digi_game.log_count) ++log_scroll;
        if ((keys & J_UP) && log_scroll) --log_scroll;
        break;
    case S_SOUND:
        change_selection(5,keys);
        if (keys & J_A) {
            if (ui_selection < 4) audio_toggle(ui_selection);
            else { sound_music_enabled = audio_enabled; audio_play(0); ui_screen = S_SFX; ui_selection = 0; }
        }
        if (keys & J_START) audio_play(!audio_enabled);
        if ((keys & J_LEFT) && audio_speed > 3) --audio_speed;
        if ((keys & J_RIGHT) && audio_speed < 15) ++audio_speed;
        if (keys & J_B) title();
        break;
    case S_SFX:
        change_selection(SFX_COUNT-1,keys);
        if (keys & J_A) { audio_sfx(SFX_NONE); audio_sfx(ui_selection+1); }
        if (keys & J_B) { audio_play(sound_music_enabled); ui_screen = S_SOUND; ui_selection = 4; }
        break;
    case S_QUIT:
        if (keys & J_A) title();
        if (keys & J_B) ui_screen = S_DUEL;
        break;
    case S_NOTICE: if (keys & (J_A|J_B)) ui_screen = notice_return; break;
    case S_HELP:
        if (!help_open) {
            change_selection(HELP_TOPICS,keys);
            if (keys & (J_A|J_START)) help_open = 1;
            else if (keys & J_B) title();
        } else {
            if (keys & J_LEFT) ui_selection = (ui_selection+HELP_TOPICS-1)%HELP_TOPICS;
            if (keys & (J_RIGHT|J_A)) ui_selection = (ui_selection+1)%HELP_TOPICS;
            if (keys & J_B) help_open = 0;
        }
        break;
    }
    if (old_screen != ui_screen) cursor_only = 0;
    if ((ui_screen == S_TARGET || ui_screen == S_ATTACK || (ui_screen == S_DUEL && digi_game.phase != P_MAIN)) && old_selection/5 != ui_selection/5) cursor_only = 0;
    if (ui_screen == S_FIELD && (old_selection/4 != ui_selection/4 || keys & (J_LEFT|J_RIGHT))) cursor_only = 0;
    if (ui_screen == S_HAND && old_screen == S_HAND && digi_game.players[0].hand_count && old_selection/6 == ui_selection/6) ui_refresh_hand();
    else if (cursor_only) ui_refresh_cursor();
    else ui_draw();
    play_action_sound();
}
