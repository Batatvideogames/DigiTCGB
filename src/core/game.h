#ifndef DIGITCGB_GAME_H
#define DIGITCGB_GAME_H

#include <stdint.h>
#ifdef __SDCC
#define DG_BANKED __banked
#else
#define DG_BANKED
#endif

#define DG_CARDS 54
#define DG_NONE 255
#define DG_DEFINITIONS 32

enum { DG_EGG, DG_DIGIMON, DG_TAMER, DG_OPTION };
enum { Z_DECK, Z_EGGS, Z_HAND, Z_SECURITY, Z_BREED, Z_FIELD, Z_TAMER, Z_SOURCE, Z_TRASH, Z_CHECK };
enum { P_BREED, P_MAIN, P_ORDER, P_TARGET, P_BLOCK, P_OVER };
enum { A_HATCH, A_RAISE, A_SKIP, A_PLAY, A_EVOLVE, A_ATTACK, A_PASS, A_EFFECT, A_TARGET, A_DONE, A_BLOCK, A_NO_BLOCK };
enum { R_MAIN, R_ATTACK, R_SECURITY };
enum { F_SUSPENDED=1, F_NEW=2, F_USED=4 };
enum { E_NONE, E_DP, E_SECURITY_DP, E_DELETE_SMALL, E_DELETE, E_MEMORY, E_LOCK, E_KAISER, E_RETURN, E_STRIP, E_UNSUSPEND, E_ATTACK_COST };
enum { L_START, L_HATCH, L_RAISE, L_PLAY, L_EVOLVE, L_ATTACK, L_BLOCK, L_CHECK, L_DELETE, L_RETURN, L_STRIP, L_EFFECT, L_PASS, L_TURN, L_WIN, L_DRAW, L_COUNT };
enum { C_KOROMON, C_BIYOMON, C_AGUMON, C_DRACOMON, C_BIRDRAMON, C_COREDRAMON, C_GREYMON, C_GARUDAMON, C_METALGREYMON, C_PHOENIXMON, C_WARGREYMON, C_TAI, C_SHADOW, C_STARLIGHT, C_GIGA, C_GAIA, C_TSUNOMON, C_GOMAMON, C_GABUMON, C_BEARMON, C_IKKAKUMON, C_GARURUMON, C_GRIZZLYMON, C_WEREGARURUMON, C_ZUDOMON, C_PLESIOMON, C_METALGARURUMON, C_MATT, C_HAMMER, C_SORROW, C_KAISER, C_COCYTUS };

typedef struct {
    const char *number;
    const char *name;
    const char *text;
    uint8_t kind, color, level, cost, evo, dp, copies;
} CardDef;

typedef struct {
    uint8_t def, zone, under, flags;
    uint8_t dp_bonus;
    uint16_t locked_until;
} Card;

typedef struct {
    Card cards[DG_CARDS];
    uint8_t deck[50], hand[50], security[50];
    uint8_t deck_count, hand_count, security_count;
    uint8_t security_dp, security_bonus;
    uint16_t security_dp_until, security_bonus_until;
} Player;

typedef struct { uint8_t kind, a, b; } Choice;
typedef struct { uint8_t kind, player, card; } LogEntry;

typedef struct {
    Player players[2];
    uint16_t rng, turn;
    int8_t memory;
    uint8_t active, phase, controller, winner;
    uint8_t attacker, attack_target, checks, had_security;
    uint8_t pending[DG_CARDS], pending_count;
    uint8_t effect, effect_owner, effect_amount, effect_limit, effect_optional;
    uint8_t effect_card, resume, effect_source;
    LogEntry log[12];
    uint8_t log_count, log_head;
} Game;

extern const CardDef dg_cards[DG_DEFINITIONS];
void dg_init(Game *g, uint16_t seed, uint8_t human_deck, uint8_t first) DG_BANKED;
uint16_t dg_choice_count(const Game *g) DG_BANKED;
uint8_t dg_choice_at(const Game *g, uint16_t index, Choice *out) DG_BANKED;
uint16_t dg_choice_window(const Game *g, uint16_t first, uint8_t size, Choice *out) DG_BANKED;
uint8_t dg_apply(Game *g, const Choice *choice) DG_BANKED;
uint8_t dg_dp(const Game *g, uint8_t owner, uint8_t card, uint8_t opponent) DG_BANKED;
uint8_t dg_sources(const Game *g, uint8_t owner, uint8_t card) DG_BANKED;
uint8_t dg_security_attack(const Game *g, uint8_t owner, uint8_t card) DG_BANKED;
uint8_t dg_count_zone(const Game *g, uint8_t owner, uint8_t zone) DG_BANKED;
uint8_t dg_nth_zone(const Game *g, uint8_t owner, uint8_t zone, uint8_t n) DG_BANKED;
uint8_t dg_ai_choose(const Game *g, Choice *choice) DG_BANKED;

void dg_log(Game *g, uint8_t kind, uint8_t owner, uint8_t def) DG_BANKED;
void dg_gain(Game *g, uint8_t owner, int8_t amount) DG_BANKED;
void dg_draw(Game *g, uint8_t owner, uint8_t mandatory) DG_BANKED;
void dg_finish(Game *g) DG_BANKED;
void dg_delete(Game *g, uint8_t owner, uint8_t card, uint8_t bounce) DG_BANKED;
void dg_strip(Game *g, uint8_t owner, uint8_t card, uint8_t count) DG_BANKED;
void dg_continue(Game *g) DG_BANKED;
void dg_option(Game *g, uint8_t owner, uint8_t def, uint8_t security) DG_BANKED;
void dg_resolve_effect(Game *g, uint8_t index) DG_BANKED;
void dg_target(Game *g, uint8_t target) DG_BANKED;
uint8_t dg_target_legal(const Game *g, uint8_t target) DG_BANKED;
uint8_t dg_option_effect(uint8_t def) DG_BANKED;
uint8_t dg_option_target_legal(const Game *g, uint8_t owner, uint8_t def, uint8_t target) DG_BANKED;
void dg_attack_begin(Game *g, uint8_t attacker, uint8_t target) DG_BANKED;
void dg_attack_block(Game *g, uint8_t blocker) DG_BANKED;
void dg_set_effect(Game *g, uint8_t effect, uint8_t owner, uint8_t amount, uint8_t limit, uint8_t optional) DG_BANKED;

#endif
