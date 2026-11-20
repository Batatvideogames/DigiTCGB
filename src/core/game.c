#ifdef __SDCC
#pragma bank 1
#endif
#include "game.h"
#include <string.h>

static uint16_t random_next(Game *g) {
    g->rng ^= g->rng << 7;
    g->rng ^= g->rng >> 9;
    g->rng ^= g->rng << 8;
    return g->rng;
}

void dg_log(Game *g, uint8_t kind, uint8_t owner, uint8_t def) DG_BANKED {
    LogEntry *e = &g->log[g->log_head];
    e->kind = kind;
    e->player = owner;
    e->card = def;
    g->log_head = (g->log_head + 1) % 12;
    if (g->log_count < 12) ++g->log_count;
}

void dg_gain(Game *g, uint8_t owner, int8_t amount) DG_BANKED {
    int16_t n = g->memory;
    n += owner == g->active ? amount : -amount;
    if (n > 10) n = 10;
    if (n < -10) n = -10;
    g->memory = (int8_t)n;
}

uint8_t dg_count_zone(const Game *g, uint8_t owner, uint8_t zone) DG_BANKED {
    uint8_t i, n = 0;
    for (i = 0; i < DG_CARDS; ++i) if (g->players[owner].cards[i].zone == zone) ++n;
    return n;
}

uint8_t dg_nth_zone(const Game *g, uint8_t owner, uint8_t zone, uint8_t n) DG_BANKED {
    uint8_t i;
    for (i = 0; i < DG_CARDS; ++i) {
        if (g->players[owner].cards[i].zone == zone) {
            if (!n) return i;
            --n;
        }
    }
    return DG_NONE;
}

uint8_t dg_sources(const Game *g, uint8_t owner, uint8_t card) DG_BANKED {
    uint8_t n = 0;
    if (card == DG_NONE) return 0;
    card = g->players[owner].cards[card].under;
    while (card != DG_NONE && n < DG_CARDS) {
        ++n;
        card = g->players[owner].cards[card].under;
    }
    return n;
}

static uint8_t sourceless(const Game *g, uint8_t owner) {
    uint8_t i;
    for (i = 0; i < DG_CARDS; ++i) {
        const Card *c = &g->players[owner].cards[i];
        if (c->zone == Z_FIELD && c->under == DG_NONE) return 1;
    }
    return 0;
}

uint8_t dg_dp(const Game *g, uint8_t owner, uint8_t card, uint8_t opponent) DG_BANKED {
    const Player *p = &g->players[owner];
    const Card *c = &p->cards[card];
    uint8_t dp = dg_cards[c->def].dp, i, sources;
    if (c->zone != Z_FIELD) return dp;
    dp += c->dp_bonus;
    if (g->active != owner) return dp;
    sources = dg_sources(g, owner, card);
    for (i = 0; i < DG_CARDS; ++i) {
        if (p->cards[i].zone == Z_TAMER && p->cards[i].def == C_TAI) ++dp;
    }
    i = c->under;
    while (i != DG_NONE) {
        uint8_t def = p->cards[i].def;
        if (def == C_AGUMON || (def == C_KOROMON && sources >= 4)) ++dp;
        if (def == C_TSUNOMON && opponent != DG_NONE && g->players[owner ^ 1].cards[opponent].zone == Z_FIELD && g->players[owner ^ 1].cards[opponent].under == DG_NONE) ++dp;
        i = p->cards[i].under;
    }
    return dp;
}

uint8_t dg_security_attack(const Game *g, uint8_t owner, uint8_t card) DG_BANKED {
    const Player *p = &g->players[owner];
    const Card *c = &p->cards[card];
    uint8_t n = 1, i = c->under;
    if (g->active == owner) {
        n += p->security_bonus;
        if (c->def == C_WARGREYMON) n += dg_sources(g, owner, card) / 2;
    }
    while (i != DG_NONE) {
        if (p->cards[i].def == C_GREYMON) ++n;
        if (p->cards[i].def == C_WEREGARURUMON && owner == g->active && sourceless(g, owner ^ 1)) ++n;
        i = p->cards[i].under;
    }
    return n;
}

void dg_draw(Game *g, uint8_t owner, uint8_t mandatory) DG_BANKED {
    Player *p = &g->players[owner];
    uint8_t card;
    if (!p->deck_count) {
        if (mandatory) {
            g->winner = owner ^ 1;
            g->phase = P_OVER;
            dg_log(g, L_WIN, g->winner, DG_NONE);
        }
        return;
    }
    card = p->deck[--p->deck_count];
    p->cards[card].zone = Z_HAND;
    p->hand[p->hand_count++] = card;
    dg_log(g, L_DRAW, owner, DG_NONE);
}

static void begin_turn(Game *g) {
    uint8_t i, p = g->active;
    g->controller = p;
    g->phase = P_BREED;
    for (i = 0; i < DG_CARDS; ++i) {
        g->players[p].cards[i].flags = 0;
    }
    if (sourceless(g, p ^ 1)) {
        for (i = 0; i < DG_CARDS; ++i) {
            if (g->players[p].cards[i].zone == Z_TAMER && g->players[p].cards[i].def == C_MATT) dg_gain(g, p, 1);
        }
    }
    dg_log(g, L_TURN, p, DG_NONE);
    if (g->turn > 1) dg_draw(g, p, 1);
}

void dg_init(Game *g, uint16_t seed, uint8_t human_deck, uint8_t first) DG_BANKED {
    uint8_t owner, def, n, i, j, id, swap;
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0xA735;
    g->winner = DG_NONE;
    g->active = first & 1;
    g->turn = 1;
    g->attacker = DG_NONE;
    g->effect_source = DG_NONE;
    for (owner = 0; owner < 2; ++owner) {
        Player *p = &g->players[owner];
        uint8_t color = human_deck ^ owner;
        id = 0;
        for (def = color * 16; def < color * 16 + 16; ++def) {
            for (n = 0; n < dg_cards[def].copies; ++n) {
                Card *c = &p->cards[id];
                c->def = def;
                c->under = DG_NONE;
                c->zone = dg_cards[def].kind == DG_EGG ? Z_EGGS : Z_DECK;
                if (c->zone == Z_DECK) p->deck[p->deck_count++] = id;
                ++id;
            }
        }
        for (i = p->deck_count - 1; i > 0; --i) {
            j = random_next(g) % (i + 1);
            swap = p->deck[i]; p->deck[i] = p->deck[j]; p->deck[j] = swap;
        }
        for (i = 0; i < 5; ++i) {
            id = p->deck[--p->deck_count];
            p->cards[id].zone = Z_SECURITY;
            p->security[p->security_count++] = id;
        }
        for (i = 0; i < 5; ++i) dg_draw(g, owner, 0);
    }
    dg_log(g, L_START, first & 1, DG_NONE);
    begin_turn(g);
}

void dg_finish(Game *g) DG_BANKED {
    uint8_t p, i;
    if (g->phase == P_OVER) return;
    g->pending_count = 0;
    g->attacker = DG_NONE;
    g->effect = E_NONE;
    g->phase = P_MAIN;
    g->controller = g->active;
    if (g->memory >= 0) return;
    for (p = 0; p < 2; ++p) {
        Player *player = &g->players[p];
        for (i = 0; i < DG_CARDS; ++i) {
            player->cards[i].dp_bonus = 0;
            if (player->cards[i].locked_until <= g->turn) player->cards[i].locked_until = 0;
        }
        if (player->security_dp_until <= g->turn) player->security_dp = 0;
        if (player->security_bonus_until <= g->turn) player->security_bonus = 0;
    }
    g->active ^= 1;
    g->memory = -g->memory;
    ++g->turn;
    begin_turn(g);
}

static uint8_t color_present(const Game *g, uint8_t owner, uint8_t color) {
    uint8_t i;
    for (i = 0; i < DG_CARDS; ++i) {
        const Card *c = &g->players[owner].cards[i];
        if ((c->zone == Z_FIELD || c->zone == Z_BREED || c->zone == Z_TAMER) && dg_cards[c->def].color == color) return 1;
    }
    return 0;
}

static uint16_t choice_scan(const Game *g, uint16_t wanted, uint8_t size, Choice *out) {
    uint16_t seen = 0;
    uint8_t a, b, i, p = g->active;
    const Player *player = &g->players[p];
#define OFFER(k,x,y) do { if (out && seen >= wanted && seen - wanted < size) { Choice *slot = out + (seen - wanted); slot->kind=(k); slot->a=(x); slot->b=(y); } ++seen; } while (0)
    if (g->phase == P_BREED) {
        a = dg_nth_zone(g, p, Z_BREED, 0);
        if (a == DG_NONE) {
            b = dg_nth_zone(g, p, Z_EGGS, 0);
            if (b != DG_NONE) OFFER(A_HATCH, b, DG_NONE);
        } else if (dg_cards[player->cards[a].def].kind == DG_DIGIMON) OFFER(A_RAISE, a, DG_NONE);
        OFFER(A_SKIP, DG_NONE, DG_NONE);
    } else if (g->phase == P_MAIN) {
        for (i = 0; i < player->hand_count; ++i) {
            const CardDef *def;
            a = player->hand[i];
            def = &dg_cards[player->cards[a].def];
            if (def->cost <= g->memory + 10 && (def->kind != DG_OPTION || color_present(g, p, def->color))) OFFER(A_PLAY, a, DG_NONE);
            if (def->kind != DG_DIGIMON || def->evo > g->memory + 10) continue;
            for (b = 0; b < DG_CARDS; ++b) {
                const Card *base = &player->cards[b];
                if ((base->zone == Z_FIELD || base->zone == Z_BREED) && dg_cards[base->def].level + 1 == def->level && dg_cards[base->def].color == def->color) OFFER(A_EVOLVE, a, b);
            }
        }
        for (a = 0; a < DG_CARDS; ++a) {
            const Card *c = &player->cards[a];
            if (c->zone != Z_FIELD || c->flags & (F_SUSPENDED | F_NEW) || c->locked_until) continue;
            OFFER(A_ATTACK, a, DG_NONE);
            for (b = 0; b < DG_CARDS; ++b) {
                const Card *foe = &g->players[p ^ 1].cards[b];
                if (foe->zone == Z_FIELD && (foe->flags & F_SUSPENDED)) OFFER(A_ATTACK, a, b);
            }
        }
        OFFER(A_PASS, DG_NONE, DG_NONE);
    } else if (g->phase == P_ORDER) {
        for (a = 0; a < g->pending_count; ++a) OFFER(A_EFFECT, a, DG_NONE);
    } else if (g->phase == P_TARGET) {
        for (a = 0; a < DG_CARDS; ++a) if (dg_target_legal(g, a)) OFFER(A_TARGET, a, DG_NONE);
        if (g->effect_optional) OFFER(A_DONE, DG_NONE, DG_NONE);
    } else if (g->phase == P_BLOCK) {
        for (a = 0; a < DG_CARDS; ++a) {
            const Card *c = &g->players[p ^ 1].cards[a];
            if (c->zone == Z_FIELD && !(c->flags & F_SUSPENDED) && !c->locked_until && (c->def == C_COREDRAMON || c->def == C_GRIZZLYMON)) OFFER(A_BLOCK, a, DG_NONE);
        }
        OFFER(A_NO_BLOCK, DG_NONE, DG_NONE);
    }
#undef OFFER
    return seen;
}

uint16_t dg_choice_count(const Game *g) DG_BANKED { return choice_scan(g, 0, 0, 0); }
uint8_t dg_choice_at(const Game *g, uint16_t index, Choice *out) DG_BANKED { return choice_scan(g, index, 1, out) > index; }
uint16_t dg_choice_window(const Game *g, uint16_t first, uint8_t size, Choice *out) DG_BANKED { return choice_scan(g, first, size, out); }

static void remove_hand(Player *p, uint8_t card) {
    uint8_t i;
    for (i = 0; i < p->hand_count; ++i) if (p->hand[i] == card) break;
    for (; i + 1 < p->hand_count; ++i) p->hand[i] = p->hand[i + 1];
    --p->hand_count;
}

uint8_t dg_apply(Game *g, const Choice *choice) DG_BANKED {
    uint16_t i, count = dg_choice_count(g);
    uint8_t legal = 0, p = g->active;
    Player *player = &g->players[p];
    Choice checks[16];
    Card *c;
    const CardDef *def;
    for (i = 0; i < count; ++i) {
        const Choice *check;
        if (!(i & 15)) dg_choice_window(g, i, 16, checks);
        check = &checks[i & 15];
        if (check->kind == choice->kind && check->a == choice->a && check->b == choice->b) { legal = 1; break; }
    }
    if (!legal) return 0;
    switch (choice->kind) {
    case A_HATCH:
        player->cards[choice->a].zone = Z_BREED;
        dg_log(g, L_HATCH, p, player->cards[choice->a].def);
        g->phase = P_MAIN;
        break;
    case A_RAISE:
        player->cards[choice->a].zone = Z_FIELD;
        dg_log(g, L_RAISE, p, player->cards[choice->a].def);
        g->phase = P_MAIN;
        break;
    case A_SKIP: g->phase = P_MAIN; break;
    case A_PASS:
        dg_log(g, L_PASS, p, DG_NONE);
        g->memory = -3;
        dg_finish(g);
        break;
    case A_PLAY:
        c = &player->cards[choice->a];
        def = &dg_cards[c->def];
        remove_hand(player, choice->a);
        dg_gain(g, p, -(int8_t)def->cost);
        c->under = DG_NONE;
        c->flags = F_NEW;
        c->dp_bonus = 0;
        c->locked_until = 0;
        c->zone = def->kind == DG_OPTION ? Z_TRASH : def->kind == DG_TAMER ? Z_TAMER : Z_FIELD;
        dg_log(g, L_PLAY, p, c->def);
        g->resume = R_MAIN;
        if (def->kind == DG_OPTION) dg_option(g, p, c->def, 0);
        else dg_finish(g);
        break;
    case A_EVOLVE: {
        Card *base = &player->cards[choice->b];
        c = &player->cards[choice->a];
        def = &dg_cards[c->def];
        remove_hand(player, choice->a);
        dg_gain(g, p, -(int8_t)def->evo);
        c->under = choice->b;
        c->zone = base->zone;
        c->flags = base->flags;
        c->dp_bonus = base->dp_bonus;
        c->locked_until = base->locked_until;
        base->zone = Z_SOURCE;
        base->flags = 0;
        base->dp_bonus = 0;
        base->locked_until = 0;
        dg_log(g, L_EVOLVE, p, c->def);
        dg_draw(g, p, 0);
        g->resume = R_MAIN;
        g->effect_card = c->def;
        if (c->zone == Z_FIELD && c->def == C_GARUDAMON) dg_set_effect(g, E_DP, p, 3, 0, 0);
        else if (c->zone == Z_FIELD && c->def == C_ZUDOMON) dg_set_effect(g, E_STRIP, p, 2, 7, 0);
        else dg_finish(g);
        break;
    }
    case A_ATTACK: dg_attack_begin(g, choice->a, choice->b); break;
    case A_EFFECT: dg_resolve_effect(g, choice->a); break;
    case A_TARGET: dg_target(g, choice->a); break;
    case A_DONE: g->effect = E_NONE; dg_continue(g); break;
    case A_BLOCK: dg_attack_block(g, choice->a); break;
    case A_NO_BLOCK: dg_attack_block(g, DG_NONE); break;
    default: return 0;
    }
    return 1;
}
