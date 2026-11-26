// ============================================================================
//  Collision.h - Deteccao de colisoes
//
//  Os corpos dos lutadores sao aproximados por ESFERAS (cabeca, tronco,
//  luvas e antebracos da guarda). As luvas se movem rapido, entao em vez de
//  testar so a posicao final usamos deteccao CONTINUA: o movimento da luva
//  entre dois quadros e um segmento p0->p1 e testamos o "segmento varrido"
//  (swept sphere) contra cada esfera alvo. Isso evita o "tunelamento"
//  (a luva atravessar a cabeca entre dois quadros sem colidir).
// ============================================================================
#pragma once
#include "Common.h"

namespace Collision {

// Teste estatico esfera x esfera
inline bool sphereSphere(const Vec3& a, float ra, const Vec3& b, float rb) {
    Vec3 d = a - b;
    float r = ra + rb;
    return dot(d, d) <= r * r;
}

// Esfera de raio r que se move de p0 ate p1 contra esfera parada (c, R).
// Retorna o parametro t em [0,1] do primeiro contato, ou -1 se nao colide.
// Equivale a um raio contra uma esfera de raio (r + R) - soma de Minkowski.
inline float sweptSphere(const Vec3& p0, const Vec3& p1, float r, const Vec3& c, float R) {
    Vec3 d = p1 - p0;
    Vec3 f = p0 - c;
    float rr = r + R;
    float cc = dot(f, f) - rr * rr;
    if (cc <= 0.0f) return 0.0f;           // ja comeca encostando
    float a = dot(d, d);
    if (a < 1e-10f) return -1.0f;           // nao se moveu
    float b = 2.0f * dot(f, d);
    float disc = b * b - 4.0f * a * cc;
    if (disc < 0.0f) return -1.0f;          // raio passa longe
    float t = (-b - std::sqrt(disc)) / (2.0f * a);
    return (t >= 0.0f && t <= 1.0f) ? t : -1.0f;
}

// Mantem um ponto dentro de um retangulo no plano XZ (limites do ringue)
inline void clampToBox(Vec3& p, float minX, float maxX, float minZ, float maxZ) {
    p.x = clampf(p.x, minX, maxX);
    p.z = clampf(p.z, minZ, maxZ);
}

} // namespace Collision
