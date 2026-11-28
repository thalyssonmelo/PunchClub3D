// ============================================================================
//  Textures.h - Texturas PROCEDURAIS (geradas por codigo, sem arquivos externos)
//  Lona do ringue com logotipo, saia do ringue com texto, torcida, piso de
//  madeira, couro das luvas, saco de pancadas, asfalto, grama e faixas.
//  Todas usam mipmapping (gluBuild2DMipmaps) e filtragem trilinear.
// ============================================================================
#pragma once
#include "GLInclude.h"

enum TexId {
    TEX_CANVAS, TEX_APRON, TEX_CROWD, TEX_WOOD, TEX_LEATHER,
    TEX_BAG, TEX_ROAD, TEX_GRASS, TEX_BANNER, TEX_CONCRETE, NUM_TEX
};

namespace Textures {
    void init();
    GLuint get(TexId id);
    void bind(TexId id);      // habilita GL_TEXTURE_2D e associa (respeita a chave de texturas)
    void unbind();
    void setNearest(bool on); // modo PS1: filtragem GL_NEAREST
    extern bool enabled;      // F5 liga/desliga texturas
}
