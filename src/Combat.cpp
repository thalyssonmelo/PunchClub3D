// ============================================================================
//  Combat.cpp
// ============================================================================
#include "Combat.h"

namespace Combat {

HitInfo testPunch(const Fighter& att, int h, const Fighter& def) {
    HitInfo best;
    const Hand& hd = att.hands[h];
    if (hd.phase != HAND_EXTEND || hd.resolved || def.status != ST_ACTIVE) return best;

    Vec3 p0 = hd.prevPos, p1 = hd.pos;
    float bestT = 2.0f;

    // 1) Guarda (luvas e antebracos do defensor)
    Vec3 gc[4]; float gr[4];
    int n = def.guardSpheres(gc, gr);
    // Guarda baixa protege apenas o corpo: a cabeca fica exposta
    if (def.guardLow > 0.5f && hd.type != CORPO) n = 0;
    for (int i = 0; i < n; ++i) {
        float t = Collision::sweptSphere(p0, p1, GLOVE_R, gc[i], gr[i]);
        if (t >= 0 && t < bestT) { bestT = t; best.kind = HIT_BLOCK; }
    }
    // 2) Cabeca
    float t = Collision::sweptSphere(p0, p1, GLOVE_R, def.headPos(), HEAD_R);
    if (t >= 0 && t < bestT) { bestT = t; best.kind = HIT_HEAD; }
    // 3) Tronco
    t = Collision::sweptSphere(p0, p1, GLOVE_R, def.torsoPos(), TORSO_R);
    if (t >= 0 && t < bestT) { bestT = t; best.kind = HIT_BODY; }

    if (best.kind != HIT_NONE) {
        best.t = bestT;
        best.point = lerp(p0, p1, bestT);
    }
    return best;
}

HitResult applyHit(Fighter& att, int h, Fighter& def, const HitInfo& hi) {
    HitResult r;
    Hand& hd = att.hands[h];
    r.kind = hi.kind;
    r.type = hd.type;
    r.point = hi.point;
    r.counter = hd.counter;
    r.fury = hd.fury;
    hd.resolved = true;
    hd.pos = hi.point;
    att.beginRetract(h);

    float base = Fighter::baseDamage(hd.type);
    float dmg = base * (0.6f + att.forca * 0.08f) * 0.85f;
    if (hd.tired) dmg *= 0.55f;
    if (hd.counter) dmg *= 1.6f;
    if (hd.fury) dmg *= 1.5f;
    if (def.exposedTimer > 0) dmg *= 1.25f;
    if (hi.kind == HIT_HEAD && hd.type == UPPER && def.duck > 0.5f) dmg *= 1.3f; // abaixou em cima do upper
    dmg *= randf(0.9f, 1.1f);
    Vec3 dir = normalize(hd.end - hd.start);

    if (hi.kind == HIT_BLOCK) {
        def.blockedHits++;
        if (def.guardRaisedTime < 0.18f) {
            // BLOQUEIO PERFEITO (aparar): guarda levantada no ultimo instante
            r.perfectBlock = true;
            att.stunTimer = std::max(att.stunTimer, 0.45f);
            def.counterTimer = 0.6f;
            def.fury = std::min(100.0f, def.fury + 10);
        } else {
            r.damage = dmg * 0.12f;
            def.hp -= r.damage;
            def.stamina -= (hd.type == UPPER) ? 16.0f : 5.0f + base * 0.5f;
            if (def.stamina <= 0) {          // guarda quebrada por cansaco
                def.stamina = 0;
                r.guardBreak = true;
                def.stunTimer = 0.8f;
                def.guardMode = GUARD_NONE;
            }
        }
        def.takeSnap(dir, 0.6f);
        return r;
    }

    if (hi.kind == HIT_BODY) dmg *= (hd.type == CORPO ? 1.0f : 0.75f);
    r.damage = dmg;
    def.hp -= dmg;
    def.hitFlash = 0.25f;
    att.landed++;
    att.damageDealt += dmg;
    att.fury = std::min(100.0f, att.fury + (hd.counter ? 14.0f : 6.0f));
    def.fury = std::min(100.0f, def.fury + 3.0f);
    if (hd.counter) att.counters++;

    // Golpe forte interrompe o preparo do defensor (levou antes de soltar o dele)
    if (dmg >= 9.0f && def.furyTimer <= 0)
        for (int k = 0; k < 2; ++k)
            if (def.hands[k].phase == HAND_WINDUP) def.beginRetract(k);

    if (hi.kind == HIT_HEAD) {
        def.takeSnap(dir + Vec3(0, 0.25f, 0), 2.5f + dmg * 0.18f);
        if (dmg >= 13.0f || (hd.counter && hd.type != RETO)) def.stunTimer = std::max(def.stunTimer, 0.4f);
        r.critical = hd.counter || dmg >= 15.0f;
    } else {
        def.takeSnap(dir, 1.0f);
        def.stamina = std::max(0.0f, def.stamina - (10.0f + att.forca));
        def.liverTimer = 3.0f;
    }
    return r;
}

bool applyWhiff(Fighter& att, int /*h*/, Fighter& def) {
    att.stamina = std::max(0.0f, att.stamina - 3.0f);
    att.exposedTimer = 0.4f;
    if (std::fabs(def.lean) > 0.35f || def.duck > 0.35f) {
        def.counterTimer = 0.7f;
        def.dodges++;
        def.fury = std::min(100.0f, def.fury + 8.0f);
        return true;
    }
    return false;
}

} // namespace Combat
