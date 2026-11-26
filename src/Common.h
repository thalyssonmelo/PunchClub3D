// ============================================================================
//  Common.h  -  Utilitarios matematicos usados em todo o projeto
//  Punch Club 3D - Trabalho Final de Computacao Grafica
// ============================================================================
#pragma once

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>

constexpr float PI = 3.14159265358979f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

// ---------------------------------------------------------------------------
// Vetor 3D simples
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() {}
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 1e-6f ? v / l : Vec3(0, 0, 0);
}

// ---------------------------------------------------------------------------
// Funcoes escalares
// ---------------------------------------------------------------------------
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline float signf(float v) { return v < 0 ? -1.0f : 1.0f; }

inline float randf(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }
inline int randi(int a, int b) { return a + std::rand() % (b - a + 1); }
inline bool chance(float p) { return randf(0, 1) < p; }

// Aproximacao exponencial independente da taxa de quadros
inline float approach(float cur, float target, float rate, float dt) {
    return cur + (target - cur) * (1.0f - std::exp(-rate * dt));
}
inline Vec3 approach(const Vec3& cur, const Vec3& target, float rate, float dt) {
    return cur + (target - cur) * (1.0f - std::exp(-rate * dt));
}

// Curvas de suavizacao (easing)
inline float easeOutCubic(float t) { t = clampf(t, 0, 1); float u = 1 - t; return 1 - u * u * u; }
inline float easeInOut(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
inline float easeOutBack(float t) {
    t = clampf(t, 0, 1);
    const float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * std::pow(t - 1, 3) + c1 * std::pow(t - 1, 2);
}

// Curva de Bezier quadratica (usada nas trajetorias dos golpes)
inline Vec3 bezier2(const Vec3& a, const Vec3& b, const Vec3& c, float t) {
    float u = 1 - t;
    return a * (u * u) + b * (2 * u * t) + c * (t * t);
}
