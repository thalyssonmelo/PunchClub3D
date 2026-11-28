// ============================================================================
//  Game.cpp - Logica do jogo: estados, carreira, luta, treinos
// ============================================================================
#include "Game.h"
#include "Audio.h"
#include "Collision.h"
#include "Textures.h"
#include "GLInclude.h"
#include <cstdio>
#include <cctype>
#include <fstream>

Game game;

static const char* SAVE_FILE = "punchclub3d_save.txt";

// ============================================================================
//  DADOS: adversarios (escada da carreira) e aparencias
// ============================================================================
static std::vector<OpponentDef> buildOpponents() {
    std::vector<OpponentDef> v;
    auto add = [&](const char* n, const char* nick, const char* d, float f, float a, float r, float track, int reward) -> OpponentDef& {
        OpponentDef o{};
        o.name = n; o.nick = nick; o.desc = d;
        o.forca = f; o.agilidade = a; o.resistencia = r; o.tracking = track; o.reward = reward;
        v.push_back(o);
        return v.back();
    };
    auto look = [](Appearance& L, float sr, float sg, float sb, float hr, float hg, float hb,
                   float gr, float gg, float gb, float cr, float cg, float cb, int style, bool beard, float bulk) {
        L.skin[0] = sr; L.skin[1] = sg; L.skin[2] = sb;
        L.shorts[0] = hr; L.shorts[1] = hg; L.shorts[2] = hb;
        L.gloves[0] = gr; L.gloves[1] = gg; L.gloves[2] = gb;
        L.hair[0] = cr; L.hair[1] = cg; L.hair[2] = cb;
        L.hairStyle = style; L.beard = beard; L.bulk = bulk; L.mitts = false;
    };

    {   OpponentDef& o = add("Zé Pancada", "O Lento", "Lento e previsível. Prepara muito os golpes.", 2.0f, 1.5f, 3.0f, 0.0f, 60);
        o.p.aggression = 0.40f; o.p.defense = 0.25f; o.p.counter = 0.10f; o.p.reaction = 0.32f; o.p.telegraph = 0.70f;
        o.p.combo = 0.10f; o.p.learning = 0.0f; o.p.feint = 0.0f; o.p.readSkill = 0.5f;
        float pr[4] = { 1.5f, 1.0f, 0.6f, 0.6f }; std::copy(pr, pr + 4, o.p.prefer);
        look(o.look, 0.85f, 0.64f, 0.50f, 0.45f, 0.45f, 0.50f, 0.45f, 0.25f, 0.12f, 0.2f, 0.15f, 0.1f, 0, true, 1.12f); }
    {   OpponentDef& o = add("Rita Furacão", "A Agressiva", "Ataca sem parar com combinações rápidas.", 3.0f, 4.5f, 3.0f, 0.0f, 90);
        o.p.aggression = 0.80f; o.p.defense = 0.30f; o.p.counter = 0.15f; o.p.reaction = 0.26f; o.p.telegraph = 0.55f;
        o.p.combo = 0.45f; o.p.learning = 0.1f; o.p.feint = 0.0f; o.p.readSkill = 0.55f;
        float pr[4] = { 1.4f, 1.2f, 0.6f, 0.8f }; std::copy(pr, pr + 4, o.p.prefer);
        look(o.look, 0.75f, 0.52f, 0.38f, 0.80f, 0.12f, 0.12f, 0.92f, 0.92f, 0.92f, 0.15f, 0.08f, 0.05f, 3, false, 0.9f); }
    {   OpponentDef& o = add("Muralha Souza", "O Tanque", "Guarda fechada, aguenta muito. Gosta de golpes no corpo.", 5.0f, 2.0f, 7.0f, 0.0f, 120);
        o.p.aggression = 0.35f; o.p.defense = 0.70f; o.p.counter = 0.10f; o.p.reaction = 0.20f; o.p.telegraph = 0.60f;
        o.p.combo = 0.20f; o.p.learning = 0.2f; o.p.feint = 0.0f; o.p.readSkill = 0.7f;
        float pr[4] = { 0.8f, 1.0f, 0.8f, 1.6f }; std::copy(pr, pr + 4, o.p.prefer);
        look(o.look, 0.55f, 0.38f, 0.27f, 0.10f, 0.55f, 0.20f, 0.08f, 0.08f, 0.08f, 0.08f, 0.06f, 0.05f, 1, true, 1.25f); }
    {   OpponentDef& o = add("Kenji Sato", "O Contra-Atacante", "Esquiva e pune cada erro. Faz fintas.", 4.0f, 7.0f, 5.0f, 0.1f, 160);
        o.p.aggression = 0.45f; o.p.defense = 0.65f; o.p.counter = 0.85f; o.p.reaction = 0.14f; o.p.telegraph = 0.50f;
        o.p.combo = 0.30f; o.p.learning = 0.5f; o.p.feint = 0.25f; o.p.readSkill = 0.75f;
        float pr[4] = { 1.2f, 1.0f, 1.0f, 0.7f }; std::copy(pr, pr + 4, o.p.prefer);
        look(o.look, 0.93f, 0.78f, 0.62f, 0.12f, 0.25f, 0.75f, 0.15f, 0.30f, 0.80f, 0.05f, 0.05f, 0.06f, 1, false, 0.95f); }
    {   OpponentDef& o = add("Diabo Loiro", "O Campeão", "Aprende seus padrões durante a luta e se adapta.", 7.0f, 7.0f, 7.0f, 0.2f, 300);
        o.p.aggression = 0.65f; o.p.defense = 0.65f; o.p.counter = 0.50f; o.p.reaction = 0.12f; o.p.telegraph = 0.42f;
        o.p.combo = 0.45f; o.p.learning = 1.0f; o.p.feint = 0.35f; o.p.readSkill = 0.8f;
        float pr[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; std::copy(pr, pr + 4, o.p.prefer);
        look(o.look, 0.90f, 0.72f, 0.60f, 0.85f, 0.65f, 0.10f, 0.80f, 0.05f, 0.05f, 0.95f, 0.85f, 0.40f, 2, false, 1.05f); }
    return v;
}
static std::vector<OpponentDef>& opponents() { static std::vector<OpponentDef> v = buildOpponents(); return v; }

int Game::numOpponents() { return (int)opponents().size(); }
const OpponentDef& Game::opponentDef(int i) { return opponents()[std::max(0, std::min(i, numOpponents() - 1))]; }

const Appearance& Game::playerLook() {
    static Appearance a = { { 0.85f, 0.66f, 0.52f }, { 0.1f, 0.1f, 0.3f }, { 0.75f, 0.06f, 0.06f }, { 0.1f, 0.1f, 0.1f }, 1, false, 1.0f, false };
    return a;
}
const Appearance& Game::coachLook() {
    static Appearance a = { { 0.45f, 0.30f, 0.20f }, { 0.3f, 0.3f, 0.32f }, { 0.9f, 0.75f, 0.1f }, { 0.8f, 0.1f, 0.1f }, 4, true, 1.15f, true };
    return a;
}
const Appearance& Game::oppLook() const {
    if (state == GS_TRAIN && train == TR_MITTS) return coachLook();
    return opponentDef(oppIndex).look;
}

// ---------------------------------------------------------------------------
// Manoplas: alvos (no mundo), golpe esperado e mao esperada
// ---------------------------------------------------------------------------
static const Vec3 MITT_POS[6] = {
    Vec3(-0.14f, 1.58f, -0.72f), Vec3(0.14f, 1.58f, -0.72f),
    Vec3(-0.36f, 1.52f, -0.66f), Vec3(0.36f, 1.52f, -0.66f),
    Vec3(0.0f, 1.12f, -0.70f),   Vec3(0.0f, 1.32f, -0.62f)
};
static const PunchType MITT_TYPE[6] = { RETO, RETO, GANCHO, GANCHO, CORPO, UPPER };
static const int MITT_HAND[6] = { 0, 1, 0, 1, -1, -1 };          // mao do jogador (-1 qualquer)
static const int MITT_COACH_HAND[6] = { 1, 0, 1, 0, 0, 1 };      // mao do treinador que segura
const char* Game::mittLabel(int t) {
    static const char* L[6] = { "JAB (J)", "DIRETO (L)", "GANCHO ESQ (A+J)", "GANCHO DIR (D+L)", "CORPO (S+J/L)", "UPPER (W+J/L)" };
    return (t >= 0 && t < 6) ? L[t] : "";
}

int Game::litMittHand() const { return mittTarget >= 0 ? MITT_COACH_HAND[mittTarget] : -1; }

// ============================================================================
//  SACO DE PANCADAS (pendulo)
// ============================================================================
Vec3 Bag::center() const {
    return anchor + Vec3(std::sin(az) * L, -std::cos(ax) * std::cos(az) * L, std::sin(ax) * L);
}
Vec3 Bag::axis() const { return normalize(anchor - center()); }
void Bag::update(float dt) {
    const float g = 9.8f, damp = 0.7f;
    wx += (-g / L * std::sin(ax) - damp * wx) * dt;
    wz += (-g / L * std::sin(az) - damp * wz) * dt;
    ax = clampf(ax + wx * dt, -0.6f, 0.6f);
    az = clampf(az + wz * dt, -0.6f, 0.6f);
}
void Bag::impulse(const Vec3& dir, float power) {
    wx += dir.z * power / L;
    wz += dir.x * power / L;
}

// ============================================================================
//  EFEITOS
// ============================================================================
void Game::addMessage(const std::string& s, float r, float g, float b, float life, float scale) {
    messages.push_back({ s, 0, life, r, g, b, scale });
    if (messages.size() > 4) messages.erase(messages.begin());
}
void Game::addPopup(const Vec3& p, const std::string& s, float r, float g, float b) {
    popups.push_back({ p, s, 0, 0.9f, r, g, b });
}
void Game::spawnParticles(const Vec3& p, const Vec3& dir, int n, float r, float g, float b) {
    for (int i = 0; i < n; ++i) {
        Vec3 v = dir * randf(1.0f, 2.5f) + Vec3(randf(-1, 1), randf(0, 1.5f), randf(-1, 1)) * 0.9f;
        float life = randf(0.4f, 0.9f);
        particles.push_back({ p, v, life, life, r, g, b });
    }
}

// ============================================================================
//  CICLO PRINCIPAL
// ============================================================================
void Game::init() {
    std::srand(42);
    std::ifstream f(SAVE_FILE);
    hasSave = f.good();
    setupFighters(0, true);
    changeState(GS_MENU);
}

void Game::changeState(GameState s) {
    state = s;
    stateTimer = 0;
    paused = false;
}

void Game::update(float dt) {
    dt = std::min(dt, 0.05f);
    if (!paused) { gameTime += dt; stateTimer += dt; }
    hubNoticeT -= dt;

    switch (state) {
    case GS_PREFIGHT:
        player.update(dt, opp.headPos(), opp.torsoPos());
        opp.update(dt, player.headPos(), player.torsoPos());
        if (stateTimer > 4.2f) startRound();
        break;
    case GS_FIGHT:
        if (!paused) updateFight(dt);
        break;
    case GS_ROUNDBREAK:
    case GS_RESULT:
    case GS_CHAMPION:
        player.update(dt, opp.headPos(), opp.torsoPos());
        opp.update(dt, player.headPos(), player.torsoPos());
        player.events.clear(); opp.events.clear();
        if (state == GS_ROUNDBREAK && stateTimer > 6.0f) { round++; startRound(); }
        break;
    case GS_TRAIN:
        if (!paused) updateTraining(dt);
        break;
    default:
        // Menu/hub: o adversario fica se aquecendo no ringue
        opp.update(dt, Vec3(0, 1.6f, 0), Vec3(0, 1.2f, 0));
        opp.events.clear();
        break;
    }
    if (!paused) updateEffects(dt);
    updateCamera(dt);

    crowdHype = approach(crowdHype, 0.25f, 0.4f, dt);
    bool arena = state != GS_TRAIN && state != GS_TRAIN_RESULT && state != GS_HUB;
    Audio::setCrowdLevel(arena ? 0.15f + crowdHype * 0.45f : 0.0f);
    Audio::update(dt);
}

void Game::updateEffects(float dt) {
    for (size_t i = 0; i < popups.size();) {
        popups[i].t += dt;
        popups[i].pos.y += dt * 0.35f;
        if (popups[i].t > popups[i].life) popups.erase(popups.begin() + i); else ++i;
    }
    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.v.y -= 9.8f * dt;
        p.p += p.v * dt;
        p.life -= dt;
        if (p.life <= 0 || p.p.y < 0) particles.erase(particles.begin() + i); else ++i;
    }
    for (size_t i = 0; i < messages.size();) {
        messages[i].t += dt;
        if (messages[i].t > messages[i].life) messages.erase(messages.begin() + i); else ++i;
    }
    shake = std::max(0.0f, shake - dt * 1.5f);
    redFlash = std::max(0.0f, redFlash - dt * 1.8f);
    whiteFlash = std::max(0.0f, whiteFlash - dt * 3.0f);
    flashTimer = std::max(0.0f, flashTimer - dt);
    // flashes aleatorios de cameras na plateia
    if (state == GS_FIGHT && flashTimer <= 0 && chance(dt * (0.3f + crowdHype * 2.0f))) {
        flashTimer = 0.12f;
        float a = randf(0, 2 * PI);
        flashPos = Vec3(std::cos(a) * 8.0f, randf(2.0f, 5.0f), -0.6f + std::sin(a) * 8.0f);
    }
}

// ============================================================================
//  PREPARACAO DA LUTA
// ============================================================================
void Game::setupFighters(int idx, bool quick) {
    const OpponentDef& d = opponentDef(idx);
    player = Fighter();
    player.name = "Você";
    player.base = Vec3(0, 0, 0);
    player.facing = -1;
    if (quick) player.setupStats(6, 6, 6);
    else player.setupStats(career.forca, career.agilidade, career.resistencia);

    opp = Fighter();
    opp.name = d.name;
    opp.base = Vec3(0, 0, -1.2f);
    opp.facing = +1;
    opp.bulk = d.look.bulk;
    opp.tracking = d.tracking;
    opp.setupStats(d.forca, d.agilidade, d.resistencia);
    player.resetForFight();
    opp.resetForFight();
    ai = OpponentAI();
    ai.p = d.p;
    ai.reset();
}

void Game::startFight(int idx, bool quick) {
    oppIndex = idx;
    quickFight = quick;
    setupFighters(idx, quick);
    round = 1;
    whoDown = 0; pendingTKO = -1;
    popups.clear(); particles.clear(); messages.clear();
    changeState(GS_PREFIGHT);
    Audio::play(SND_CHEER, 0.8f);
    crowdHype = 0.8f;
}

void Game::startRound() {
    roundTime = ROUND_LEN;
    player.resetForRound();
    opp.resetForRound();
    player.stamina = player.maxStamina;
    opp.stamina = opp.maxStamina;
    opp.base.x = 0;
    whoDown = 0; pendingTKO = -1;
    punchBuffer[0] = punchBuffer[1] = 0;
    ai.reset();
    changeState(GS_FIGHT);
    Audio::play(SND_BELL, 0.9f);
    messages.clear();
    addMessage("ROUND " + std::to_string(round), 1, 0.85f, 0.2f, 1.3f, 1.4f);
    messages.push_back({ "LUTE!", -1.2f, 0.8f, 1, 0.3f, 0.2f, 1.6f });
}

void Game::endRound() {
    Audio::play(SND_BELL, 0.9f);
    Audio::playDelayed(SND_BELL, 0.35f, 0.9f);
    Audio::playDelayed(SND_BELL, 0.7f, 0.9f);
    if (round < NUM_ROUNDS) {
        player.hp = std::min(player.maxHp, player.hp + (player.maxHp - player.hp) * 0.3f);
        opp.hp = std::min(opp.maxHp, opp.hp + (opp.maxHp - opp.hp) * 0.3f);
        player.resetForRound(); opp.resetForRound();
        changeState(GS_ROUNDBREAK);
    } else {
        float pP = player.damageDealt + player.landed * 1.5f + opp.knockdowns * 25.0f;
        float pO = opp.damageDealt + opp.landed * 1.5f + player.knockdowns * 25.0f;
        char buf[96];
        std::snprintf(buf, sizeof buf, "Pontos dos juízes: %d x %d", (int)pP, (int)pO);
        finishFight(pP >= pO, "Decisão dos juízes");
        resultExtra = buf + std::string("  ") + resultExtra;
    }
}

void Game::finishFight(bool won, const std::string& method) {
    playerWon = won;
    resultMethod = method;
    resultTitle = won ? "VITÓRIA!" : "DERROTA";
    resultExtra.clear();
    pendingTKO = -1;
    changeState(GS_RESULT);
    Audio::play(SND_CHEER, 1.0f);
    crowdHype = 1.0f;
    if (!quickFight) {
        const OpponentDef& d = opponentDef(oppIndex);
        if (won) {
            career.money += d.reward;
            career.fame += 10 * (oppIndex + 1);
            career.wins++;
            if (career.next == oppIndex) career.next++;
            resultExtra = "Prêmio: $" + std::to_string(d.reward);
        } else {
            career.losses++;
            career.money += 10;
            resultExtra = "Cachê de consolação: $10";
        }
        career.day++;
        career.energy = 60;
        for (bool& t : career.trained) t = false;
        saveCareer();
    }
}

// ============================================================================
//  LUTA
// ============================================================================
void Game::requestPunch(int hand) {
    PunchType t = RETO;
    if (keys['w']) t = UPPER;
    else if (keys['s']) t = CORPO;
    else if (keys['a'] || keys['d']) t = GANCHO;
    punchBuffer[hand] = 0.18f;   // buffer de entrada: aperto um pouco antes ainda conta
    bufferedType[hand] = t;
}

void Game::updatePlayerInput(float dt, bool allowPunch) {
    (void)allowPunch;
    for (int h = 0; h < 2; ++h) punchBuffer[h] = std::max(0.0f, punchBuffer[h] - dt);
    if (!player.canAct()) return;
    bool g = keys[' '];
    bool s = keys['s'];
    float prevL = player.leanTarget, prevD = player.duckTarget;
    if (g) player.setGuard(s ? GUARD_LOW : GUARD_HIGH);
    else player.setGuard(GUARD_NONE);
    player.leanTarget = (keys['d'] ? 1.0f : 0.0f) - (keys['a'] ? 1.0f : 0.0f);
    player.duckTarget = (s && !g) ? 1.0f : 0.0f;
    // Esquivar custa um pouco de folego (evita esquiva infinita)
    if ((prevL == 0 && player.leanTarget != 0) || (prevD == 0 && player.duckTarget != 0))
        player.stamina = std::max(0.0f, player.stamina - 2.0f);
}

void Game::tryBufferedPunches(float /*dt*/, const Vec3& /*tgtHead*/, const Vec3& /*tgtBody*/) {
    for (int h = 0; h < 2; ++h) {
        if (punchBuffer[h] <= 0) continue;
        if (keys[' ']) { punchBuffer[h] = 0; continue; }    // com guarda fechada nao soca
        if (player.startPunch(h, bufferedType[h])) {
            punchBuffer[h] = 0;
            if (state == GS_FIGHT) ai.observePlayerPunch(h, bufferedType[h], gameTime);
        }
    }
}

void Game::updateFight(float dt) {
    float dtW = dt;
    if (hitStop > 0) { hitStop -= dt; dtW = dt * 0.08f; }        // "congelamento" no impacto
    float dtP = dtW;
    float dtO = dtW * (player.furyTimer > 0 ? 0.55f : 1.0f);    // furia: camera lenta para o adversario

    bool active = whoDown == 0 && pendingTKO < 0;
    if (active) roundTime -= dt;

    updatePlayerInput(dt, active);
    if (active) {
        ai.update(dtO, opp, player);
        tryBufferedPunches(dt, opp.headPos(), opp.torsoPos());
    }
    player.update(dtP, opp.headPos(), opp.torsoPos());
    opp.update(dtO, player.headPos(), player.torsoPos());

    if (active) {
        resolveHits(player, opp, true);
        if (whoDown == 0) resolveHits(opp, player, false);
    }
    player.finishExtends(); opp.finishExtends();
    processEvents(player, opp, true);
    processEvents(opp, player, false);

    if (whoDown != 0) updateKnockdown(dt);
    if (pendingTKO >= 0) {
        pendingTKO -= dt;
        if (pendingTKO < 0) finishFight(whoDown == 2, "Nocaute técnico (3 quedas)");
        return;
    }
    if (whoDown == 0 && roundTime <= 0) { endRound(); return; }

    // Batimento cardiaco com pouca vida
    static float heart = 0;
    heart -= dt;
    if (player.hp < player.maxHp * 0.25f && heart <= 0 && whoDown == 0) { Audio::play(SND_HEART, 0.7f); heart = 0.9f; }
    if (player.furyTimer > 0 && heart <= 0) { Audio::play(SND_HEART, 0.5f, 1.3f); heart = 0.5f; }
}

void Game::resolveHits(Fighter& a, Fighter& d, bool aIsPlayer) {
    for (int h = 0; h < 2; ++h) {
        HitInfo hi = Combat::testPunch(a, h, d);
        if (hi.kind == HIT_NONE) continue;
        HitResult r = Combat::applyHit(a, h, d, hi);

        if (r.kind == HIT_BLOCK) {
            if (r.perfectBlock) {
                Audio::play(SND_PARRY, 0.9f);
                addMessage(aIsPlayer ? "APARADO!" : "VOCÊ APAROU!", 0.4f, 0.8f, 1.0f, 0.9f, 1.0f);
                addPopup(hi.point, "APARADO", 0.4f, 0.8f, 1.0f);
                if (!aIsPlayer) { shake = 0.05f; }
                hitStop = 0.05f;
            } else {
                Audio::play(SND_BLOCK, 0.8f, randf(0.9f, 1.1f));
                addPopup(hi.point, "BLOQ", 0.7f, 0.7f, 0.8f);
                if (!aIsPlayer) shake = std::max(shake, 0.08f);
            }
            if (r.guardBreak) addMessage("GUARDA QUEBRADA!", 1, 0.5f, 0.1f, 1.0f, 1.0f);
            if (!aIsPlayer) ai.observeMyResult(r.type, 1);
            continue;
        }

        // Acerto na cabeca ou no corpo
        Audio::play(r.kind == HIT_HEAD ? SND_PUNCH_HEAD : SND_PUNCH_BODY,
                    clampf(0.6f + r.damage / 25.0f, 0, 1), randf(0.9f, 1.1f));
        char buf[16]; std::snprintf(buf, sizeof buf, "%d", (int)std::round(r.damage));
        if (r.critical) addPopup(hi.point, std::string(buf) + "!", 1.0f, 0.85f, 0.1f);
        else addPopup(hi.point, buf, 1.0f, 1.0f, 1.0f);
        if (r.kind == HIT_HEAD) spawnParticles(hi.point, normalize(a.hands[h].end - a.hands[h].start), 10, 0.75f, 0.85f, 1.0f);
        crowdHype = std::min(1.0f, crowdHype + r.damage / 40.0f);
        if (r.damage > 12) Audio::play(SND_CHEER, 0.4f);
        if (r.critical) { hitStop = 0.08f; flashTimer = 0.12f; }
        if (r.counter) addMessage("CONTRA-ATAQUE!", 1.0f, 0.75f, 0.1f, 1.0f, 1.1f);
        if (r.kind == HIT_BODY && r.type == CORPO) addPopup(hi.point + Vec3(0, -0.15f, 0), "-FÔLEGO", 1.0f, 0.8f, 0.2f);

        if (aIsPlayer) {
            if (r.counter || d.exposedTimer > 0) ai.observePlayerCounter();
            ai.observeHitTaken(d);
        } else {
            shake = std::max(shake, 0.25f + r.damage / 40.0f);
            redFlash = std::max(redFlash, 0.45f + r.damage / 50.0f);
            ai.observeMyResult(r.type, 0);
        }
        if (d.hp <= 0) { onKnockdown(aIsPlayer ? 2 : 1); return; }
    }
}

void Game::processEvents(Fighter& a, Fighter& d, bool aIsPlayer) {
    for (const FighterEvent& ev : a.events) {
        PunchType t = a.hands[ev.hand].type;
        switch (ev.type) {
        case FighterEvent::LAUNCH:
            Audio::play(SND_WHOOSH, aIsPlayer ? 0.35f : 0.5f, t == GANCHO ? 0.85f : (t == UPPER ? 0.95f : 1.1f));
            break;
        case FighterEvent::WHIFF: {
            if (whoDown != 0) break;
            bool dodge = Combat::applyWhiff(a, ev.hand, d);
            if (aIsPlayer) {
                if (dodge) addPopup(d.headPos() + Vec3(0, 0.25f, 0), "ESQUIVOU", 0.7f, 0.7f, 0.7f);
            } else {
                if (dodge) {
                    addMessage("ESQUIVA!", 0.3f, 1.0f, 0.4f, 0.7f, 0.9f);
                    if (std::fabs(d.lean) > 0.35f || d.duck > 0.35f) addPopup(a.hands[ev.hand].pos, "CONTRA-ATAQUE AGORA!", 1, 0.8f, 0.2f);
                }
                ai.observeMyResult(t, 2);
            }
            break;
        }
        case FighterEvent::FEINT_DONE:
            if (!aIsPlayer && (std::fabs(player.lean) > 0.4f || player.duck > 0.4f || player.guardAmt > 0.6f))
                addPopup(a.headPos() + Vec3(0, 0.3f, 0), "FINTA!", 1, 0.5f, 0.2f);
            break;
        }
    }
    a.events.clear();
}

void Game::onKnockdown(int who) {
    Fighter& d = (who == 1) ? player : opp;
    Fighter& a = (who == 1) ? opp : player;
    d.hp = 0;
    d.status = ST_DOWN;
    d.knockdowns++;
    whoDown = who;
    count = 0; countTimer = 1.3f; mash = 0; mashLast = -1;
    for (int h = 0; h < 2; ++h) {
        if (a.hands[h].busy()) a.beginRetract(h);
        if (d.hands[h].busy()) d.beginRetract(h);
    }
    a.setGuard(GUARD_NONE); a.leanTarget = 0; a.duckTarget = 0;
    a.events.clear(); d.events.clear();
    Audio::play(SND_KO, 1.0f);
    Audio::play(SND_CHEER, 1.0f);
    crowdHype = 1.0f;
    hitStop = 0.18f;
    whiteFlash = 0.6f;
    addMessage("KNOCKDOWN!", 1, 0.3f, 0.2f, 1.6f, 1.6f);
    if (d.knockdowns >= 3) {
        pendingTKO = 2.5f;
        addMessage("NOCAUTE TÉCNICO!", 1, 0.8f, 0.2f, 2.5f, 1.3f);
        return;
    }
    if (who == 2) {
        float c = clampf(0.92f - 0.25f * (d.knockdowns - 1) + d.resistencia * 0.02f, 0.1f, 0.95f);
        getUpAt = chance(c) ? randi(3, 8) : 99;
    }
}

void Game::updateKnockdown(float dt) {
    if (pendingTKO >= 0) return;
    countTimer -= dt;
    if (countTimer <= 0) {
        count++;
        countTimer = 1.0f;
        Audio::play(SND_COUNT, 0.9f);
        addMessage(std::to_string(count), 1, 1, 1, 0.9f, 2.2f);
        if (count >= 10) {
            addMessage("NOCAUTE!", 1, 0.2f, 0.2f, 2.0f, 1.8f);
            finishFight(whoDown == 2, "Nocaute");
            return;
        }
    }
    auto standUp = [&](Fighter& f) {
        f.status = ST_ACTIVE;
        f.hp = f.maxHp * std::max(0.2f, 0.55f - 0.1f * f.knockdowns);
        f.stamina = f.maxStamina * 0.6f;
        f.stunTimer = 0.4f;
        whoDown = 0;
        Audio::play(SND_CHEER, 0.7f);
    };
    if (whoDown == 2 && count >= getUpAt) {
        standUp(opp);
        ai.reset();
        addMessage(std::string(opp.name) + " LEVANTOU!", 1, 0.6f, 0.3f, 1.2f, 0.9f);
    }
    if (whoDown == 1) {
        mash = std::max(0.0f, mash - dt * 0.12f);
        if (mash >= 1.0f) {
            standUp(player);
            addMessage("DE PÉ! CONTINUE!", 0.3f, 1, 0.4f, 1.2f, 1.1f);
        }
    }
}

// ============================================================================
//  TREINOS (mini-jogos)
// ============================================================================
void Game::startTraining(TrainKind k) {
    train = k;
    trainTime = 0; trainScore = 0; trainHits = 0; trainCombo = 0; trainBest = 0;
    trainStarted = false;
    popups.clear(); particles.clear(); messages.clear();
    player = Fighter();
    player.base = Vec3(0, 0, 0); player.facing = -1;
    player.setupStats(career.forca, career.agilidade, career.resistencia);
    player.resetForFight();
    punchBuffer[0] = punchBuffer[1] = 0;
    if (k == TR_BAG) bag = Bag();
    if (k == TR_MITTS) {
        opp = Fighter();
        opp.name = "Treinador";
        opp.base = Vec3(0, 0, -1.2f); opp.facing = +1; opp.bulk = 1.15f;
        opp.setupStats(4, 3, 5);
        opp.useMitts = true;
        opp.mittTarget[0] = opp.local(0.18f, 1.3f, 0.25f);
        opp.mittTarget[1] = opp.local(-0.18f, 1.3f, 0.25f);
        opp.resetForFight();
        mittTarget = -1; mittCooldown = 0.8f; mittWindow = 1.4f; mittCalls = 0; coachSwinging = false;
    }
    if (k == TR_RUN) { runPos = 0; runSpeed = 0; runBeatT = 0; runPhase = 0; runLastBeat = -1; runFeedback.clear(); }
    changeState(GS_TRAIN);
}

void Game::updateTraining(float dt) {
    if (!trainStarted && stateTimer >= 3.0f) {
        trainStarted = true;
        Audio::play(SND_BELL, 0.6f);
        addMessage("VAI!", 0.3f, 1, 0.4f, 0.8f, 1.6f);
    }
    if (trainStarted) {
        trainTime += dt;
        if (trainTime >= 20.0f) { finishTraining(); return; }
    }
    switch (train) {
        case TR_BAG: updateBag(dt); break;
        case TR_MITTS: updateMitts(dt); break;
        case TR_RUN: updateRun(dt); break;
    }
}

void Game::updateBag(float dt) {
    updatePlayerInput(dt, true);
    Vec3 c = bag.center(), ax = bag.axis();
    Vec3 tHead = c + ax * 0.25f, tBody = c - ax * 0.2f;
    tryBufferedPunches(dt, tHead, tBody);
    player.update(dt, tHead, tBody);
    bag.update(dt);

    for (int h = 0; h < 2; ++h) {
        Hand& hd = player.hands[h];
        if (hd.phase != HAND_EXTEND || hd.resolved) continue;
        for (int k = -1; k <= 1; ++k) {
            Vec3 sc = c + ax * (k * 0.38f);
            float t = Collision::sweptSphere(hd.prevPos, hd.pos, GLOVE_R, sc, 0.26f);
            if (t < 0) continue;
            Vec3 hp = lerp(hd.prevPos, hd.pos, t);
            hd.resolved = true; hd.pos = hp;
            player.beginRetract(h);
            float power = Fighter::baseDamage(hd.type) * (0.6f + player.forca * 0.08f) * (hd.tired ? 0.5f : 1.0f);
            bool timing = bag.wx > 0.25f;   // saco voltando na sua direcao = golpe no tempo
            if (timing) power *= 1.5f;
            if (trainStarted) {
                trainScore += power; trainHits++; trainCombo++;
                trainBest = std::max(trainBest, trainCombo);
            }
            bag.impulse(normalize(hd.end - hd.start), power * 0.10f);
            Audio::play(SND_BAG, clampf(0.5f + power / 20.0f, 0, 1), randf(0.9f, 1.1f));
            char buf[16]; std::snprintf(buf, sizeof buf, "%d", (int)power);
            addPopup(hp, timing ? std::string(buf) + " NO TEMPO!" : std::string(buf), 1, timing ? 0.85f : 1, timing ? 0.2f : 1);
            shake = std::max(shake, 0.04f + power * 0.004f);
            break;
        }
    }
    player.finishExtends();
    for (auto& ev : player.events) if (ev.type == FighterEvent::WHIFF) trainCombo = 0;
    player.events.clear();
}

void Game::setMittPose() {
    if (!opp.useMitts) return;
    opp.mittTarget[0] = opp.local(0.18f, 1.32f, 0.25f);
    opp.mittTarget[1] = opp.local(-0.18f, 1.32f, 0.25f);
    if (mittTarget >= 0) opp.mittTarget[MITT_COACH_HAND[mittTarget]] = MITT_POS[mittTarget];
}

void Game::updateMitts(float dt) {
    updatePlayerInput(dt, true);
    Vec3 tgt = mittTarget >= 0 ? MITT_POS[mittTarget] : opp.headPos();
    tryBufferedPunches(dt, tgt, tgt);
    player.update(dt, tgt, tgt);
    opp.update(dt, player.headPos(), player.torsoPos());

    if (!coachSwinging) {
        if (mittTarget < 0) {
            mittCooldown -= dt;
            if (mittCooldown <= 0 && trainStarted) {
                mittCalls++;
                if (mittCalls % 6 == 0) {
                    // O treinador testa sua defesa com um gancho lento
                    coachSwinging = true;
                    opp.useMitts = false;
                    opp.startPunch(randi(0, 1), GANCHO, 0.65f);
                    addMessage("DEFENDA!", 1, 0.5f, 0.2f, 0.8f, 1.1f);
                } else {
                    mittTarget = randi(0, 5);
                    mittTimer = mittWindow;
                    Audio::play(SND_UI, 0.5f);
                }
            }
        } else {
            mittTimer -= dt;
            if (mittTimer <= 0) {
                trainCombo = 0;
                addPopup(MITT_POS[mittTarget], "LENTO!", 1, 0.4f, 0.3f);
                mittTarget = -1;
                mittCooldown = 0.4f;
            }
        }
    } else {
        for (int h = 0; h < 2; ++h) {
            HitInfo hi = Combat::testPunch(opp, h, player);
            if (hi.kind == HIT_NONE) continue;
            opp.hands[h].resolved = true; opp.hands[h].pos = hi.point; opp.beginRetract(h);
            if (hi.kind == HIT_BLOCK) {
                trainScore += 5; Audio::play(SND_BLOCK, 0.7f);
                addPopup(hi.point, "BLOQUEOU +5", 0.6f, 0.8f, 1);
            } else {
                trainScore = std::max(0.0f, trainScore - 15); trainCombo = 0;
                shake = 0.2f; redFlash = 0.35f;
                Audio::play(SND_MITT, 0.9f, 0.7f);
                addMessage("AI! ESQUIVE!", 1, 0.3f, 0.3f, 0.8f, 1.0f);
            }
        }
        opp.finishExtends();
        for (auto& ev : opp.events)
            if (ev.type == FighterEvent::WHIFF) {
                trainScore += 15;
                addMessage("BOA ESQUIVA! +15", 0.3f, 1, 0.4f, 0.8f, 1.0f);
            }
        opp.events.clear();
        if (!opp.hands[0].busy() && !opp.hands[1].busy()) {
            coachSwinging = false; opp.useMitts = true; mittCooldown = 0.5f;
        }
    }
    setMittPose();

    if (mittTarget >= 0) {
        Vec3 mp = MITT_POS[mittTarget];
        for (int h = 0; h < 2; ++h) {
            Hand& hd = player.hands[h];
            if (hd.phase != HAND_EXTEND || hd.resolved) continue;
            float t = Collision::sweptSphere(hd.prevPos, hd.pos, GLOVE_R, mp, 0.13f);
            if (t < 0) continue;
            hd.resolved = true; hd.pos = lerp(hd.prevPos, hd.pos, t);
            player.beginRetract(h);
            bool correct = hd.type == MITT_TYPE[mittTarget] && (MITT_HAND[mittTarget] < 0 || MITT_HAND[mittTarget] == h);
            float pts = 10.0f + (correct ? 10.0f : 0.0f) + trainCombo * 2.0f;
            if (trainStarted) {
                trainScore += pts; trainHits++; trainCombo++;
                trainBest = std::max(trainBest, trainCombo);
            }
            Audio::play(SND_MITT, 0.9f, correct ? 1.15f : 0.95f);
            addPopup(mp, correct ? "PERFEITO +" + std::to_string((int)pts) : "+" + std::to_string((int)pts),
                     correct ? 0.3f : 1, 1, correct ? 0.4f : 1);
            mittTarget = -1;
            mittCooldown = randf(0.15f, 0.4f);
            mittWindow = std::max(0.55f, mittWindow - 0.04f);
            break;
        }
    }
    player.finishExtends();
    player.events.clear();
}

void Game::updateRun(float dt) {
    if (trainStarted) {
        const float beat = 0.42f;
        int before = (int)std::floor(runBeatT / beat);
        runBeatT += dt;
        if ((int)std::floor(runBeatT / beat) != before) Audio::play(SND_COUNT, 0.25f, 1.6f);  // metronomo
    }
    runSpeed = std::max(0.0f, runSpeed - 0.5f * dt);
    runPos += runSpeed * dt;
    runPhase += runSpeed * dt * 2.2f;
    runFeedbackT -= dt;
    trainScore = runPos;
}

// Cada batida n do metronomo pede um pe: n par = A (esquerdo), n impar = D (direito)
void Game::runStep(int foot) {
    if (!trainStarted) return;
    const float beat = 0.42f;
    int n = (int)std::floor(runBeatT / beat + 0.5f);      // batida mais proxima
    float err = std::fabs(runBeatT - n * beat);
    int expected = n % 2;
    if (n == runLastBeat) {
        runFeedback = "FORA DO RITMO"; runSpeed *= 0.75f; trainCombo = 0;
    } else if (foot != expected) {
        runFeedback = "PÉ ERRADO"; runSpeed *= 0.6f; trainCombo = 0;
    } else if (err < 0.07f) {
        runFeedback = "PERFEITO"; runSpeed += 0.9f; trainCombo++;
    } else if (err < 0.14f) {
        runFeedback = "BOM"; runSpeed += 0.5f; trainCombo++;
    } else {
        runFeedback = "FORA DO RITMO"; runSpeed *= 0.75f; trainCombo = 0;
    }
    runLastBeat = n;
    trainBest = std::max(trainBest, trainCombo);
    runSpeed = std::min(runSpeed, 9.0f);
    runFeedbackT = 0.5f;
    Audio::play(SND_STEP, 0.6f, randf(0.9f, 1.1f));
}

void Game::finishTraining() {
    float score = trainScore;
    float* stat = nullptr;
    const char* statName = "";
    float base = 0;
    switch (train) {
        case TR_BAG:   stat = &career.forca;       statName = "Força";       base = score / 900.0f; break;
        case TR_MITTS: stat = &career.agilidade;   statName = "Agilidade";   base = score / 700.0f; break;
        case TR_RUN:   stat = &career.resistencia; statName = "Resistência"; base = score / 250.0f; break;
    }
    // Retorno decrescente: quanto mais alto o atributo, mais dificil subir
    gain = clampf(base, 0.05f, 0.7f) * (1.25f - *stat * 0.08f);
    gain = std::max(0.03f, gain);
    *stat = std::min(10.0f, *stat + gain);
    career.trained[train] = true;
    char buf[160];
    if (train == TR_RUN) std::snprintf(buf, sizeof buf, "Distância: %.0f m   Melhor sequência: %d", score, trainBest);
    else std::snprintf(buf, sizeof buf, "Pontos: %.0f   Acertos: %d   Melhor combo: %d", score, trainHits, trainBest);
    trainResultText = buf;
    std::snprintf(buf, sizeof buf, "%s +%.2f  (agora %.2f)", statName, gain, *stat);
    trainResultText += std::string("\n") + buf;
    Audio::play(SND_BELL, 0.6f);
    saveCareer();
    changeState(GS_TRAIN_RESULT);
}

// ============================================================================
//  CARREIRA (hub estilo Punch Club)
// ============================================================================
static const char* HUB_ITEMS[8] = {
    "Treinar FORÇA no saco de pancadas  (-25 energia)",
    "Treinar AGILIDADE nas manoplas  (-25 energia)",
    "Treinar RESISTÊNCIA correndo  (-25 energia)",
    "Trabalhar no galpão  (+$35, -30 energia)",
    "Comer um PF  (-$12, +30 energia)",
    "Dormir  (próximo dia)",
    "LUTAR contra o próximo adversário  (precisa de 40 energia)",
    "Salvar e voltar ao menu"
};
const char* hubItem(int i) { return HUB_ITEMS[i]; }

void Game::hubAction(int i) {
    Audio::play(SND_UI, 0.6f);
    auto notice = [&](const std::string& s) { hubNotice = s; hubNoticeT = 3.0f; };
    switch (i) {
    case 0: case 1: case 2:
        if (career.energy < 25) { notice("Sem energia para treinar. Coma ou durma."); return; }
        career.energy -= 25;
        startTraining((TrainKind)i);
        return;
    case 3:
        if (career.energy < 30) { notice("Cansado demais para trabalhar."); return; }
        career.energy -= 30; career.money += 35;
        notice("Você carregou caixas o dia inteiro: +$35.");
        return;
    case 4:
        if (career.money < 12) { notice("Sem dinheiro para comer. Vá trabalhar!"); return; }
        if (career.energy >= 100) { notice("Você está de barriga cheia."); return; }
        career.money -= 12; career.energy = std::min(100, career.energy + 30);
        notice("Arroz, feijão, bife e salada: +30 energia.");
        return;
    case 5: sleep(); return;
    case 6:
        if (career.next >= numOpponents()) { notice("Você já é o campeão!"); return; }
        if (career.energy < 40) { notice("Energia insuficiente para lutar (mínimo 40)."); return; }
        startFight(career.next, false);
        return;
    case 7:
        saveCareer();
        changeState(GS_MENU);
        return;
    }
}

void Game::sleep() {
    career.day++;
    career.energy = 100;
    // Como no Punch Club: atributos nao treinados caem com o tempo
    std::string lost;
    float* stats[3] = { &career.forca, &career.agilidade, &career.resistencia };
    const char* names[3] = { "Força", "Agilidade", "Resistência" };
    for (int i = 0; i < 3; ++i) {
        if (!career.trained[i] && *stats[i] > 1.0f) {
            *stats[i] = std::max(1.0f, *stats[i] - 0.08f);
            lost += (lost.empty() ? "" : ", ") + std::string(names[i]);
        }
        career.trained[i] = false;
    }
    hubNotice = "Dia " + std::to_string(career.day) + ". " +
                (lost.empty() ? "Você treinou tudo ontem!" : "Sem treino ontem, caiu: " + lost);
    hubNoticeT = 4.0f;
    saveCareer();
}

void Game::saveCareer() {
    std::ofstream f(SAVE_FILE);
    if (!f) return;
    f << "PC3D 1\n" << career.day << ' ' << career.money << ' ' << career.energy << ' ' << career.fame << ' '
      << career.next << ' ' << career.wins << ' ' << career.losses << ' '
      << career.forca << ' ' << career.agilidade << ' ' << career.resistencia << ' '
      << career.trained[0] << ' ' << career.trained[1] << ' ' << career.trained[2] << '\n';
    hasSave = true;
}

bool Game::loadCareer() {
    std::ifstream f(SAVE_FILE);
    std::string tag; int ver;
    if (!(f >> tag >> ver) || tag != "PC3D") return false;
    Career c;
    f >> c.day >> c.money >> c.energy >> c.fame >> c.next >> c.wins >> c.losses
      >> c.forca >> c.agilidade >> c.resistencia >> c.trained[0] >> c.trained[1] >> c.trained[2];
    if (!f) return false;
    c.active = true;
    career = c;
    return true;
}

// ============================================================================
//  CAMERA
// ============================================================================
void Game::updateCamera(float dt) {
    float targetFov = 62;
    Vec3 ringC(0, 1.2f, -0.6f);
    switch (state) {
    case GS_MENU: case GS_HELP: case GS_SELECT: case GS_CHAMPION: {
        float a = gameTime * 0.15f;
        cam.eye = Vec3(std::sin(a) * 5.5f, 2.6f, -0.6f + std::cos(a) * 5.5f);
        cam.target = ringC; cam.up = Vec3(0, 1, 0);
        break;
    }
    case GS_HUB: case GS_TRAIN_RESULT: {
        float a = gameTime * 0.1f + 1.0f;
        cam.eye = Vec3(std::sin(a) * 4.0f, 2.2f, -0.6f + std::cos(a) * 4.0f);
        cam.target = Vec3(0, 1.4f, -0.8f); cam.up = Vec3(0, 1, 0);
        break;
    }
    case GS_PREFIGHT: {
        float t = clampf(stateTimer / 4.2f, 0, 1);
        float a = lerpf(1.1f, -0.38f, easeInOut(t));
        float r = lerpf(2.6f, 1.55f, easeInOut(t));
        cam.eye = opp.base + Vec3(std::sin(a) * r, 1.55f + 0.35f * (1 - t), std::cos(a) * r);
        cam.target = opp.headPos() - Vec3(0, 0.1f, 0); cam.up = Vec3(0, 1, 0);
        targetFov = 55;
        lookSmooth = opp.headPos();
        break;
    }
    case GS_ROUNDBREAK: case GS_RESULT: {
        float a = 0.6f + gameTime * 0.08f;
        cam.eye = Vec3(std::sin(a) * 2.8f, 1.9f, -0.6f + std::cos(a) * 2.8f);
        cam.target = Vec3(opp.base.x, 1.2f, -0.9f); cam.up = Vec3(0, 1, 0);
        break;
    }
    case GS_FIGHT: case GS_TRAIN: {
        if (state == GS_TRAIN && train == TR_RUN) {
            cam.eye = Vec3(std::sin(runPhase * 0.5f) * 0.04f, 1.6f + std::fabs(std::sin(runPhase)) * 0.07f, -runPos);
            cam.target = cam.eye + Vec3(0, -0.12f, -5);
            cam.up = Vec3(0, 1, 0);
            targetFov = 70 + runSpeed;
            break;
        }
        // olho um pouco atras do centro da cabeca: as luvas nao tampam a visao
        Vec3 eye = player.headPos() + Vec3(0, 0.03f, 0.08f);
        Vec3 look;
        if (state == GS_TRAIN && train == TR_BAG) look = bag.center() + Vec3(0, 0.2f, 0);
        else look = opp.headPos();
        if (opp.status != ST_ACTIVE && state == GS_FIGHT) look = opp.torsoPos();
        lookSmooth = approach(lookSmooth, look, 6.0f, dt);
        Vec3 tgt(lerpf(eye.x, lookSmooth.x, 0.55f), eye.y - 0.06f + (lookSmooth.y - eye.y) * 0.3f, lookSmooth.z);
        float roll = player.lean * 0.14f;
        Vec3 up(std::sin(roll), std::cos(roll), 0);
        // Jogador caido: a camera vai ao chao olhando para as luzes do teto
        float down = easeInOut(player.downAnim);
        if (down > 0.001f) {
            eye = lerp(eye, Vec3(0.25f, 0.32f, 0.4f), down);
            tgt = lerp(tgt, Vec3(-0.3f, 3.5f, -2.0f), down);
            up = normalize(lerp(up, Vec3(0.5f, 0.2f, -1.0f), down));
        }
        Vec3 sh(randf(-1, 1), randf(-1, 1), randf(-0.3f, 0.3f));
        cam.eye = eye + sh * (shake * 0.05f);
        cam.target = tgt + sh * (shake * 0.08f);
        cam.up = up;
        targetFov = (player.furyTimer > 0) ? 78 : 68;
        break;
    }
    }
    cam.fov = approach(cam.fov, targetFov, 5.0f, dt);
}

// ============================================================================
//  ENTRADA
// ============================================================================
static int menuCount() { return 5; }

void Game::keyDown(unsigned char k) {
    if (k >= 'A' && k <= 'Z') k = (unsigned char)std::tolower(k);
    keys[k] = true;
    bool enter = (k == 13 || k == ' ');
    if (k == 'm') { Audio::toggleMute(); return; }

    switch (state) {
    case GS_MENU:
        if (k == 'w') { menuSel = (menuSel + menuCount() - 1) % menuCount(); Audio::play(SND_UI, 0.4f); }
        if (k == 's') { menuSel = (menuSel + 1) % menuCount(); Audio::play(SND_UI, 0.4f); }
        if (enter) {
            Audio::play(SND_UI, 0.7f);
            switch (menuSel) {
            case 0: career = Career(); career.active = true; saveCareer(); hubSel = 0;
                    hubNotice = "Bem-vindo à academia! Treine, trabalhe e suba no ranking."; hubNoticeT = 5;
                    changeState(GS_HUB); break;
            case 1: if (loadCareer()) { hubSel = 0; changeState(GS_HUB); }
                    else { hubNotice = "Nenhum jogo salvo."; hubNoticeT = 2; } break;
            case 2: changeState(GS_SELECT); break;
            case 3: changeState(GS_HELP); break;
            case 4: Audio::shutdown(); std::exit(0);
            }
        }
        break;
    case GS_HELP:
        if (enter || k == 27) changeState(GS_MENU);
        break;
    case GS_SELECT:
        if (k == 'a') { selectSel = (selectSel + numOpponents() - 1) % numOpponents(); oppIndex = selectSel; Audio::play(SND_UI, 0.4f); }
        if (k == 'd') { selectSel = (selectSel + 1) % numOpponents(); oppIndex = selectSel; Audio::play(SND_UI, 0.4f); }
        if (enter) startFight(selectSel, true);
        if (k == 27) changeState(GS_MENU);
        break;
    case GS_HUB:
        if (k == 'w') { hubSel = (hubSel + 7) % 8; Audio::play(SND_UI, 0.4f); }
        if (k == 's') { hubSel = (hubSel + 1) % 8; Audio::play(SND_UI, 0.4f); }
        if (k >= '1' && k <= '8') { hubSel = k - '1'; hubAction(hubSel); }
        else if (enter) hubAction(hubSel);
        if (k == 27) { saveCareer(); changeState(GS_MENU); }
        break;
    case GS_PREFIGHT:
        if (enter) startRound();
        break;
    case GS_FIGHT:
    case GS_TRAIN:
        if (paused) {
            if (k == 27 || k == 'p') paused = false;
            if (k == 'q') {
                if (state == GS_TRAIN) changeState(GS_HUB);
                else changeState(quickFight ? GS_MENU : GS_HUB);
            }
            break;
        }
        if (k == 27 || k == 'p') { paused = true; break; }
        if (k == 'h') hints = !hints;
        if (state == GS_TRAIN && train == TR_RUN) {
            if (k == 'a') runStep(0);
            if (k == 'd') runStep(1);
            break;
        }
        if (k == 'j' || k == 'l') {
            int h = (k == 'j') ? 0 : 1;
            if (state == GS_FIGHT && whoDown == 1) {
                if (h != mashLast) {
                    mash += 0.075f * (1.0f + player.resistencia * 0.04f) / (1.0f + 0.45f * (player.knockdowns - 1));
                    mashLast = h;
                    Audio::play(SND_STEP, 0.3f, 1.4f);
                }
            } else requestPunch(h);
        }
        if (k == 'f' && state == GS_FIGHT && player.fury >= 100 && player.canAct()) {
            player.fury = 0;
            player.furyTimer = 4.0f;
            addMessage("FÚRIA!", 1, 0.2f, 0.1f, 1.2f, 1.6f);
            Audio::play(SND_HEART, 1.0f, 0.8f);
            Audio::play(SND_CHEER, 0.6f);
        }
        break;
    case GS_ROUNDBREAK:
        if (enter) { round++; startRound(); }
        break;
    case GS_RESULT:
        if (enter && stateTimer > 1.0f) {
            if (quickFight) changeState(GS_SELECT);
            else if (playerWon && oppIndex == numOpponents() - 1) changeState(GS_CHAMPION);
            else changeState(GS_HUB);
        }
        break;
    case GS_TRAIN_RESULT:
        if (enter) changeState(GS_HUB);
        break;
    case GS_CHAMPION:
        if (enter && stateTimer > 1.0f) changeState(GS_MENU);
        break;
    }
}

void Game::keyUp(unsigned char k) {
    if (k >= 'A' && k <= 'Z') k = (unsigned char)std::tolower(k);
    keys[k] = false;
}

void Game::specialDown(int k) {
    switch (k) {
    case GLUT_KEY_F1: showDebug = !showDebug; break;
    case GLUT_KEY_F2: shadows = !shadows; break;
    case GLUT_KEY_F3: fog = !fog; break;
    case GLUT_KEY_F4: smooth = !smooth; break;
    case GLUT_KEY_F5: Textures::enabled = !Textures::enabled; break;
    case GLUT_KEY_F6: lighting = !lighting; break;
    case GLUT_KEY_F7: wire = !wire; break;
    case GLUT_KEY_F8: cones = !cones; break;
    case GLUT_KEY_F9: ps1 = !ps1; break;
    // Setas navegam nos menus
    case GLUT_KEY_UP:    if (state == GS_MENU || state == GS_HUB) { keyDown('w'); keyUp('w'); } break;
    case GLUT_KEY_DOWN:  if (state == GS_MENU || state == GS_HUB) { keyDown('s'); keyUp('s'); } break;
    case GLUT_KEY_LEFT:  if (state == GS_SELECT) { keyDown('a'); keyUp('a'); } break;
    case GLUT_KEY_RIGHT: if (state == GS_SELECT) { keyDown('d'); keyUp('d'); } break;
    }
}

void Game::mouseButton(int button, bool down) {
    if (!down) return;
    if (button == GLUT_LEFT_BUTTON) { keyDown('j'); keyUp('j'); }
    if (button == GLUT_RIGHT_BUTTON) { keyDown('l'); keyUp('l'); }
}
