// ============================================================================
//  AI.h - Inteligencia Artificial do adversario
//
//  Combina tres tecnicas:
//   1) MAQUINA DE ESTADOS FINITOS (Neutro, Atacando, Defendendo,
//      Recuperando, Atordoado, Caido).
//   2) PERCEPCAO COM TEMPO DE REACAO: a IA so "ve" o golpe do jogador
//      depois de um atraso que depende da personalidade. Ela pode ler o
//      tipo de golpe pelo recuo do braco (com uma chance de errar a leitura).
//   3) APRENDIZADO DURANTE A LUTA:
//      - Cadeia de Markov de 1a ordem sobre os golpes do jogador: preve o
//        proximo golpe a partir do anterior e antecipa a defesa.
//      - "Bandido multi-bracos" (UCB1) para escolher qual golpe usar:
//        cada tipo de golpe tem uma taxa de sucesso observada contra ESTE
//        jogador; a IA explora e passa a usar o que funciona.
//      - Detecta o habito do jogador de contra-atacar e passa a fechar a
//        guarda depois de atacar.
// ============================================================================
#pragma once
#include "Fighter.h"
#include <deque>
#include <vector>
#include <string>

struct Personality {
    float aggression = 0.5f;  // vontade de atacar
    float defense = 0.4f;     // chance de reagir a um golpe
    float counter = 0.2f;     // preferencia por esquivar e contra-atacar
    float reaction = 0.3f;    // tempo de reacao (s)
    float telegraph = 0.55f;  // duracao do preparo dos golpes (s) - quanto maior, mais facil de ler
    float combo = 0.2f;       // chance de encadear golpes
    float learning = 0.0f;    // 0..1 o quanto usa o aprendizado
    float feint = 0.0f;       // chance de fintar
    float readSkill = 0.6f;   // chance de identificar corretamente o tipo de golpe
    float prefer[NUM_PUNCH] = { 1, 1, 1, 1 };
};

class OpponentAI {
public:
    enum Mode { NEUTRO, ATACANDO, DEFENDENDO, RECUPERANDO, ATORDOADO, CAIDO };

    Personality p;
    Mode mode = NEUTRO;
    std::string thought;          // texto de depuracao ("o que a IA pensa")

    void reset();
    void update(float dt, Fighter& me, const Fighter& player);

    // Notificacoes vindas do jogo (alimentam o aprendizado)
    void observePlayerPunch(int hand, PunchType t, float gameTime);
    void observeMyResult(PunchType t, int result);   // 0 acertou, 1 bloqueado, 2 esquivado
    void observePlayerCounter();
    void observeHitTaken(Fighter& me);               // levou um golpe: pode "fechar" a guarda

    // Consultas para o HUD de depuracao
    int predictNext(float& prob, int& samples) const;
    float successRate(int t) const { return (success[t] + 1.0f) / (tries[t] + 2.0f); }
    const char* modeName() const;
    int triesOf(int t) const { return (int)tries[t]; }
    int predHits = 0, predTotal = 0;   // acuracia da previsao de Markov

private:
    struct Action { int hand; PunchType type; float delay; bool feint; bool follow; bool counter; };
    struct Pending { int id; int hand; PunchType seenType; float timeLeft; bool predicted; };

    std::deque<Action> queue;
    std::vector<Pending> pending;
    int lastSeenId[2] = { 0, 0 };
    float thinkTimer = 0, actionTimer = 0, defenseTimer = 0, guardTimer = 0;
    float circleDir = 1, circleTimer = 0, time = 0, recoverTimer = 0;
    bool counterQueued = false;
    float pressure = 0;     // quantos golpes o jogador tem jogado recentemente
    float recentHits = 0;   // quantos golpes a IA levou recentemente

    // Aprendizado
    int markov[9][8] = {};        // [golpe anterior (8 = inicio)][proximo golpe]
    int lastPlayerSym = 8;
    float lastPlayerPunchTime = -10;
    float tries[NUM_PUNCH] = {}, success[NUM_PUNCH] = {};
    int myAttacks = 0, playerCounters = 0;
    int curPred = -1; float curPredProb = 0; int curPredSamples = 0; // previsao feita ANTES do golpe atual

    PunchType chooseType(const Fighter& me, const Fighter& player) const;
    void buildCombo(const Fighter& me, const Fighter& player);
    void react(Fighter& me, const Fighter& player, int hand, PunchType t);
};
