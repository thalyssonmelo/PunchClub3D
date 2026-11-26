// ============================================================================
//  AI.cpp - Implementacao da IA do adversario
// ============================================================================
#include "AI.h"
#include <cmath>

static const float TYPE_WINDUP_MULT[NUM_PUNCH] = { 0.85f, 1.15f, 1.25f, 1.0f };

const char* OpponentAI::modeName() const {
    switch (mode) {
        case NEUTRO:      return "NEUTRO";
        case ATACANDO:    return "ATACANDO";
        case DEFENDENDO:  return "DEFENDENDO";
        case RECUPERANDO: return "RECUPERANDO";
        case ATORDOADO:   return "ATORDOADO";
        case CAIDO:       return "CAIDO";
    }
    return "?";
}

void OpponentAI::reset() {
    mode = NEUTRO;
    queue.clear(); pending.clear();
    thinkTimer = 0.8f; actionTimer = defenseTimer = guardTimer = 0;
    circleTimer = 0; counterQueued = false;
    thought = "estudando o adversario";
}

// ---------------------------------------------------------------------------
// APRENDIZADO
// ---------------------------------------------------------------------------
void OpponentAI::observePlayerPunch(int hand, PunchType t, float gameTime) {
    pressure += 1.0f;
    int sym = hand * 4 + (int)t;
    if (gameTime - lastPlayerPunchTime > 1.5f) lastPlayerSym = 8; // nova sequencia
    // Registra a previsao feita ANTES de saber o golpe (para medir acuracia e reagir)
    curPred = predictNext(curPredProb, curPredSamples);
    if (curPredSamples >= 5 && curPred >= 0) {
        predTotal++;
        if (curPred % 4 == (int)t) predHits++;
    }
    markov[lastPlayerSym][sym]++;
    lastPlayerSym = sym;
    lastPlayerPunchTime = gameTime;
}

int OpponentAI::predictNext(float& prob, int& samples) const {
    const int* row = markov[lastPlayerSym];
    int total = 0, best = -1, bestC = 0;
    // agrupa por tipo de golpe (mao esquerda/direita somadas)
    int byType[NUM_PUNCH] = {};
    for (int i = 0; i < 8; ++i) { total += row[i]; byType[i % 4] += row[i]; }
    for (int t = 0; t < NUM_PUNCH; ++t) if (byType[t] > bestC) { bestC = byType[t]; best = t; }
    samples = total;
    prob = total > 0 ? bestC / (float)total : 0.0f;
    return best;
}

void OpponentAI::observeMyResult(PunchType t, int result) {
    tries[t] += 1.0f;
    if (result == 0) success[t] += 1.0f;
    else if (result == 1) success[t] += 0.2f; // bloqueado ainda gasta folego do jogador
}

void OpponentAI::observePlayerCounter() { playerCounters++; }

// Ao apanhar, lutadores defensivos se cobrem (como no boxe real e no Wii Sports)
void OpponentAI::observeHitTaken(Fighter& me) {
    recentHits += 1.0f;
    if (me.stunTimer > 0 || mode == ATACANDO) return;
    if (chance(p.defense * 0.6f + recentHits * 0.12f)) {
        queue.clear();
        me.setGuard(GUARD_HIGH);
        me.leanTarget = 0; me.duckTarget = 0;
        guardTimer = randf(0.6f, 1.1f);
        mode = DEFENDENDO;
        defenseTimer = guardTimer;
        thought = "cobrindo a cabeca";
    }
}

// Escolha do golpe: preferencia da personalidade + UCB1 + contexto
PunchType OpponentAI::chooseType(const Fighter& me, const Fighter& player) const {
    float w[NUM_PUNCH];
    float T = 0;
    for (int t = 0; t < NUM_PUNCH; ++t) T += tries[t];
    float L = p.learning;
    for (int t = 0; t < NUM_PUNCH; ++t) {
        float ucb = successRate(t) + 0.35f * std::sqrt(std::log(T + 2.0f) / (tries[t] + 1.0f));
        w[t] = p.prefer[t] * (1.0f - L) + L * ucb * 2.0f;
    }
    // Contexto: le a postura atual do jogador
    if (player.guardAmt > 0.6f && player.guardLow < 0.5f) {
        w[CORPO] *= 2.2f; w[UPPER] *= 1.5f; w[RETO] *= 0.5f; w[GANCHO] *= 0.7f;
    } else if (player.guardLow > 0.5f) {
        w[RETO] *= 2.0f; w[GANCHO] *= 1.8f; w[CORPO] *= 0.2f;
    }
    if (player.stunTimer > 0) { w[UPPER] *= 2.0f; w[GANCHO] *= 1.5f; }
    if (me.stamina < me.maxStamina * 0.35f) { w[UPPER] *= 0.5f; w[RETO] *= 1.5f; } // golpes baratos

    float sum = 0;
    for (float x : w) sum += x;
    float r = randf(0, sum);
    for (int t = 0; t < NUM_PUNCH; ++t) { r -= w[t]; if (r <= 0) return (PunchType)t; }
    return RETO;
}

void OpponentAI::buildCombo(const Fighter& me, const Fighter& player) {
    queue.clear();
    bool fury = me.furyTimer > 0;
    int maxLen = fury ? 4 : 3;
    int n = 1;
    while (n < maxLen && chance(p.combo + (fury ? 0.5f : 0.0f))) n++;
    int hand = chance(0.6f) ? 0 : 1;
    if (p.feint > 0 && chance(p.feint)) {
        queue.push_back({ hand, RETO, 0, true, false, false });
        thought = "finta...";
        hand = 1 - hand;
    } else {
        thought = n > 1 ? "combinacao de " + std::to_string(n) + " golpes" : "ataque";
    }
    for (int i = 0; i < n; ++i) {
        PunchType t = chooseType(me, player);
        if (i == 0 && t == RETO && queue.empty()) hand = 0; // jab de abertura
        bool follow = i > 0 || !queue.empty();
        queue.push_back({ hand, t, i == 0 ? 0.02f : randf(0.06f, 0.16f), false, follow, false });
        hand = 1 - hand;
    }
}

// Reacao a um golpe percebido
void OpponentAI::react(Fighter& me, const Fighter& player, int hand, PunchType t) {
    float c = p.defense;
    if (me.stamina < me.maxStamina * 0.25f) c *= 0.6f;
    if (mode == RECUPERANDO) c = std::min(1.0f, c + 0.2f);
    bool busy = me.hands[0].phase == HAND_EXTEND || me.hands[1].phase == HAND_EXTEND;
    if (busy) c *= 0.2f;  // no meio do proprio soco nao da para defender
    if (!chance(c)) { thought = "nao reagiu a tempo"; return; }

    // Aborta o proprio preparo para defender
    for (int k = 0; k < 2; ++k) if (me.hands[k].phase == HAND_WINDUP) me.beginRetract(k);
    queue.clear();

    float side = hand == 0 ? -1.0f : 1.0f;
    float leanAway = -signf(dot(player.right() * side, me.right()));
    float wDodge = 0, wDuck = 0, wBlock = 0, wLow = 0;
    switch (t) {
        case RETO:   wDodge = 0.3f + p.counter; wDuck = 0.2f + p.counter * 0.5f; wBlock = 0.4f + (1 - p.counter) * 0.5f; break;
        case GANCHO: wDuck = 0.4f + p.counter * 0.4f; wDodge = 0.3f; wBlock = 0.4f + (1 - p.counter) * 0.4f; break;
        case UPPER:  wDodge = 0.8f; wBlock = 0.3f; break;
        case CORPO:  wLow = 1.0f; wDodge = 0.05f; break;
        default: break;
    }
    float sum = wDodge + wDuck + wBlock + wLow;
    float r = randf(0, sum);
    if ((r -= wDodge) <= 0) {
        me.setGuard(GUARD_NONE);
        float dir = (t == GANCHO) ? leanAway : (chance(0.5f) ? 1.0f : -1.0f);
        me.leanTarget = dir * randf(0.85f, 1.0f);
        me.duckTarget = 0;
        thought = std::string("esquiva lateral (") + punchName(t) + ")";
    } else if ((r -= wDuck) <= 0) {
        me.setGuard(GUARD_NONE);
        me.duckTarget = 1; me.leanTarget = 0;
        thought = std::string("abaixou (") + punchName(t) + ")";
    } else if ((r -= wBlock) <= 0) {
        me.setGuard(GUARD_HIGH); guardTimer = 0.5f;
        me.leanTarget = 0; me.duckTarget = 0;
        thought = std::string("guarda alta (") + punchName(t) + ")";
    } else {
        me.setGuard(GUARD_LOW); guardTimer = 0.5f;
        me.leanTarget = 0; me.duckTarget = 0;
        thought = "guarda baixa (Corpo)";
    }
    mode = DEFENDENDO;
    defenseTimer = 0.45f;
}

// ---------------------------------------------------------------------------
void OpponentAI::update(float dt, Fighter& me, const Fighter& player) {
    time += dt;
    pressure = std::max(0.0f, pressure - dt * 0.8f);
    recentHits = std::max(0.0f, recentHits - dt * 0.5f);
    if (me.status != ST_ACTIVE) { mode = CAIDO; queue.clear(); pending.clear(); return; }
    if (me.stunTimer > 0) { mode = ATORDOADO; queue.clear(); thought = "atordoado!"; return; }
    if (mode == ATORDOADO || mode == CAIDO) { mode = NEUTRO; thinkTimer = 0.2f; }

    // ---------------- PERCEPCAO (com atraso de reacao) ----------------
    for (int h = 0; h < 2; ++h) {
        const Hand& ph = player.hands[h];
        if (ph.attacking() && ph.id != lastSeenId[h]) {
            lastSeenId[h] = ph.id;
            float r = p.reaction * randf(0.8f, 1.25f);
            PunchType seen = chance(p.readSkill) ? ph.type : (PunchType)randi(0, 3);
            bool predicted = false;
            // Antecipacao: se o modelo de Markov tem confianca, reage antes
            if (p.learning > 0 && curPredSamples >= 5 && curPredProb > 0.45f &&
                chance(p.learning * curPredProb)) {
                r *= 0.4f;
                seen = (PunchType)curPred;
                predicted = true;
            }
            pending.push_back({ ph.id, h, seen, r, predicted });
        }
    }
    for (size_t i = 0; i < pending.size();) {
        Pending& pd = pending[i];
        pd.timeLeft -= dt;
        if (pd.timeLeft <= 0) {
            const Hand& ph = player.hands[pd.hand];
            if (ph.id == pd.id && ph.attacking() && !ph.resolved) {
                react(me, player, pd.hand, pd.seenType);
                if (pd.predicted) thought = "previ seu " + std::string(punchName(pd.seenType)) + "!";
            }
            pending.erase(pending.begin() + i);
        } else ++i;
    }

    // ---------------- CONTRA-ATAQUE apos esquiva ----------------
    if (me.counterTimer > 0 && !counterQueued && mode != ATACANDO) {
        counterQueued = true;
        if (chance(0.25f + p.counter * 0.75f)) {
            queue.clear();
            PunchType t = player.duck > 0.4f ? UPPER : (chance(0.5f) ? GANCHO : RETO);
            int hand = me.lean > 0 ? 0 : 1;
            queue.push_back({ hand, t, 0.0f, false, false, true });
            if (chance(p.combo)) queue.push_back({ 1 - hand, RETO, 0.08f, false, true, false });
            me.leanTarget = 0; me.duckTarget = 0;
            mode = ATACANDO; actionTimer = 0;
            thought = "CONTRA-ATAQUE!";
        }
    }
    if (me.counterTimer <= 0) counterQueued = false;

    guardTimer -= dt;

    // ---------------- MAQUINA DE ESTADOS ----------------
    switch (mode) {
    case DEFENDENDO:
        defenseTimer -= dt;
        // Atras da guarda, espera o jogador errar ou cansar para sair batendo
        if ((player.exposedTimer > 0 || player.stamina < player.maxStamina * 0.2f) && queue.empty() &&
            chance(dt * 5.0f * (p.aggression + p.counter))) {
            PunchType t = chooseType(me, player);
            queue.push_back({ chance(0.5f) ? 0 : 1, t, 0.0f, false, false, true });
            if (chance(p.combo)) queue.push_back({ 1 - queue.back().hand, chooseType(me, player), 0.1f, false, true, false });
            me.leanTarget = 0; me.duckTarget = 0;
            mode = ATACANDO; actionTimer = 0;
            thought = player.exposedTimer > 0 ? "voce errou: puni!" : "voce cansou: hora de atacar";
            break;
        }
        if (defenseTimer <= 0) {
            me.leanTarget = 0; me.duckTarget = 0;
            if (guardTimer <= 0) me.setGuard(GUARD_NONE);
            mode = NEUTRO; thinkTimer = randf(0.05f, 0.2f);
        }
        break;

    case ATACANDO: {
        actionTimer -= dt;
        if (actionTimer <= 0 && !queue.empty()) {
            Action a = queue.front();
            if (!me.hands[a.hand].busy() && me.hands[1 - a.hand].phase != HAND_WINDUP) {
                float w = p.telegraph * TYPE_WINDUP_MULT[a.type];
                if (a.follow) w *= 0.7f;
                if (a.counter) w = p.telegraph * 0.45f;
                if (a.feint) w = p.telegraph * 0.8f;
                if (me.stamina < me.maxStamina * 0.3f) w *= 1.25f;
                if (me.furyTimer > 0) w *= 0.6f;
                if (me.startPunch(a.hand, a.type, w, a.feint)) {
                    queue.pop_front();
                    if (!a.feint) myAttacks++;
                    actionTimer = queue.empty() ? 0 : queue.front().delay;
                    if (a.feint) actionTimer = w + 0.02f; // espera a finta terminar
                }
            }
        }
        if (queue.empty() && !me.hands[0].busy() && !me.hands[1].busy()) {
            mode = NEUTRO;
            thinkTimer = randf(0.15f, 0.35f);
            // Aprendeu que o jogador contra-ataca? Fecha a guarda depois de atacar.
            float habit = playerCounters / (myAttacks + 2.0f);
            if (chance(p.defense * 0.35f + p.learning * habit * 2.5f)) {
                me.setGuard(GUARD_HIGH); guardTimer = 0.6f;
                if (habit > 0.15f && p.learning > 0) thought = "aprendi: voce contra-ataca";
            }
        }
        break;
    }

    case RECUPERANDO:
        recoverTimer += dt;
        me.setGuard(GUARD_HIGH);
        me.leanTarget = std::sin(time * 2.0f) * 0.1f;
        if (me.stamina > me.maxStamina * 0.55f || recoverTimer > 3.0f) {
            mode = NEUTRO; thinkTimer = 0.1f;
        }
        break;

    case NEUTRO: {
        // Movimentacao lateral (circular pelo ringue) e balanco do tronco
        circleTimer -= dt;
        if (circleTimer <= 0) { circleDir = chance(0.5f) ? 1.0f : -1.0f; circleTimer = randf(0.8f, 2.0f); }
        me.base.x = clampf(me.base.x + circleDir * 0.22f * dt, -0.45f, 0.45f);
        if (guardTimer <= 0) { me.leanTarget = std::sin(time * 1.7f) * 0.12f * (p.counter + 0.3f); me.duckTarget = 0; }

        thinkTimer -= dt;
        if (thinkTimer > 0) break;
        thinkTimer = randf(0.12f, 0.3f);

        float sr = me.stamina / me.maxStamina;
        if (sr < 0.22f) {
            mode = RECUPERANDO; recoverTimer = 0;
            thought = "sem folego, recuperando";
            break;
        }
        if (guardTimer <= 0) {
            // Sob pressao (jogador socando muito) a IA tende a fechar a guarda
            float g = p.defense * (0.4f + std::min(0.6f, pressure * 0.12f));
            if (chance(g)) { me.setGuard(GUARD_HIGH); guardTimer = randf(0.4f, 1.0f); }
            else me.setGuard(GUARD_NONE);
        }
        if (me.fury >= 100 && chance(0.5f)) {
            me.furyTimer = 3.5f; me.fury = 0;
            buildCombo(me, player);
            thought = "MODO FURIA!";
            mode = ATACANDO; actionTimer = 0;
            break;
        }
        float atk = p.aggression * 0.45f * (0.4f + 0.6f * sr);
        if (player.exposedTimer > 0) atk *= 2.5f;
        if (player.stunTimer > 0) atk *= 3.0f;
        if (player.stamina < player.maxStamina * 0.25f) atk *= 1.8f;   // pune quem cansou
        if (me.guardMode != GUARD_NONE && pressure < 0.5f) atk *= 1.3f; // jogador parou: hora de atacar
        if (chance(atk)) {
            buildCombo(me, player);
            mode = ATACANDO; actionTimer = 0;
        }
        break;
    }
    default: break;
    }
}
