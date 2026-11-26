// ============================================================================
//  combat_test.cpp - Teste automatico (sem janela) da deteccao de colisoes.
//  Simula cada golpe contra cada postura defensiva e imprime a tabela de
//  resultados. Compilar com:  make test
// ============================================================================
#include "../src/Combat.h"
#include <cstdio>

static const char* kindName(HitKind k) {
    switch (k) { case HIT_BLOCK: return "BLOQ"; case HIT_HEAD: return "CABECA";
                 case HIT_BODY: return "CORPO"; default: return "erro"; }
}

struct Stance { const char* name; float lean, duck; GuardMode g; };

// Atacante golpeia; o defensor assume a postura 'react' segundos apos o
// inicio do preparo do golpe.
static HitKind simulate(bool aiAttacks, int hand, PunchType type, const Stance& st, float react, float windup) {
    Fighter pl, op;
    pl.base = Vec3(0, 0, 0);     pl.facing = -1; pl.setupStats(5, 5, 5);
    op.base = Vec3(0, 0, -1.2f); op.facing = +1; op.setupStats(5, 5, 5);
    pl.resetForFight(); op.resetForFight();
    Fighter& A = aiAttacks ? op : pl;
    Fighter& D = aiAttacks ? pl : op;
    const float dt = 1.0f / 240.0f;
    for (int i = 0; i < 60; ++i) { A.update(dt, D.headPos(), D.torsoPos()); D.update(dt, A.headPos(), A.torsoPos()); }
    A.startPunch(hand, type, windup);
    float time = 0;
    for (int i = 0; i < 400; ++i) {
        time += dt;
        if (time >= react) { D.leanTarget = st.lean; D.duckTarget = st.duck; D.guardMode = st.g; D.guardRaisedTime = 1; }
        A.update(dt, D.headPos(), D.torsoPos());
        D.update(dt, A.headPos(), A.torsoPos());
        D.guardRaisedTime = 1; // ignora bloqueio perfeito no teste
        HitInfo hi = Combat::testPunch(A, hand, D);
        if (hi.kind != HIT_NONE) return hi.kind;
        A.finishExtends();
        if (A.hands[hand].phase == HAND_RETRACT || A.hands[hand].phase == HAND_IDLE) {
            if (i > 5) return HIT_NONE;
        }
    }
    return HIT_NONE;
}

int main() {
    Stance stances[] = {
        { "parado",      0, 0, GUARD_NONE },
        { "guarda alta", 0, 0, GUARD_HIGH },
        { "guarda baixa",0, 0, GUARD_LOW  },
        { "incl. esq",  -1, 0, GUARD_NONE },
        { "incl. dir",   1, 0, GUARD_NONE },
        { "abaixar",     0, 1, GUARD_NONE },
    };
    for (int who = 0; who < 2; ++who) {
        bool ai = who == 0;
        float windup = ai ? 0.5f : -1;
        float react = ai ? 0.25f : 0.0f;
        std::printf("\n=== %s ataca (defensor reage %.2fs apos inicio do preparo) ===\n",
                    ai ? "ADVERSARIO" : "JOGADOR", react);
        std::printf("%-18s", "golpe");
        for (auto& s : stances) std::printf("%-13s", s.name);
        std::printf("\n");
        for (int t = 0; t < NUM_PUNCH; ++t)
            for (int h = 0; h < 2; ++h) {
                char label[64];
                std::snprintf(label, sizeof label, "%s %s", punchName((PunchType)t), h == 0 ? "esq" : "dir");
                std::printf("%-18s", label);
                for (auto& s : stances) std::printf("%-13s", kindName(simulate(ai, h, (PunchType)t, s, react, windup)));
                std::printf("\n");
            }
    }
    return 0;
}
