#ifdef __SDCC
#pragma bank 2
#endif
#include "game.h"

static void clear_card(Card *c) {
    c->under = DG_NONE;
    c->flags = 0;
    c->dp_bonus = 0;
    c->locked_until = 0;
}

void dg_delete(Game *g, uint8_t owner, uint8_t card, uint8_t bounce) DG_BANKED {
    Player *p = &g->players[owner];
    uint8_t next, source = p->cards[card].under;
    dg_log(g, bounce ? L_RETURN : L_DELETE, owner, p->cards[card].def);
    while (source != DG_NONE) {
        next = p->cards[source].under;
        p->cards[source].zone = Z_TRASH;
        clear_card(&p->cards[source]);
        source = next;
    }
    clear_card(&p->cards[card]);
    p->cards[card].zone = bounce ? Z_HAND : Z_TRASH;
    if (bounce) p->hand[p->hand_count++] = card;
}

void dg_strip(Game *g, uint8_t owner, uint8_t card, uint8_t count) DG_BANKED {
    Player *p = &g->players[owner];
    uint8_t source, previous;
    while (count-- && p->cards[card].under != DG_NONE) {
        previous = card;
        source = p->cards[card].under;
        while (p->cards[source].under != DG_NONE) {
            previous = source;
            source = p->cards[source].under;
        }
        p->cards[previous].under = DG_NONE;
        p->cards[source].zone = Z_TRASH;
        clear_card(&p->cards[source]);
        dg_log(g, L_STRIP, owner, p->cards[source].def);
    }
}

static uint8_t source_on_field(const Game *g, uint8_t owner, uint8_t target) {
    uint8_t i, j;
    const Player *p = &g->players[owner];
    for (i = 0; i < DG_CARDS; ++i) {
        if (p->cards[i].zone != Z_FIELD) continue;
        j = p->cards[i].under;
        while (j != DG_NONE) {
            if (j == target) return 1;
            j = p->cards[j].under;
        }
    }
    return 0;
}

static uint8_t target_legal(const Game *g, uint8_t effect, uint8_t owner, uint8_t limit, uint8_t target) {
    const Card *c;
    if (target >= DG_CARDS) return 0;
    if (effect == E_DP) return g->players[owner].cards[target].zone == Z_FIELD;
    if (effect == E_KAISER) {
        c = &g->players[owner].cards[target];
        return c->zone == Z_SOURCE && dg_cards[c->def].kind == DG_DIGIMON && source_on_field(g, owner, target);
    }
    c = &g->players[owner ^ 1].cards[target];
    if (c->zone != Z_FIELD) return 0;
    switch (effect) {
    case E_DELETE_SMALL: return dg_dp(g, owner ^ 1, target, DG_NONE) <= 4;
    case E_DELETE: case E_RETURN: return 1;
    case E_LOCK: return c->under == DG_NONE;
    case E_STRIP: return dg_cards[c->def].level <= limit;
    default: return 0;
    }
}

uint8_t dg_target_legal(const Game *g, uint8_t target) DG_BANKED {
    return target_legal(g,g->effect,g->effect_owner,g->effect_limit,target);
}

uint8_t dg_option_effect(uint8_t def) DG_BANKED {
    switch (def) {
    case C_SHADOW: return E_DP;
    case C_GIGA: return E_DELETE_SMALL;
    case C_GAIA: return E_DELETE;
    case C_SORROW: return E_LOCK;
    case C_KAISER: return E_KAISER;
    case C_COCYTUS: return E_RETURN;
    default: return E_NONE;
    }
}

uint8_t dg_option_target_legal(const Game *g, uint8_t owner, uint8_t def, uint8_t target) DG_BANKED {
    return target_legal(g,dg_option_effect(def),owner,0,target);
}

void dg_set_effect(Game *g, uint8_t effect, uint8_t owner, uint8_t amount, uint8_t limit, uint8_t optional) DG_BANKED {
    uint8_t i;
    g->effect = effect;
    g->effect_owner = owner;
    g->effect_amount = amount;
    g->effect_limit = limit;
    g->effect_optional = optional;
    g->controller = owner;
    g->phase = P_TARGET;
    for (i = 0; i < DG_CARDS; ++i) if (dg_target_legal(g, i)) return;
    g->effect = E_NONE;
    dg_continue(g);
}

void dg_target(Game *g, uint8_t target) DG_BANKED {
    uint8_t p = g->effect_owner, i;
    Player *player = &g->players[p];
    switch (g->effect) {
    case E_DP: player->cards[target].dp_bonus += g->effect_amount; break;
    case E_DELETE_SMALL:
        dg_delete(g, p ^ 1, target, 0);
        if (--g->effect_amount) {
            for (i = 0; i < DG_CARDS; ++i) if (dg_target_legal(g, i)) return;
        }
        break;
    case E_DELETE: dg_delete(g, p ^ 1, target, 0); break;
    case E_RETURN: dg_delete(g, p ^ 1, target, 1); break;
    case E_STRIP: dg_strip(g, p ^ 1, target, g->effect_amount); break;
    case E_LOCK: g->players[p ^ 1].cards[target].locked_until = g->turn + 1; break;
    case E_KAISER:
        for (i = 0; i < DG_CARDS; ++i) {
            if (player->cards[i].under == target) {
                player->cards[i].under = player->cards[target].under;
                break;
            }
        }
        clear_card(&player->cards[target]);
        player->cards[target].zone = Z_FIELD;
        player->cards[target].flags = F_NEW;
        dg_log(g, L_PLAY, p, player->cards[target].def);
        break;
    default: break;
    }
    g->effect = E_NONE;
    dg_continue(g);
}

void dg_option(Game *g, uint8_t owner, uint8_t def, uint8_t security) DG_BANKED {
    Player *p = &g->players[owner];
    g->effect_card = def;
    dg_log(g, L_EFFECT, owner, def);
    switch (def) {
    case C_SHADOW:
        if (!security) { dg_set_effect(g, E_DP, owner, 3, 0, 0); return; }
        ++p->security_bonus;
        p->security_bonus_until = g->turn + 1;
        break;
    case C_STARLIGHT:
        p->security_dp += 7;
        p->security_dp_until = g->turn + (security ? 0 : 1);
        break;
    case C_GIGA: dg_set_effect(g, E_DELETE_SMALL, owner, 2, 4, 1); return;
    case C_GAIA: dg_set_effect(g, E_DELETE, owner, 1, 0, 0); return;
    case C_HAMMER: dg_gain(g, owner, security ? 2 : 1); break;
    case C_SORROW: dg_set_effect(g, E_LOCK, owner, 1, 0, 0); return;
    case C_KAISER: dg_set_effect(g, E_KAISER, owner, 1, 0, 0); return;
    case C_COCYTUS: dg_set_effect(g, E_RETURN, owner, 1, 0, 0); return;
    default: break;
    }
    dg_continue(g);
}

void dg_attack_begin(Game *g, uint8_t attacker, uint8_t target) DG_BANKED {
    Player *p = &g->players[g->active];
    Card *c = &p->cards[attacker];
    uint8_t i;
    c->flags |= F_SUSPENDED;
    g->attacker = attacker;
    g->attack_target = target;
    g->pending_count = 0;
    g->resume = R_ATTACK;
    dg_log(g, L_ATTACK, g->active, c->def);
    if (c->def == C_COREDRAMON || c->def == C_GRIZZLYMON || (c->def == C_METALGARURUMON && !(c->flags & F_USED))) g->pending[g->pending_count++] = attacker;
    i = c->under;
    while (i != DG_NONE) {
        if (p->cards[i].def == C_GABUMON || p->cards[i].def == C_GARURUMON) g->pending[g->pending_count++] = i;
        i = p->cards[i].under;
    }
    dg_continue(g);
}

void dg_resolve_effect(Game *g, uint8_t index) DG_BANKED {
    uint8_t card = g->pending[index], i, p = g->active;
    uint8_t def = g->players[p].cards[card].def;
    for (i = index; i + 1 < g->pending_count; ++i) g->pending[i] = g->pending[i + 1];
    --g->pending_count;
    g->effect_source = card;
    g->effect_card = def;
    dg_log(g, L_EFFECT, p, def);
    if (def == C_GABUMON || def == C_GARURUMON) {
        dg_set_effect(g, E_STRIP, p, 1, def == C_GABUMON ? 5 : 7, 0);
        return;
    }
    if (def == C_METALGARURUMON) {
        g->players[p].cards[card].flags &= (uint8_t)~F_SUSPENDED;
        g->players[p].cards[card].flags |= F_USED;
    } else if (def == C_COREDRAMON || def == C_GRIZZLYMON) dg_gain(g, p, -2);
    dg_continue(g);
}

void dg_attack_block(Game *g, uint8_t blocker) DG_BANKED {
    uint8_t p = g->active, i, attack_dp, defend_dp;
    Card *attacker = &g->players[p].cards[g->attacker];
    if (blocker != DG_NONE) {
        g->players[p ^ 1].cards[blocker].flags |= F_SUSPENDED;
        g->attack_target = blocker;
        dg_log(g, L_BLOCK, p ^ 1, g->players[p ^ 1].cards[blocker].def);
        i = attacker->under;
        while (i != DG_NONE) {
            if (g->players[p].cards[i].def == C_METALGREYMON) dg_gain(g, p, 3);
            i = g->players[p].cards[i].under;
        }
    }
    if (attacker->zone != Z_FIELD) { dg_finish(g); return; }
    if (g->attack_target != DG_NONE) {
        i = g->attack_target;
        if (g->players[p ^ 1].cards[i].zone != Z_FIELD) { dg_finish(g); return; }
        attack_dp = dg_dp(g, p, g->attacker, i);
        defend_dp = dg_dp(g, p ^ 1, i, g->attacker);
        if (attack_dp <= defend_dp) dg_delete(g, p, g->attacker, 0);
        if (defend_dp <= attack_dp) dg_delete(g, p ^ 1, i, 0);
        dg_finish(g);
        return;
    }
    if (!dg_security_attack(g, p, g->attacker)) { dg_finish(g); return; }
    if (!g->players[p ^ 1].security_count) {
        g->winner = p;
        g->phase = P_OVER;
        dg_log(g, L_WIN, p, DG_NONE);
        return;
    }
    g->checks = 0;
    g->resume = R_SECURITY;
    dg_continue(g);
}

void dg_continue(Game *g) DG_BANKED {
    uint8_t p = g->active, i, def, card;
    Player *foe = &g->players[p ^ 1];
    if (g->phase == P_OVER) return;
    if (g->resume == R_MAIN) { dg_finish(g); return; }
    if (g->resume == R_ATTACK) {
        if (g->pending_count) {
            g->phase = P_ORDER;
            g->controller = p;
            return;
        }
        if (g->players[p].cards[g->attacker].zone != Z_FIELD) { dg_finish(g); return; }
        g->phase = P_BLOCK;
        g->controller = p ^ 1;
        for (i = 0; i < DG_CARDS; ++i) {
            const Card *c = &foe->cards[i];
            if (c->zone == Z_FIELD && !(c->flags & F_SUSPENDED) && !c->locked_until && (c->def == C_COREDRAMON || c->def == C_GRIZZLYMON)) return;
        }
        dg_attack_block(g, DG_NONE);
        return;
    }
    while (g->players[p].cards[g->attacker].zone == Z_FIELD && foe->security_count && g->checks < dg_security_attack(g, p, g->attacker)) {
        card = foe->security[--foe->security_count];
        def = foe->cards[card].def;
        foe->cards[card].zone = Z_TRASH;
        ++g->checks;
        dg_log(g, L_CHECK, p ^ 1, def);
        if (dg_cards[def].kind == DG_DIGIMON) {
            if (dg_cards[def].dp + foe->security_dp >= dg_dp(g, p, g->attacker, DG_NONE)) dg_delete(g, p, g->attacker, 0);
        } else if (dg_cards[def].kind == DG_TAMER) {
            foe->cards[card].zone = Z_TAMER;
            clear_card(&foe->cards[card]);
            dg_log(g, L_PLAY, p ^ 1, def);
        } else {
            dg_option(g, p ^ 1, def, 1);
            return;
        }
    }
    dg_finish(g);
}
