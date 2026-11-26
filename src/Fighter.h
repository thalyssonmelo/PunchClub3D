// ============================================================================
//  Fighter.h - Lutador (usado tanto pelo jogador quanto pelo adversario)
//
//  Cada lutador tem: atributos estilo Punch Club (Forca, Agilidade,
//  Resistencia), vida, folego (stamina), medidor de Furia, postura
//  defensiva (inclinar, abaixar, guarda alta/baixa) e duas maos com uma
//  maquina de estados de golpe: PREPARO -> EXTENSAO -> RECOLHER.
//
//  Sistema de coordenadas: Y para cima. O jogador fica em z = 0 olhando
//  para -Z; o adversario fica em z negativo olhando para +Z. Cada lutador
//  tem um referencial local (direita, cima, frente) e todos os pontos do
//  corpo sao calculados nesse referencial.
// ============================================================================
#pragma once
#include "Common.h"
#include <vector>
#include <string>

enum PunchType { RETO = 0, GANCHO, UPPER, CORPO, NUM_PUNCH };
enum HandPhase { HAND_IDLE, HAND_WINDUP, HAND_EXTEND, HAND_RETRACT };
enum GuardMode { GUARD_NONE, GUARD_HIGH, GUARD_LOW };
enum FighterStatus { ST_ACTIVE, ST_DOWN, ST_OUT };

const char* punchName(PunchType t);

// Raios das esferas de colisao
constexpr float GLOVE_R = 0.10f;
constexpr float HEAD_R  = 0.15f;
constexpr float TORSO_R = 0.23f;

struct Hand {
    HandPhase phase = HAND_IDLE;
    PunchType type = RETO;
    float t = 0;
    float dWindup = 0.1f, dExtend = 0.15f, dRetract = 0.16f;
    Vec3 pos, prevPos, start, ctrl, end, retractFrom, windFrom;
    bool resolved = false;   // golpe ja acertou/bloqueou algo
    bool counter = false;    // lancado durante janela de contra-ataque
    bool fury = false;       // lancado em modo furia
    bool tired = false;      // lancado sem folego (mais fraco)
    bool feint = false;      // finta: cancela no fim do preparo
    bool extendDone = false; // chegou ao fim da trajetoria
    bool aimSet = false;     // alvo registrado no inicio do preparo
    Vec3 aimHead, aimBody;   // onde o alvo estava quando o golpe comecou
    int id = 0;              // identificador unico (usado pela IA)

    bool busy() const { return phase != HAND_IDLE; }
    bool attacking() const { return phase == HAND_WINDUP || phase == HAND_EXTEND; }
    float extension() const; // 0..1, quanto o braco esta esticado
};

struct FighterEvent {
    enum Type { LAUNCH, WHIFF, FEINT_DONE } type;
    int hand;
};

struct Fighter {
    std::string name;
    Vec3 base;            // posicao dos pes
    float facing = -1;    // -1: olha para -Z (jogador) ; +1: olha para +Z (adversario)
    float bulk = 1.0f;    // porte fisico (visual)
    float tracking = 0.0f; // 0..1: quanto o golpe corrige a mira durante o preparo

    // Atributos (1..10, fracionarios como no Punch Club)
    float forca = 2, agilidade = 2, resistencia = 2;

    float hp = 100, maxHp = 100;
    float stamina = 100, maxStamina = 100;
    float fury = 0;               // 0..100
    float furyTimer = 0;          // > 0: modo furia ativo

    // Postura
    float lean = 0, leanTarget = 0;   // -1 (esquerda) .. +1 (direita) no referencial local
    float duck = 0, duckTarget = 0;   // 0..1
    GuardMode guardMode = GUARD_NONE;
    float guardAmt = 0;               // 0..1 (suavizado)
    float guardLow = 0;               // 0 alta .. 1 baixa (suavizado)
    float guardRaisedTime = 99;       // segundos desde que levantou a guarda

    Hand hands[2];                    // 0 = esquerda, 1 = direita

    // Temporizadores de estado
    float stunTimer = 0;      // atordoado: nao pode agir
    float counterTimer = 0;   // janela de contra-ataque apos esquiva
    float exposedTimer = 0;   // exposto apos errar um golpe
    float liverTimer = 0;     // golpe no figado: folego regenera devagar
    float hitFlash = 0;       // efeito visual de dano
    float animTime = 0;

    // Reacao visual da cabeca ao levar um golpe
    Vec3 snap;                // deslocamento da cabeca (mundo)
    Vec3 snapVel;

    FighterStatus status = ST_ACTIVE;
    float downAnim = 0;       // 0 em pe .. 1 caido
    int knockdowns = 0;

    // Mitts (treino de manoplas): posicao forcada das maos
    bool useMitts = false;
    Vec3 mittTarget[2];

    // Estatisticas da luta
    int thrown = 0, landed = 0, blockedHits = 0, dodges = 0, counters = 0;
    float damageDealt = 0;

    std::vector<FighterEvent> events;

    static int nextPunchId;

    // ---- configuracao ----
    void setupStats(float f, float a, float r);
    void resetForFight();
    void resetForRound();

    // ---- referencial local ----
    Vec3 right() const { return Vec3(-facing, 0, 0); }
    Vec3 fwd() const { return Vec3(0, 0, facing); }
    Vec3 local(float lx, float ly, float lz) const;

    // ---- pontos do corpo (mundo) ----
    float lungeAmount() const;
    Vec3 headPos() const;
    Vec3 torsoPos() const;
    Vec3 shoulderPos(int h) const;
    Vec3 restGlovePos(int h) const;
    int guardSpheres(Vec3* c, float* r) const;  // esferas que bloqueiam

    // ---- acoes ----
    bool canAct() const { return status == ST_ACTIVE && stunTimer <= 0; }
    float speedFactor() const { return 1.25f - agilidade * 0.05f; }
    static float baseDamage(PunchType t);
    static float staminaCost(PunchType t);
    bool startPunch(int h, PunchType t, float windupOverride = -1, bool feint = false);
    void beginRetract(int h);
    void setGuard(GuardMode m);
    void takeSnap(const Vec3& dir, float power);

    void update(float dt, const Vec3& targetHead, const Vec3& targetBody);
    void finishExtends();
};
