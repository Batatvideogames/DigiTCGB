#pragma bank 4
#include <string.h>
#include "ui_internal.h"
#include "gfx.h"
#include "audio.h"

static char text_buffer[192];
static void text(uint8_t x, uint8_t y, uint8_t w, const char *s) {
    strcpy(text_buffer,s);
    gfx_text(x,y,w,text_buffer);
}
static void wrap(uint8_t x, uint8_t y, uint8_t w, uint8_t rows, const char *s) {
    strcpy(text_buffer,s);
    gfx_wrap(x,y,w,rows,text_buffer);
}
static void header(const char *s) {
    gfx_band(0,8,1); text(4,0,152,s);
}
static void footer(const char *a, const char *b) {
    gfx_box(0,120,160,24);
    text(6,124,148,a); text(6,132,148,b);
}
static void confirm_controls(void) {
    text(8,128,70,"A: CONFIRM"); text(90,128,62,"B: CANCEL");
}
static uint8_t definition(uint8_t owner, uint8_t id) {
    return id == DG_NONE ? DG_NONE : digi_game.players[owner].cards[id].def;
}
static const char *kind_name(uint8_t kind) {
    return kind == DG_EGG ? "DIGI-EGG" : kind == DG_DIGIMON ? "DIGIMON" : kind == DG_TAMER ? "TAMER" : "OPTION";
}
static void memory_bar(void) {
    uint8_t i;
    int8_t m = digi_game.active ? -digi_game.memory : digi_game.memory;
    gfx_band(56,16,1);
    text(4,57,36,"MEMORY");
    text(77,57,6,"0");
    text(106,57,24,m >= 0 ? "YOU" : "FOE");
    gfx_number(136,57,m >= 0 ? m : -m);
    for (i = 0; i < 21; ++i) gfx_rect(19+i*6,67,2,3,1);
    gfx_rect(78,65,4,7,1);
    gfx_rect(78-m*6,65,4,7,1);
    gfx_rect(79-m*6,66,2,4,0);
}
static void cursor(uint8_t x, uint8_t y) { gfx_cursor(x,y); }
static void position(uint16_t at, uint16_t count, uint8_t y) {
    gfx_number(112,y,count ? at+1 : 0); text(130,y,6,"/"); gfx_number(138,y,count);
}
static void memory_after(uint8_t cost, uint8_t y) {
    int8_t after = digi_game.memory-cost;
    text(6,y,84,after < 0 ? "AFTER: FOE" : "AFTER: YOU");
    gfx_number(74,y,after < 0 ? -after : after);
    text(92,y,60,after < 0 ? "TURN ENDS" : "MEMORY");
}

static void title_screen(void) {
    gfx_title();
    gfx_agumon(48,32);
    text(47,128,72,"PRESS START");
}

static void start_menu(void) {
    static const char *const items[] = {"PLAY GAIA RED","PLAY COCYTUS BLUE","SOUND TEST","WATCH A DUEL","HOW TO PLAY"};
    uint8_t i;
    header("DIGITAL CARD BATTLE");
    gfx_box(8,16,144,40);
    text(44,24,90,"DIGITCGB"); text(29,40,108,"STARTER DECK DEMO");
    gfx_box(8,64,144,72);
    for (i = 0; i < 5; ++i) {
        text(24,72+i*12,120,items[i]);
        if (title_selection == i) cursor(14,72+i*12);
    }
}

static void status(uint8_t owner, uint8_t y) {
    const Player *p = &digi_game.players[owner];
    uint8_t x = owner ? 4 : 56;
    uint8_t id = dg_nth_zone(&digi_game,owner,Z_FIELD,0), def;
    if (id == DG_NONE) id = dg_nth_zone(&digi_game,owner,Z_BREED,0);
    def = definition(owner,id);
    gfx_portrait(def,owner ? 120 : 0,owner ? 8 : 72,owner);
    text(x,y,96,def == DG_NONE ? "NO DIGIMON" : dg_cards[def].name);
    text(x,y+10,54,"SEC"); gfx_number(x+24,y+10,p->security_count);
    text(x+42,y+10,36,"HAND"); gfx_number(x+72,y+10,p->hand_count);
    text(x,y+20,42,"DECK"); gfx_number(x+30,y+20,p->deck_count);
    text(x+54,y+20,24,"EGG"); gfx_number(x+78,y+20,dg_count_zone(&digi_game,owner,Z_EGGS));
    text(x,y+30,42,"BATTLE"); gfx_number(x+42,y+30,dg_count_zone(&digi_game,owner,Z_FIELD));
    text(x+60,y+30,24,"TMR"); gfx_number(x+84,y+30,dg_count_zone(&digi_game,owner,Z_TAMER));
}

static void choice_label(const Choice *c, uint8_t y) {
    uint8_t def = ui_choice_def(c);
    const char *label = 0;
    switch (c->kind) {
    case A_HATCH: label = "HATCH DIGI-EGG"; break;
    case A_RAISE: label = "MOVE TO BATTLE"; break;
    case A_SKIP: label = "SKIP BREEDING"; break;
    case A_DONE: label = "DONE SELECTING"; break;
    case A_NO_BLOCK: label = "DO NOT BLOCK"; break;
    case A_PASS: label = "PASS TURN"; break;
    }
    text(16,y,136,label ? label : def == DG_NONE ? "CHOOSE" : dg_cards[def].name);
}

static void prompt(void) {
    uint16_t count = ui_choices(M_ALL,0,0), start = (ui_selection/5)*5, i;
    Choice c;
    const char *label = digi_game.phase == P_BREED ? "BREEDING PHASE" : digi_game.phase == P_BLOCK ? "BLOCK THIS ATTACK?" : digi_game.phase == P_ORDER ? "CHOOSE EFFECT ORDER" : "CHOOSE EFFECT TARGET";
    header(label);
    if (digi_game.phase == P_TARGET) text(6,16,148,dg_cards[digi_game.effect_card].name);
    else text(6,16,148,digi_game.phase == P_BLOCK ? "YOUR DEFENSE" : "YOUR TURN");
    gfx_box(0,32,160,112);
    for (i = 0; i < 5 && start+i < count; ++i) {
        ui_choices(M_ALL,start+i,&c); choice_label(&c,40+i*12);
        if (start+i == ui_selection) cursor(6,40+i*12);
    }
    if (count > 5) position(ui_selection/5,(count+4)/5,108);
}

static void duel(void) {
    static const char *const commands[] = {"HAND","FIELD","ATTACK","PASS"};
    uint8_t i;
    if (!ui_auto && digi_game.controller == 0 && digi_game.phase != P_MAIN && digi_game.phase != P_OVER) { prompt(); return; }
    header(ui_auto ? "WATCHING A DUEL" : digi_game.active ? "OPPONENT TURN" : "YOUR TURN");
    gfx_number(140,0,digi_game.turn);
    status(1,12); memory_bar(); status(0,74);
    gfx_box(0,112,160,32);
    if (digi_game.phase == P_OVER) {
        text(8,116,144,digi_game.winner ? "OPPONENT WON" : "YOU WON");
        text(8,132,144,"STARTER DUEL COMPLETE");
    } else if (ui_auto || digi_game.controller) {
        const LogEntry *e = &digi_game.log[(digi_game.log_head+11)%12];
        text(8,116,144,log_names[e->kind]);
        text(8,132,144,e->card == DG_NONE ? "RESOLVING TURN" : dg_cards[e->card].name);
    } else for (i = 0; i < 4; ++i) {
        uint8_t x = (i&1) ? 94 : 14, y = 116+(i/2)*12;
        text(x,y,60,commands[i]);
        if (!i) gfx_number(44,y,digi_game.players[0].hand_count);
        if (command == i) cursor(x-8,y);
    }
}

static void preview_stats(uint8_t def, uint8_t owner) {
    const CardDef *c = &dg_cards[def];
    gfx_portrait(def,8,16,owner);
    text(72,16,84,c->number); text(72,24,84,kind_name(c->kind));
    if (c->kind == DG_DIGIMON || c->kind == DG_EGG) { text(72,32,48,"LEVEL"); gfx_number(108,32,c->level); }
    if (c->kind != DG_EGG) { text(72,48,42,"PLAY"); gfx_number(108,48,c->cost); }
    if (c->kind == DG_DIGIMON) { gfx_number(72,56,(uint16_t)c->dp*1000); text(114,56,30,"DP"); }
}

static void small_stats(uint8_t def, uint8_t owner) {
    gfx_box(0,8,64,64);
    preview_stats(def,owner);
}

static void hand(void) {
    const Player *p = &digi_game.players[0];
    uint8_t i, start = (ui_selection/6)*6;
    header("YOUR HAND");
    if (!p->hand_count) { wrap(8,32,144,3,"YOUR HAND IS EMPTY."); return; }
    gfx_hand_begin(0);
    position(ui_selection,p->hand_count,0);
    small_stats(p->cards[p->hand[ui_selection]].def,0);
    gfx_box(0,72,160,72);
    for (i = 0; i < 6 && start+i < p->hand_count; ++i) {
        text(16,78+i*10,136,dg_cards[p->cards[p->hand[start+i]].def].name);
        if (start+i == ui_selection) cursor(6,78+i*10);
    }
}

void ui_refresh_hand(void) DG_BANKED {
    const Player *p = &digi_game.players[0];
    gfx_hand_begin(1);
    position(ui_selection,p->hand_count,0);
    preview_stats(p->cards[p->hand[ui_selection]].def,0);
    cursor(6,78+(ui_selection%6)*10);
    gfx_hand_present();
}

static void actions(void) {
    uint8_t def = definition(0,selected_card), i, kind;
    header(dg_cards[def].name); small_stats(def,0);
    gfx_box(0,72,160,72);
    for (i = 0; i < 3; ++i) {
        kind = ui_hand_actions(i);
        if (kind == DG_NONE) break;
        text(16,80+i*12,136,kind == A_PLAY ? "PLAY" : kind == A_EVOLVE ? "DIGIVOLVE" : "CARD DETAILS");
        if (kind != 100) {
            uint8_t cost = kind == A_PLAY ? dg_cards[def].cost : dg_cards[def].evo;
            int8_t after = digi_game.memory-cost;
            gfx_number(94,80+i*12,cost);
            text(110,80+i*12,24,after < 0 ? "FOE" : "YOU");
            gfx_number(134,80+i*12,after < 0 ? -after : after);
        }
        if (i == ui_selection) cursor(6,80+i*12);
    }
    text(8,132,144,hand_play_allowed || hand_evolve_allowed ? "MEMORY AFTER COST" : "NO LEGAL ACTION NOW");
}

static void targets(void) {
    uint8_t mode = ui_screen == S_ATTACK ? M_ATTACKERS : action_mode;
    uint16_t count = ui_choices(mode,0,0), i, start = (ui_selection/5)*5;
    Choice c;
    header(mode == M_ATTACKERS ? "CHOOSE ATTACKER" : mode == M_EVOLVE ? "DIGIVOLVE ONTO" : mode == M_OPTION_TARGETS ? "CHOOSE OPTION TARGET" : "CHOOSE ATTACK TARGET");
    if (mode != M_ATTACKERS) text(6,16,148,dg_cards[definition(0,selected_card)].name);
    gfx_box(0,32,160,112);
    if (!count) wrap(8,42,144,6,"NO DIGIMON CAN ATTACK. CHECK IF SUSPENDED, PLAYED THIS TURN OR PREVENTED BY AN EFFECT.");
    for (i = 0; i < 5 && start+i < count; ++i) {
        uint8_t owner = mode == M_OPTION_TARGETS ? ui_option_owner() : mode == M_ATTACK_TARGETS ? 1 : 0, id, def;
        ui_choices(mode,start+i,&c);
        id = mode == M_ATTACKERS || mode == M_OPTION_TARGETS ? c.a : c.b; def = definition(owner,id);
        text(16,40+i*12,136,def == DG_NONE ? mode == M_OPTION_TARGETS ? "PLAY WITHOUT TARGET" : "OPPONENT / SECURITY" : dg_cards[def].name);
        if (start+i == ui_selection) cursor(6,40+i*12);
    }
    if (count > 5) position(ui_selection/5,(count+4)/5,108);
    if (count && mode != M_ATTACKERS) { ui_choices(mode,ui_selection,&c); memory_after(mode == M_OPTION_TARGETS ? dg_cards[definition(0,selected_card)].cost : ui_cost(&c),132); }
}

static void confirmation(void) {
    uint8_t def = ui_choice_def(&pending_choice), cost = ui_cost(&pending_choice);
    int8_t after = digi_game.memory-cost;
    header("CONFIRM ACTION");
    gfx_box(0,16,160,128);
    if (pending_choice.kind == A_PASS) {
        text(8,24,144,"PASS YOUR TURN?");
        wrap(8,48,144,3,"YOUR OPPONENT RECEIVES 3 MEMORY.");
    } else {
        text(8,24,144,pending_choice.kind == A_EVOLVE ? "DIGIVOLVE" : pending_choice.kind == A_ATTACK ? "ATTACK" : "PLAY");
        text(8,36,144,dg_cards[def].name);
        if (pending_choice.kind == A_ATTACK || pending_choice.kind == A_EVOLVE) {
            def = definition(pending_choice.kind == A_ATTACK ? 1 : 0,pending_choice.b);
            text(8,50,144,def == DG_NONE ? "TARGET: OPPONENT" : dg_cards[def].name);
        }
        text(8,68,96,"MEMORY COST"); gfx_number(110,68,cost);
        text(8,82,90,after >= 0 ? "AFTER: YOU" : "AFTER: FOE"); gfx_number(110,82,after >= 0 ? after : -after);
        text(8,100,144,after < 0 ? "TURN ENDS AFTER EFFECTS" : "CONTINUE YOUR TURN");
    }
    confirm_controls();
}

static void field(void) {
    uint8_t owner = browse_owners[browse], zone = browse_zones[browse];
    uint8_t count = dg_count_zone(&digi_game,owner,zone), start = (ui_selection/4)*4, i;
    header(browse_names[browse]);
    if (count > 4) position(ui_selection/4,(count+3)/4,12);
    gfx_box(0,24,160,120);
    for (i = 0; i < 4 && start+i < count; ++i) {
        uint8_t id = dg_nth_zone(&digi_game,owner,zone,start+i), def = definition(owner,id), y = 30+i*22;
        text(16,y,136,dg_cards[def].name);
        if (dg_cards[def].kind == DG_DIGIMON) {
            text(16,y+9,24,"DP"); gfx_number(34,y+9,(uint16_t)dg_dp(&digi_game,owner,id,DG_NONE)*1000);
            text(76,y+9,48,"SOURCES"); gfx_number(124,y+9,dg_sources(&digi_game,owner,id));
        } else text(16,y+9,136,kind_name(dg_cards[def].kind));
        if (start+i == ui_selection) cursor(6,y);
    }
    if (!count) text(16,46,128,"NO CARDS HERE");
}

static void card(void) {
    const Card *c = &digi_game.players[inspector_owner].cards[inspector_card];
    const CardDef *def = &dg_cards[c->def];
    uint8_t id, y = 34, pages = ui_text_page(c->def,255,0);
    header(def->name);
    if (def->kind >= DG_TAMER) {
        small_stats(c->def,inspector_owner);
        text(72,32,84,def->color ? "BLUE" : "RED");
        text(72,60,30,"TEXT");
        position(card_page,pages,60);
        gfx_box(0,72,160,72);
        ui_text_page(c->def,card_page,text_buffer);
        gfx_wrap(8,80,144,6,text_buffer);
        return;
    }
    if (!card_page) {
        small_stats(c->def,inspector_owner);
        gfx_box(0,72,160,72);
        text(8,80,144,def->color ? "BLUE" : "RED");
        if (def->kind == DG_DIGIMON) {
            text(8,92,90,"DIGIVOLVE"); gfx_number(74,92,def->evo);
            text(92,92,42,"FROM L"); gfx_number(134,92,def->level-1);
            text(8,104,144,c->zone == Z_SOURCE ? "DIGIVOLUTION SOURCE" : c->zone == Z_BREED ? "IN BREEDING AREA" : c->locked_until ? "CANNOT ATTACK OR BLOCK" : c->flags & F_SUSPENDED ? "SUSPENDED" : c->flags & F_NEW ? "PLAYED THIS TURN" : "READY");
        } else if (def->kind == DG_EGG) text(8,96,144,"HATCH IN BREEDING");
    } else if (card_page <= pages) {
        text(6,16,90,def->number);
        position(card_page-1,pages,16);
        gfx_box(0,32,160,112);
        ui_text_page(c->def,card_page-1,text_buffer);
        gfx_wrap(8,40,144,7,text_buffer);
    } else {
        text(6,16,148,"DIGIVOLUTION SOURCES");
        gfx_box(0,28,160,116);
        id = digi_game.players[inspector_owner].cards[inspector_root].under;
        if (id == DG_NONE) text(8,y,144,"NO SOURCES");
        while (id != DG_NONE && y <= 104) {
            text(8,y,144,dg_cards[definition(inspector_owner,id)].name);
            id = digi_game.players[inspector_owner].cards[id].under; y += 10;
        }
        if (id != DG_NONE) text(8,120,144,"MORE SOURCES BELOW");
    }
}

static void board_row(uint8_t owner, uint8_t y) {
    uint8_t i, id, def, x = owner ? 2 : 48;
    uint8_t selected = board_selection[owner];
    uint8_t start = selected ? ((selected-1)/3)*3 : 0;
    id = ui_board_card(owner,selected);
    gfx_portrait_pose(definition(owner,id),owner ? 120 : 0,y,owner,GFX_IDLE);
    for (i = 0; i < 4; ++i) {
        id = ui_board_card(owner,i ? start+i : 0);
        def = definition(owner,id);
        if (!i || id != DG_NONE) text(x,y+i*10,8,!i ? "B" : digi_game.players[owner].cards[id].zone == Z_TAMER ? "T" : "D");
        if (def != DG_NONE) text(x+10,y+i*10,100,dg_cards[def].name);
        else if (!i) text(x+10,y,100,"EMPTY");
        if (owner == board_owner && ((!i && !selected) || (i && selected == start+i)))
            gfx_rect(x+10,y+i*10+8,100,1,1);
    }
    text(4,y+40,24,"SEC"); gfx_number(28,y+40,digi_game.players[owner].security_count);
    text(54,y+40,42,"BATTLE"); gfx_number(98,y+40,dg_count_zone(&digi_game,owner,Z_FIELD));
    text(118,y+40,24,"TMR"); gfx_number(144,y+40,dg_count_zone(&digi_game,owner,Z_TAMER));
}

static void board(void) {
    uint8_t id;
    header(board_owner ? "FIELD: OPPONENT" : "FIELD: YOU");
    position(ui_selection,ui_board_count(board_owner),0);
    board_row(1,8); memory_bar(); board_row(0,72);
    id = ui_board_card(board_owner,ui_selection);
    gfx_box(0,120,160,24);
    text(6,124,148,id == DG_NONE ? "EMPTY BREEDING AREA" : dg_cards[definition(board_owner,id)].name);
    if (id != DG_NONE) {
        text(6,134,48,dg_cards[definition(board_owner,id)].number);
        text(64,134,90,!ui_selection ? "BREEDING" : kind_name(dg_cards[definition(board_owner,id)].kind));
    }
}

static void log_screen(void) {
    uint8_t i;
    header("DUEL LOG");
    gfx_box(0,16,160,128);
    for (i = 0; i < 5 && i+log_scroll < digi_game.log_count; ++i) {
        const LogEntry *e = &digi_game.log[(digi_game.log_head+11-i-log_scroll)%12];
        uint8_t y = 22+i*18;
        text(6,y,24,e->player ? "FOE" : "YOU"); text(32,y,120,log_names[e->kind]);
        if (e->card != DG_NONE) text(12,y+8,140,dg_cards[e->card].name);
    }
}

static const char *effect_message(uint8_t def, uint8_t security) {
    switch (def) {
    case C_SHADOW: return security ? "ALL DIGIMON ON THIS SIDE GAIN SECURITY ATTACK +1 UNTIL THE END OF ITS NEXT TURN." : "1 OF YOUR DIGIMON GAINS 3000 DP FOR THIS TURN.";
    case C_STARLIGHT: return security ? "SECURITY DIGIMON ON THIS SIDE GAIN 7000 DP FOR THIS TURN." : "YOUR SECURITY DIGIMON GAIN 7000 DP UNTIL THE END OF YOUR OPPONENT'S NEXT TURN.";
    case C_GIGA: return "DELETE UP TO 2 OPPOSING DIGIMON WITH 4000 DP OR LESS.";
    case C_GAIA: return "DELETE 1 OPPOSING DIGIMON.";
    case C_HAMMER: return security ? "GAIN 2 MEMORY." : "GAIN 1 MEMORY.";
    case C_SORROW: return security ? "AN OPPOSING DIGIMON WITH NO SOURCES CANNOT ATTACK OR BLOCK UNTIL THE END OF YOUR NEXT TURN." : "AN OPPOSING DIGIMON WITH NO SOURCES CANNOT ATTACK OR BLOCK UNTIL THE END OF YOUR OPPONENT'S NEXT TURN.";
    case C_KAISER: return "PLAY 1 DIGIMON FROM THIS SIDE'S DIGIVOLUTION CARDS WITHOUT PAYING ITS COST.";
    case C_COCYTUS: return "RETURN 1 OPPOSING DIGIMON TO HAND. TRASH ITS DIGIVOLUTION CARDS.";
    case C_GARUDAMON: return "1 OF YOUR DIGIMON GAINS 3000 DP FOR THIS TURN.";
    case C_ZUDOMON: return "TRASH THE BOTTOM 2 DIGIVOLUTION CARDS OF 1 OPPOSING DIGIMON.";
    case C_GABUMON: return "TRASH THE BOTTOM DIGIVOLUTION CARD OF 1 OPPOSING DIGIMON AT LEVEL 5 OR LOWER.";
    case C_GARURUMON: return "TRASH THE BOTTOM DIGIVOLUTION CARD OF 1 OPPOSING DIGIMON.";
    case C_METALGARURUMON: return "UNSUSPEND THIS DIGIMON.";
    case C_COREDRAMON: case C_GRIZZLYMON: return "LOSE 2 MEMORY.";
    default: return "RESOLVE THE CARD'S EFFECT.";
    }
}

static void event_screen(void) {
    const LogEntry *e = &ui_events[event_index];
    const char *message = e->kind == L_COUNT ? "THE EFFECT IS APPLIED TO THIS DIGIMON." : log_names[e->kind];
    uint8_t pose = e->kind == L_ATTACK ? GFX_ATTACK : e->kind == L_DELETE ? GFX_HURT : GFX_BATTLE;
    if (e->kind == L_WIN) {
        header("DUEL RESULT"); gfx_box(0,24,160,120);
        text(8,40,144,e->player ? "OPPONENT WON" : "YOU WON");
        return;
    }
    header(e->kind == L_CHECK ? "SECURITY REVEAL" : e->player ? "OPPONENT ACTION" : "YOUR ACTION");
    if (e->card != DG_NONE) {
        const CardDef *c = &dg_cards[e->card];
        gfx_portrait_pose(e->card,8,16,e->player,pose);
        text(64,16,90,c->number);
        text(64,28,90,kind_name(c->kind));
        text(64,40,90,e->player ? "OPPONENT" : "YOU");
        text(8,60,144,c->name);
        if (e->kind == L_CHECK) {
            if (c->kind == DG_DIGIMON) {
                uint8_t deleted = event_index+1 < event_count && ui_events[event_index+1].kind == L_DELETE && ui_events[event_index+1].player != e->player;
                message = deleted ? "SECURITY BATTLE: THE ATTACKER IS DELETED. THIS SECURITY CARD GOES TO THE TRASH." : "SECURITY BATTLE: THE ATTACKER SURVIVES. THIS SECURITY CARD GOES TO THE TRASH.";
            } else if (c->kind == DG_TAMER) message = "SECURITY EFFECT: PLAY THIS TAMER WITHOUT PAYING ITS MEMORY COST.";
            else message = "AN OPTION WAS REVEALED. ITS SECURITY EFFECT ACTIVATES.";
        } else if (e->kind == L_EVOLVE) {
            if (!evolution_drew) message = "DIGIVOLVED. DECK EMPTY: NO CARD DRAWN.";
            else if (e->player) message = "DIGIVOLVED.\nOPPONENT DREW 1 CARD.";
            else {
                gfx_box(0,72,160,72);
                text(8,80,144,"DIGIVOLVED.");
                text(8,96,144,"DRAW 1:");
                text(8,108,144,dg_cards[evolution_draw_card].name);
                return;
            }
        } else if (e->kind == L_EFFECT) message = effect_message(e->card,event_index && ui_events[event_index-1].kind == L_CHECK && ui_events[event_index-1].card == e->card);
        else if (e->kind == L_ATTACK) {
            if (event_attack_target == DG_NONE) message = "THIS DIGIMON ATTACKS THE OPPOSING PLAYER.";
            else {
                gfx_box(0,72,160,72);
                text(8,80,144,"ATTACK TARGET");
                text(8,96,144,dg_cards[event_attack_target].name);
                return;
            }
        }
        else if (e->kind == L_DELETE) message = "THIS DIGIMON AND ITS DIGIVOLUTION CARDS ARE TRASHED.";
        else if (e->kind == L_RETURN) message = "THIS DIGIMON RETURNS TO HAND. ITS DIGIVOLUTION CARDS ARE TRASHED.";
        else if (e->kind == L_STRIP) message = "THIS DIGIVOLUTION CARD IS TRASHED.";
        else if (e->kind == L_BLOCK) message = "THIS DIGIMON SUSPENDS AND BLOCKS THE ATTACK.";
    }
    gfx_box(0,72,160,72);
    wrap(8,80,144,6,message);
}

static void sound(void) {
    static const char *const channels[] = {"PULSE 1  MELODY","PULSE 2  HARMONY","WAVE     BASS","NOISE    RHYTHM"};
    uint8_t i;
    header("SOUND TEST"); text(6,16,148,"BRAVE HEART / BATTLE MIX");
    gfx_box(0,32,160,80);
    for (i = 0; i < 4; ++i) {
        text(16,40+i*14,108,channels[i]); text(130,40+i*14,24,audio_channels&(1<<i) ? "ON" : "OFF");
        if (ui_selection == i) cursor(6,40+i*14);
    }
    text(16,96,136,"SOUND EFFECTS");
    if (ui_selection == 4) cursor(6,96);
    text(6,120,60,audio_enabled ? "PLAYING" : "PAUSED");
    gfx_number(76,120,1120/audio_speed); text(100,120,24,"BPM");
}

static void help(void) {
    static const char *const topics[] = {
        "DUEL SCREEN","YOUR HAND","PLAY / ATTACK","FIELD","OTHER ZONES",
        "CARD DETAILS","DUEL LOG","SOUND TEST","SOUND EFFECTS"
    };
    static const char *const controls[HELP_TOPICS][6][2] = {
        {{"D-PAD","COMMAND"},{"A","CHOOSE"},{"SELECT","FIELD"},{"START","DUEL LOG"},{"B","LEAVE DUEL"},{"A/B","NEXT MESSAGE"}},
        {{"UP/DOWN","CARD"},{"LEFT/RIGHT","JUMP 6 CARDS"},{"A","ACTIONS"},{"START","DETAILS"},{"B","DUEL"}},
        {{"UP/DOWN","SELECT"},{"A","USE CHOICE"},{"START","DETAILS"},{"B","BACK/CANCEL"}},
        {{"UP/DOWN","CHANGE SIDE"},{"LEFT/RIGHT","CARD"},{"A","DETAILS"},{"START","OTHER ZONES"},{"B/SELECT","DUEL"}},
        {{"LEFT/RIGHT","ZONE"},{"UP/DOWN","CARD"},{"A/START","DETAILS"},{"B/SELECT","FIELD"}},
        {{"LEFT/RIGHT","TEXT PAGE"},{"DOWN","NEXT SOURCE"},{"UP","TOP CARD"},{"B","BACK"}},
        {{"UP/DOWN","SCROLL"},{"B/START","DUEL"}},
        {{"UP/DOWN","CHANNEL"},{"A","MUTE/UNMUTE"},{"START","PAUSE/RESUME"},{"LEFT/RIGHT","TEMPO"},{"B","MENU"}},
        {{"UP/DOWN","EFFECT"},{"A","PLAY SOUND"},{"B","SOUND TEST"}}
    };
    static const char *const notes[] = {
        0,0,"CANCEL BEFORE COMMITTING",0,"FROM FIELD: PRESS START",
        "DOWN CYCLES STACK CARDS","FROM DUEL: PRESS START",
        "EFFECTS ROW: A OPENS","SOUND TEST > EFFECTS"
    };
    uint8_t i;
    header("HOW TO PLAY"); gfx_box(0,16,160,104);
    if (!help_open) {
        for (i = 0; i < HELP_TOPICS; ++i) text(18,24+i*10,136,topics[i]);
        cursor(8,24+ui_selection*10);
        footer("UP/DOWN: TOPIC","A: READ   B: MENU");
        return;
    }
    position(ui_selection,HELP_TOPICS,0);
    text(8,24,144,topics[ui_selection]); gfx_rect(8,36,144,1,1);
    for (i = 0; i < 6 && controls[ui_selection][i][0]; ++i) {
        text(8,44+i*12,66,controls[ui_selection][i][0]);
        text(80,44+i*12,72,controls[ui_selection][i][1]);
    }
    if (notes[ui_selection]) text(8,108,144,notes[ui_selection]);
    footer("LEFT/RIGHT: TOPIC","A: NEXT   B: TOPICS");
}

void ui_draw(void) DG_BANKED {
    gfx_begin();
    switch (ui_screen) {
    case S_TITLE: title_screen(); break;
    case S_MENU: start_menu(); break;
    case S_DUEL: duel(); break;
    case S_HAND: hand(); break;
    case S_ACTION: actions(); break;
    case S_TARGET: case S_ATTACK: targets(); break;
    case S_COMMIT: confirmation(); break;
    case S_FIELD: field(); break;
    case S_CARD: card(); break;
    case S_BOARD: board(); break;
    case S_LOG: log_screen(); break;
    case S_EVENT: event_screen(); break;
    case S_SOUND: sound(); break;
    case S_SFX: {
        static const char *const names[] = {"CARD PLAY","ATTACK","BLOCK","DIGIVOLUTION","SECURITY","DELETION","VICTORY","DEFEAT","GAIA FORCE","BOOT INTRO"};
        uint8_t i;
        header("SOUND EFFECTS");
        gfx_box(0,24,160,120);
        for (i = 0; i < SFX_COUNT-1; ++i) text(16,32+i*10,136,names[i]);
        cursor(6,32+ui_selection*10);
        break;
    }
    case S_QUIT:
        header("LEAVE THIS DUEL?"); gfx_box(0,24,160,120);
        wrap(8,40,144,5,"THIS PRACTICE DUEL WILL BE LOST. RETURN TO THE TITLE SCREEN?");
        confirm_controls();
        break;
    case S_NOTICE:
        header("DUEL GUIDE"); gfx_box(0,24,160,120); wrap(8,40,144,6,notice);
        break;
    case S_HELP:
        help(); break;
    }
    gfx_present();
}

void ui_refresh_cursor(void) DG_BANKED {
    switch (ui_screen) {
    case S_MENU: cursor(14,72+title_selection*12); break;
    case S_ACTION: cursor(6,80+ui_selection*12); break;
    case S_SOUND: cursor(6,40+ui_selection*14); break;
    case S_SFX: cursor(6,32+ui_selection*10); break;
    case S_HELP: cursor(8,24+ui_selection*10); break;
    case S_DUEL:
        if (digi_game.phase == P_MAIN) cursor((command&1) ? 86 : 6,116+(command/2)*12);
        else cursor(6,40+(ui_selection%5)*12);
        break;
    case S_TARGET: case S_ATTACK: cursor(6,40+(ui_selection%5)*12); break;
    case S_FIELD: cursor(6,30+(ui_selection%4)*22); break;
    }
    gfx_cursor_present();
}
