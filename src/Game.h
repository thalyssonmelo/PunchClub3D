// ============================================================================
//  Game.h - Estado global do jogo: menus, carreira (estilo Punch Club),
//  lutas em primeira pessoa (estilo Wii Sports Boxing) e mini-jogos de treino.
// ============================================================================
#pragma once
#include "Fighter.h"
#include "AI.h"
#include "Combat.h"
#include <string>
#include <vector>

enum GameState {
    GS_MENU, GS_HELP, GS_SELECT, GS_HUB, GS_PREFIGHT, GS_FIGHT, GS_ROUNDBREAK,
    GS_RESULT, GS_TRAIN, GS_TRAIN_RESULT, GS_CHAMPION
};
enum TrainKind { TR_BAG, TR_MITTS, TR_RUN };

// Aparencia do modelo 3D (modelagem hierarquica feita com primitivas)
struct Appearance {
    float skin[3], shorts[3], gloves[3], hair[3];
    int hairStyle;      // 0 careca, 1 curto, 2 moicano, 3 rabo de cavalo, 4 bone
    bool beard;
    float bulk;
    bool mitts;
};

struct OpponentDef {
    const char* name;
    const char* nick;
    const char* desc;
    float forca, agilidade, resistencia;
    float tracking;
    Personality p;
    Appearance look;
    int reward;
};

struct Career {
    bool active = false;
    int day = 1, money = 50, energy = 100, fame = 0, next = 0, wins = 0, losses = 0;
    float forca = 2, agilidade = 2, resistencia = 2;
    bool trained[3] = { false, false, false };
};

struct Popup   { Vec3 pos; std::string text; float t, life; float r, g, b; };
struct Particle{ Vec3 p, v; float life, maxLife; float r, g, b; };
struct Message { std::string text; float t, life; float r, g, b; float scale; };

// Saco de pancadas: pendulo esferico com amortecimento (fisica simples)
struct Bag {
    Vec3 anchor = Vec3(0, 3.1f, -1.05f);
    float L = 1.75f;
    float ax = 0, az = 0, wx = 0, wz = 0;   // angulos e velocidades angulares
    Vec3 center() const;
    Vec3 axis() const;                       // direcao do centro para o gancho
    void update(float dt);
    void impulse(const Vec3& dir, float power);
};

struct CameraState { Vec3 eye, target, up; float fov = 68; };

class Game {
public:
    GameState state = GS_MENU;
    bool paused = false;
    float gameTime = 0, stateTimer = 0;
    int winW = 1280, winH = 720;
    float fps = 60;

    // Lutadores
    Fighter player, opp;
    OpponentAI ai;
    int oppIndex = 0;
    bool quickFight = false;
    Career career;

    // Luta
    int round = 1;
    float roundTime = 0;
    static constexpr float ROUND_LEN = 90.0f;
    static constexpr int NUM_ROUNDS = 3;
    int whoDown = 0;            // 0 ninguem, 1 jogador, 2 adversario
    float countTimer = 0; int count = 0; int getUpAt = 99;
    float mash = 0; int mashLast = -1;
    bool playerWon = false;
    std::string resultTitle, resultMethod, resultExtra;
    float hitStop = 0, shake = 0, redFlash = 0, whiteFlash = 0;
    float flashTimer = 0; Vec3 flashPos;
    float crowdHype = 0.3f;
    float punchBuffer[2] = { 0, 0 }; PunchType bufferedType[2] = { RETO, RETO };
    float pendingTKO = -1;

    // Treino
    TrainKind train = TR_BAG;
    float trainTime = 0, trainScore = 0, gain = 0;
    int trainHits = 0, trainCombo = 0, trainBest = 0;
    bool trainStarted = false;
    Bag bag;
    int mittTarget = -1; float mittTimer = 0, mittWindow = 1.4f, mittCooldown = 0.5f;
    int mittCalls = 0; bool coachSwinging = false;
    float runPos = 0, runSpeed = 0, runBeatT = 0, runPhase = 0; int runLastBeat = -1;
    std::string runFeedback; float runFeedbackT = 0;
    std::string trainResultText;

    // Menus
    int menuSel = 0, hubSel = 0, selectSel = 0;
    std::string hubNotice; float hubNoticeT = 0;
    bool hasSave = false;

    // Opcoes graficas (teclas F1..F8)
    bool showDebug = false, shadows = true, fog = true, smooth = true, hints = true;
    bool wire = false, lighting = true, cones = true;
    bool ps1 = false;         // F9: cena em baixa resolucao (estilo PS1)
    int crowdDrawn = 0, crowdTotal = 0;

    // Entrada
    bool keys[256] = {};
    bool mouseL = false, mouseR = false;

    // Efeitos
    std::vector<Popup> popups;
    std::vector<Particle> particles;
    std::vector<Message> messages;
    CameraState cam;
    Vec3 lookSmooth = Vec3(0, 1.6f, -1.2f);

    // ---- ciclo ----
    void init();
    void update(float dt);
    void keyDown(unsigned char k);
    void keyUp(unsigned char k);
    void specialDown(int k);
    void mouseButton(int button, bool down);

    // ---- dados ----
    static int numOpponents();
    static const OpponentDef& opponentDef(int i);
    const Appearance& oppLook() const;
    static const Appearance& playerLook();
    static const Appearance& coachLook();
    static const char* mittLabel(int t);
    int litMittHand() const;   // mao do treinador com a manopla acesa (-1 nenhuma)

    // ---- efeitos ----
    void addMessage(const std::string& s, float r, float g, float b, float life = 1.1f, float scale = 1.0f);
    void addPopup(const Vec3& p, const std::string& s, float r, float g, float b);
    void spawnParticles(const Vec3& p, const Vec3& dir, int n, float r, float g, float b);

private:
    void changeState(GameState s);
    void setupFighters(int idx, bool quick);
    void startFight(int idx, bool quick);
    void startRound();
    void endRound();
    void finishFight(bool won, const std::string& method);
    void updateFight(float dt);
    void updatePlayerInput(float dt, bool allowPunch);
    void tryBufferedPunches(float dt, const Vec3& tgtHead, const Vec3& tgtBody);
    void processEvents(Fighter& a, Fighter& d, bool aIsPlayer);
    void resolveHits(Fighter& a, Fighter& d, bool aIsPlayer);
    void onKnockdown(int who);
    void updateKnockdown(float dt);
    void requestPunch(int hand);

    void startTraining(TrainKind k);
    void updateTraining(float dt);
    void updateBag(float dt);
    void updateMitts(float dt);
    void updateRun(float dt);
    void runStep(int foot);
    void finishTraining();
    void setMittPose();

    void hubAction(int i);
    void sleep();
    void saveCareer();
    bool loadCareer();

    void updateCamera(float dt);
    void updateEffects(float dt);
};

extern Game game;
