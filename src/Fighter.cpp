// ============================================================================
//  Fighter.cpp - Logica de movimento, postura e golpes de um lutador
// ============================================================================
#include "Fighter.h"

int Fighter::nextPunchId = 1;

const char* punchName(PunchType t) {
    switch (t) {
        case RETO:   return "Direto";
        case GANCHO: return "Gancho";
        case UPPER:  return "Uppercut";
        case CORPO:  return "Corpo";
        default:     return "?";
    }
}

float Hand::extension() const {
    switch (phase) {
        case HAND_EXTEND:  return easeOutCubic(t);
        case HAND_RETRACT: return 1.0f - easeInOut(t);
        default:           return 0.0f;
    }
}

// ---------------------------------------------------------------------------
void Fighter::setupStats(float f, float a, float r) {
    forca = f; agilidade = a; resistencia = r;
    maxHp = 90.0f + resistencia * 6.0f;
    maxStamina = 70.0f + resistencia * 8.0f;
}

void Fighter::resetForFight() {
    hp = maxHp; stamina = maxStamina; fury = 0; furyTimer = 0;
    knockdowns = 0; status = ST_ACTIVE; downAnim = 0;
    thrown = landed = blockedHits = dodges = counters = 0;
    damageDealt = 0;
    resetForRound();
}

void Fighter::resetForRound() {
    lean = leanTarget = duck = duckTarget = 0;
    guardMode = GUARD_NONE; guardAmt = 0; guardLow = 0;
    stunTimer = counterTimer = exposedTimer = liverTimer = hitFlash = 0;
    snap = snapVel = Vec3();
    for (int h = 0; h < 2; ++h) {
        hands[h].phase = HAND_IDLE;
        hands[h].pos = hands[h].prevPos = restGlovePos(h);
    }
    events.clear();
}

// ---------------------------------------------------------------------------
// Converte coordenadas locais (direita, cima, frente) para o mundo
Vec3 Fighter::local(float lx, float ly, float lz) const {
    return base + right() * lx + Vec3(0, ly, 0) + fwd() * lz;
}

float Fighter::lungeAmount() const {
    float m = 0;
    for (int h = 0; h < 2; ++h) m = std::max(m, hands[h].extension());
    return m;
}

Vec3 Fighter::headPos() const {
    float lunge = lungeAmount();
    Vec3 p = local(lean * 0.36f, 1.62f - duck * 0.38f, duck * 0.06f + lunge * 0.10f);
    // Caido: a cabeca vai para o chao, atras do corpo
    if (downAnim > 0.001f) p = lerp(p, local(0, 0.22f, -1.35f), easeInOut(downAnim));
    return p + snap;
}

Vec3 Fighter::torsoPos() const {
    float lunge = lungeAmount();
    Vec3 p = local(lean * 0.14f, 1.18f - duck * 0.14f, lunge * 0.06f);
    if (downAnim > 0.001f) p = lerp(p, local(0, 0.22f, -0.8f), easeInOut(downAnim));
    return p + snap * 0.3f;
}

Vec3 Fighter::shoulderPos(int h) const {
    float side = (h == 0) ? -1.0f : 1.0f;
    float lunge = lungeAmount();
    // Rotacao do tronco: o ombro do braco que golpeia vai para frente
    float twist = 0;
    if (hands[h].phase == HAND_EXTEND || hands[h].phase == HAND_RETRACT) twist = 0.09f * hands[h].extension();
    else if (hands[1 - h].extension() > 0) twist = -0.05f * hands[1 - h].extension();
    Vec3 p = local(side * 0.22f * bulk + lean * 0.22f, 1.42f - duck * 0.33f, lunge * 0.18f + twist);
    if (downAnim > 0.001f) p = lerp(p, local(side * 0.25f, 0.25f, -0.95f), easeInOut(downAnim));
    return p;
}

Vec3 Fighter::restGlovePos(int h) const {
    if (useMitts) return mittTarget[h];
    float side = (h == 0) ? -1.0f : 1.0f;
    Vec3 head = headPos() - snap * 0.5f;
    Vec3 torso = torsoPos();
    Vec3 R = right(), U(0, 1, 0), F = fwd();
    float bob = std::sin(animTime * 3.2f + side) * 0.012f;
    Vec3 idle = head + R * (side * 0.20f) + U * (-0.25f + bob) + F * 0.30f;
    Vec3 high = head + R * (side * 0.105f) + U * (-0.06f) + F * 0.27f;
    Vec3 low  = torso + R * (side * 0.14f) + U * 0.02f + F * 0.28f;
    Vec3 g = lerp(high, low, guardLow);
    Vec3 p = lerp(idle, g, guardAmt);
    if (downAnim > 0.001f) p = lerp(p, local(side * 0.5f, 0.15f, -0.7f), easeInOut(downAnim));
    return p;
}

// Esferas da guarda (luvas + antebracos). So existem com guarda levantada.
int Fighter::guardSpheres(Vec3* c, float* r) const {
    if (guardAmt < 0.6f || status != ST_ACTIVE) return 0;
    int n = 0;
    Vec3 R = right(), U(0, 1, 0), F = fwd();
    for (int h = 0; h < 2; ++h) {
        if (hands[h].phase != HAND_IDLE) continue;
        float side = (h == 0) ? -1.0f : 1.0f;
        Vec3 p = hands[h].pos;
        c[n] = p; r[n] = 0.12f; ++n;
        Vec3 fore = p + U * (-0.12f * (1 - guardLow) + 0.03f * guardLow)
                      + R * (side * (0.04f + 0.05f * guardLow)) + F * (-0.08f);
        c[n] = fore; r[n] = 0.085f; ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
float Fighter::baseDamage(PunchType t) {
    static const float d[NUM_PUNCH] = { 7.0f, 10.0f, 13.0f, 6.0f };
    return d[t];
}
float Fighter::staminaCost(PunchType t) {
    static const float c[NUM_PUNCH] = { 6.0f, 9.0f, 12.0f, 7.0f };
    return c[t];
}

bool Fighter::startPunch(int h, PunchType t, float windupOverride, bool feint) {
    if (!canAct()) return false;
    Hand& hd = hands[h];
    if (hd.busy()) return false;
    if (hands[1 - h].phase == HAND_WINDUP) return false; // um preparo por vez

    static const float wind[NUM_PUNCH] = { 0.07f, 0.10f, 0.12f, 0.08f };
    static const float ext[NUM_PUNCH]  = { 0.13f, 0.19f, 0.17f, 0.15f };
    float sf = speedFactor();
    if (furyTimer > 0) sf *= 0.8f;

    hd.type = t;
    hd.feint = feint;
    hd.dWindup = (windupOverride > 0 ? windupOverride : wind[t] * sf);
    hd.dExtend = ext[t] * sf;
    hd.dRetract = 0.17f * sf;
    hd.counter = counterTimer > 0;
    hd.fury = furyTimer > 0;
    hd.resolved = false;
    hd.id = nextPunchId++;

    float cost = feint ? 2.0f : staminaCost(t) * (hd.fury ? 0.5f : 1.0f);
    hd.tired = stamina < cost;
    if (hd.tired) hd.dWindup *= 1.4f;
    stamina = std::max(0.0f, stamina - cost);

    hd.windFrom = hd.pos;
    hd.aimSet = false;
    hd.phase = HAND_WINDUP;
    hd.t = 0;
    guardMode = GUARD_NONE;   // nao da para socar com a guarda fechada
    if (!feint) ++thrown;
    return true;
}

void Fighter::beginRetract(int h) {
    Hand& hd = hands[h];
    hd.phase = HAND_RETRACT;
    hd.t = 0;
    hd.retractFrom = hd.pos;
}

void Fighter::setGuard(GuardMode m) {
    if (m != GUARD_NONE && guardMode == GUARD_NONE) guardRaisedTime = 0;
    guardMode = m;
}

void Fighter::takeSnap(const Vec3& dir, float power) {
    snapVel += dir * power;
}

// ---------------------------------------------------------------------------
void Fighter::update(float dt, const Vec3& targetHead, const Vec3& targetBody) {
    animTime += dt;
    stunTimer = std::max(0.0f, stunTimer - dt);
    counterTimer = std::max(0.0f, counterTimer - dt);
    exposedTimer = std::max(0.0f, exposedTimer - dt);
    liverTimer = std::max(0.0f, liverTimer - dt);
    hitFlash = std::max(0.0f, hitFlash - dt);
    furyTimer = std::max(0.0f, furyTimer - dt);
    guardRaisedTime += dt;

    if (status != ST_ACTIVE) {
        guardMode = GUARD_NONE; leanTarget = 0; duckTarget = 0;
    }
    downAnim = approach(downAnim, status != ST_ACTIVE ? 1.0f : 0.0f, status != ST_ACTIVE ? 5.0f : 3.0f, dt);

    // Postura suavizada (agilidade deixa a esquiva mais rapida)
    float postureRate = 12.0f + agilidade * 1.3f;
    if (stunTimer > 0) {
        leanTarget = std::sin(animTime * 7.0f) * 0.25f; // cambaleando
        duckTarget = 0.1f;
        guardMode = GUARD_NONE;
    }
    lean = approach(lean, leanTarget, postureRate, dt);
    duck = approach(duck, duckTarget, postureRate, dt);
    guardAmt = approach(guardAmt, guardMode != GUARD_NONE ? 1.0f : 0.0f, 18.0f + agilidade, dt);
    guardLow = approach(guardLow, guardMode == GUARD_LOW ? 1.0f : 0.0f, 16.0f, dt);

    // Mola da reacao da cabeca
    snapVel += (snap * -140.0f - snapVel * 14.0f) * dt;
    snap += snapVel * dt;

    // Folego regenera quando nao esta atacando
    bool attacking = hands[0].attacking() || hands[1].attacking();
    if (!attacking && status == ST_ACTIVE) {
        float regen = 8.0f + resistencia * 1.6f;
        if (guardMode != GUARD_NONE) regen *= 0.6f;
        if (liverTimer > 0) regen *= 0.3f;
        stamina = std::min(maxStamina, stamina + regen * dt);
    }

    // Maquina de estados das maos
    for (int h = 0; h < 2; ++h) {
        Hand& hd = hands[h];
        float side = (h == 0) ? -1.0f : 1.0f;
        Vec3 rest = restGlovePos(h);
        Vec3 R = right(), U(0, 1, 0), F = fwd();
        hd.prevPos = hd.pos;

        switch (hd.phase) {
        case HAND_IDLE:
            hd.pos = approach(hd.pos, rest, 22.0f, dt);
            break;

        case HAND_WINDUP: {
            if (!hd.aimSet) { hd.aimHead = targetHead; hd.aimBody = targetBody; hd.aimSet = true; }
            hd.t += dt / hd.dWindup;
            // Recuo caracteristico de cada golpe (o "telegrafo" que a defesa le)
            Vec3 off;
            switch (hd.type) {
                case RETO:   off = F * -0.07f + U * 0.01f; break;
                case GANCHO: off = R * (side * 0.15f) + F * -0.04f; break;
                case UPPER:  off = U * -0.17f + F * -0.03f; break;
                case CORPO:  off = U * -0.13f + R * (side * 0.04f); break;
                default: break;
            }
            hd.pos = lerp(hd.windFrom, rest + off, easeInOut(hd.t));
            if (hd.t >= 1.0f) {
                if (hd.feint) {           // finta: desiste do golpe
                    beginRetract(h);
                    events.push_back({ FighterEvent::FEINT_DONE, h });
                    break;
                }
                // Lanca o golpe. A mira foi definida quando o PREPARO comecou
                // (o defensor ve o recuo do braco e tem tempo de sair da linha).
                // Lutadores com 'tracking' corrigem parte da mira.
                Vec3 aim = (hd.type == CORPO) ? hd.aimBody : hd.aimHead;
                Vec3 now = (hd.type == CORPO) ? targetBody : targetHead;
                Vec3 target = lerp(aim, now, tracking);
                // O uppercut sobe procurando o queixo: acompanha quem se abaixa
                if (hd.type == UPPER) target.y = std::min(target.y, now.y);
                hd.start = hd.pos;
                Vec3 dir = normalize(target - hd.start);
                hd.end = target + dir * 0.12f;
                Vec3 sh = shoulderPos(h);
                const float reach = 1.12f;
                if (length(hd.end - sh) > reach) hd.end = sh + normalize(hd.end - sh) * reach;
                Vec3 mid = (hd.start + hd.end) * 0.5f;
                switch (hd.type) {
                    case RETO:   hd.ctrl = mid + U * 0.02f; break;
                    case GANCHO: hd.ctrl = mid + R * (side * 0.72f) + F * -0.05f; break;
                    case UPPER:  hd.ctrl = mid + U * -0.45f + F * -0.12f; break;
                    case CORPO:  hd.ctrl = mid + U * -0.10f + R * (side * 0.12f); break;
                    default: hd.ctrl = mid; break;
                }
                hd.phase = HAND_EXTEND;
                hd.extendDone = false;
                hd.t = 0;
                hd.prevPos = hd.pos;
                events.push_back({ FighterEvent::LAUNCH, h });
            }
            break;
        }

        case HAND_EXTEND:
            hd.t += dt / hd.dExtend;
            hd.pos = bezier2(hd.start, hd.ctrl, hd.end, easeOutCubic(std::min(hd.t, 1.0f)));
            if (hd.t >= 1.0f) { hd.t = 1.0f; hd.extendDone = true; }
            break;

        case HAND_RETRACT:
            hd.t += dt / hd.dRetract;
            hd.pos = lerp(hd.retractFrom, rest, easeInOut(hd.t));
            if (hd.t >= 1.0f) { hd.phase = HAND_IDLE; hd.t = 0; }
            break;
        }
    }
}

// Chamado pelo jogo DEPOIS do teste de colisao do quadro: golpes que chegaram
// ao fim da trajetoria sem acertar nada viram "erro" (whiff).
void Fighter::finishExtends() {
    for (int h = 0; h < 2; ++h) {
        Hand& hd = hands[h];
        if (hd.phase == HAND_EXTEND && hd.extendDone) {
            if (!hd.resolved) events.push_back({ FighterEvent::WHIFF, h });
            beginRetract(h);
        }
    }
}
