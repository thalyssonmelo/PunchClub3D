// ============================================================================
//  Combat.h - Resolucao de golpes entre dois lutadores
//  (colisao luva x guarda/cabeca/tronco e aplicacao de dano)
// ============================================================================
#pragma once
#include "Fighter.h"
#include "Collision.h"

enum HitKind { HIT_NONE, HIT_BLOCK, HIT_HEAD, HIT_BODY };

struct HitInfo {
    HitKind kind = HIT_NONE;
    Vec3 point;
    float t = -1;
};

struct HitResult {
    HitKind kind = HIT_NONE;
    PunchType type = RETO;
    float damage = 0;
    bool counter = false;
    bool perfectBlock = false;
    bool guardBreak = false;
    bool critical = false;
    bool fury = false;
    Vec3 point;
};

namespace Combat {
    // Testa o deslocamento da luva neste quadro contra o corpo do defensor
    HitInfo testPunch(const Fighter& att, int h, const Fighter& def);
    // Aplica as consequencias de um contato
    HitResult applyHit(Fighter& att, int h, Fighter& def, const HitInfo& hi);
    // Golpe que nao acertou nada. Retorna true se foi uma esquiva do defensor.
    bool applyWhiff(Fighter& att, int h, Fighter& def);
}
