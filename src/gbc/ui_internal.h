#ifndef DIGITCGB_UI_INTERNAL_H
#define DIGITCGB_UI_INTERNAL_H
#include "ui.h"
enum { S_TITLE, S_DUEL, S_FIELD, S_CARD, S_LOG, S_SOUND, S_HELP, S_QUIT, S_HAND, S_ACTION, S_TARGET, S_COMMIT, S_ATTACK, S_BOARD, S_NOTICE, S_MENU, S_EVENT, S_SFX };
enum { M_ALL, M_PLAY, M_EVOLVE, M_ATTACKERS, M_ATTACK_TARGETS, M_OPTION_TARGETS };
enum { HELP_TOPICS = 9 };
extern Game digi_game;
extern uint8_t ui_screen, ui_auto, title_selection, command, browse;
extern uint8_t help_open;
extern uint8_t inspector_owner, inspector_root, inspector_card, card_page, card_return;
extern uint8_t selected_card, action_mode, log_scroll, board_owner;
extern uint8_t board_selection[2], event_index, event_count;
extern uint8_t event_attack_target;
extern uint8_t evolution_drew, evolution_draw_card;
extern LogEntry ui_events[12];
extern uint8_t hand_play_allowed, hand_evolve_allowed;
extern uint16_t ui_selection;
extern Choice pending_choice;
extern char notice[96];
extern const uint8_t browse_owners[], browse_zones[];
extern const char *const browse_names[], *const log_names[];
uint16_t ui_choices(uint8_t mode, uint16_t wanted, Choice *out) DG_BANKED;
uint8_t ui_target_owner(void) DG_BANKED;
uint8_t ui_option_owner(void) DG_BANKED;
uint8_t ui_choice_def(const Choice *c) DG_BANKED;
uint8_t ui_hand_actions(uint8_t index) DG_BANKED;
uint8_t ui_cost(const Choice *c) DG_BANKED;
uint8_t ui_board_count(uint8_t owner) DG_BANKED;
uint8_t ui_board_card(uint8_t owner, uint8_t index) DG_BANKED;
uint8_t ui_text_page(uint8_t def, uint8_t page, char *out) DG_BANKED;
void ui_draw(void) DG_BANKED;
void ui_refresh_cursor(void) DG_BANKED;
void ui_refresh_hand(void) DG_BANKED;
#endif
