// ============================================================================
//  Scene.h - Renderizacao 3D (arena, academia, rua, lutadores, sombras)
// ============================================================================
#pragma once
#include "Common.h"

class Game;

namespace Scene {
    void init();
    void render(const Game& g);
    // Projeta um ponto 3D para coordenadas de tela (origem embaixo a esquerda)
    bool project(const Vec3& p, float& sx, float& sy);
}
