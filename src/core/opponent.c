#ifdef __SDCC
#pragma bank 3
#endif
#include "game.h"

static int16_t score(const Game *g, const Choice *c) {
    uint8_t p = g->controller, foe = p ^ 1;
    const Player *player = &g->players[p];
    const CardDef *def;
    int16_t value = -100;
    uint8_t a_dp, d_dp;
    switch (c->kind) {
    case A_HATCH: return 90;
    case A_RAISE: return 90;
    case A_SKIP: return 0;
    case A_PASS: return -10;
    case A_EFFECT: return 20;
    case A_DONE: return -10;
    case A_NO_BLOCK: return 0;
    case A_BLOCK:
        a_dp = dg_dp(g, foe, g->attacker, c->a);
        d_dp = dg_dp(g, p, c->a, g->attacker);
        if (!player->security_count && g->attack_target == DG_NONE) return 200;
        if (d_dp >= a_dp) return 60;
        if (g->attack_target == DG_NONE && player->security_count <= dg_security_attack(g, foe, g->attacker)) return 30;
        return -20;
    case A_ATTACK:
        a_dp = dg_dp(g, p, c->a, c->b);
        def = &dg_cards[player->cards[c->a].def];
        if (c->b == DG_NONE) {
            if (!g->players[foe].security_count) return 500;
            value = 30 + a_dp + 7 * dg_security_attack(g, p, c->a);
            if (def->kind == DG_DIGIMON && (def == &dg_cards[C_COREDRAMON] || def == &dg_cards[C_GRIZZLYMON])) value -= 25;
            return value;
        }
        d_dp = dg_dp(g, foe, c->b, c->a);
        if (a_dp > d_dp) return 65 + dg_sources(g, foe, c->b) * 4;
        if (a_dp == d_dp) return 20 + dg_sources(g, foe, c->b) * 4 - dg_sources(g, p, c->a) * 4;
        return -30;
    case A_EVOLVE:
        def = &dg_cards[player->cards[c->a].def];
        value = 55 + def->level * 2 - def->evo * 3;
        if (player->cards[c->b].zone == Z_BREED) value += 7;
        if (!(player->cards[c->b].flags & (F_NEW | F_SUSPENDED))) value += 8;
        return value;
    case A_PLAY:
        def = &dg_cards[player->cards[c->a].def];
        if (def->kind == DG_DIGIMON) return 45 + def->dp - def->cost * 5;
        if (def->kind == DG_TAMER) return 35;
        switch (player->cards[c->a].def) {
        case C_HAMMER: return 100;
        case C_GAIA: case C_COCYTUS: return dg_count_zone(g, foe, Z_FIELD) ? 45 : -40;
        case C_GIGA: {
            uint8_t i, n = 0;
            for (i = 0; i < DG_CARDS; ++i) if (g->players[foe].cards[i].zone == Z_FIELD && dg_dp(g, foe, i, DG_NONE) <= 4) ++n;
            return n ? 35 + n * 8 : -40;
        }
        case C_KAISER: return dg_count_zone(g, p, Z_SOURCE) > 1 ? 36 : -40;
        case C_STARLIGHT: return player->security_count ? 10 : -40;
        case C_SHADOW: return dg_count_zone(g, p, Z_FIELD) ? 12 : -40;
        case C_SORROW: return dg_count_zone(g, foe, Z_FIELD) ? 15 : -40;
        default: return -40;
        }
    case A_TARGET:
        if (g->effect == E_DP) return 10 + dg_dp(g, p, c->a, DG_NONE) + ((player->cards[c->a].flags & (F_NEW | F_SUSPENDED)) ? 0 : 30);
        if (g->effect == E_KAISER) return 10 + dg_cards[player->cards[c->a].def].cost * 5;
        if (g->effect == E_STRIP) return 10 + dg_sources(g, foe, c->a) * 10;
        return 10 + dg_cards[g->players[foe].cards[c->a].def].cost * 5 + dg_sources(g, foe, c->a) * 5;
    default: return value;
    }
}

uint8_t dg_ai_choose(const Game *g, Choice *choice) DG_BANKED {
    uint16_t i, count = dg_choice_count(g);
    int16_t best = -32767, value;
    Choice candidates[16];
    if (!count) return 0;
    for (i = 0; i < count; ++i) {
        const Choice *candidate;
        if (!(i & 15)) dg_choice_window(g, i, 16, candidates);
        candidate = &candidates[i & 15];
        value = score(g, candidate);
        if (value > best) { best = value; *choice = *candidate; }
    }
    return 1;
}
