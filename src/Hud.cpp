// ============================================================================
//  Hud.cpp - Interface em PIXEL ART: barras, textos, menus, telas de resultado
//  Texto: fonte bitmap 5x7 propria (desenhada com quads, com acentos do
//  portugues montados por cima da letra base). Formas: caixas com borda preta
//  grossa e sombra "chapada", sem cantos arredondados nem degrades.
//  A interface usa uma tela virtual de 720 linhas de altura (a largura
//  acompanha a proporcao da janela), escalada para o tamanho real.
// ============================================================================
#include "Hud.h"
#include "Game.h"
#include "Scene.h"
#include "Audio.h"
#include "Textures.h"
#include "GLInclude.h"
#include <cstdio>

extern const char* hubItem(int i);

namespace Hud {

static float W = 1280, H = 720, T = 0;
static float winW = 1280, winH = 720;

// ---------------------------------------------------------------------------
// Paleta
// ---------------------------------------------------------------------------
struct Col { float r, g, b; };
static const Col INK   = { 0.10f, 0.06f, 0.07f };   // contorno e sombras
static const Col PAPER = { 0.96f, 0.95f, 0.92f };   // texto principal
static const Col MUTED = { 0.79f, 0.78f, 0.75f };   // texto secundario
static const Col DIM   = { 0.45f, 0.45f, 0.50f };
static const Col RED   = { 0.91f, 0.25f, 0.16f };   // corner do jogador
static const Col BLUE  = { 0.18f, 0.48f, 1.00f };   // corner do adversario
static const Col GOLD  = { 0.96f, 0.72f, 0.00f };   // furia e destaques
static const Col GREEN = { 0.40f, 0.85f, 0.40f };
static const Col PANEL = { 0.09f, 0.08f, 0.11f };
static Col mix(const Col& a, const Col& b, float t) { return { lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t) }; }

// Tamanhos da fonte (tamanho do "pixel" da fonte, em pixels da tela virtual)
static const int S1 = 2, S2 = 3, S3 = 4;

// ---------------------------------------------------------------------------
// Formas
// ---------------------------------------------------------------------------
static void rect(float x, float y, float w, float h, const Col& c, float a = 1) {
    glColor4f(c.r, c.g, c.b, a);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
// Caixa com borda grossa e sombra chapada deslocada para baixo/direita
static void box(float x, float y, float w, float h, const Col& fill, float a = 1, float border = 4, float shadow = 4) {
    if (shadow > 0) rect(x + shadow, y - shadow, w, h, INK, a);
    rect(x, y, w, h, INK, a);
    rect(x + border, y + border, w - 2 * border, h - 2 * border, fill, a);
}
// Painel escuro com faixa colorida no topo
static void panel(float x, float y, float w, float h, const Col& accent, float a = 0.93f) {
    box(x, y, w, h, PANEL, a);
    rect(x + 4, y + h - 8, w - 8, 4, accent, a);
}
// Barra com brilho em cima e sombra embaixo (estilo 16 bits)
static void bar(float x, float y, float w, float h, float frac, const Col& c, bool rtl = false, float a = 1) {
    frac = clampf(frac, 0, 1);
    box(x, y, w, h, mix(c, INK, 0.78f), a);
    float iw = w - 8, ih = h - 8;
    float fw = std::floor(iw * frac);
    if (fw < 1) return;
    float fx = rtl ? x + 4 + iw - fw : x + 4;
    rect(fx, y + 4, fw, ih, c, a);
    float band = std::max(2.0f, std::floor(ih / 4));
    rect(fx, y + 4 + ih - band, fw, band, mix(c, PAPER, 0.45f), a);
    rect(fx, y + 4, fw, band, mix(c, INK, 0.3f), a);
}
// Barra em blocos (atributos de 0 a max)
static void segBar(float x, float y, float w, float h, float v, float maxv, const Col& c) {
    const int n = 10;
    float gap = 4, bw = (w - gap * (n - 1)) / n;
    float filled = clampf(v / maxv, 0, 1) * n;
    for (int i = 0; i < n; ++i) {
        float bx = x + i * (bw + gap);
        rect(bx, y, bw, h, INK);
        float k = clampf(filled - i, 0, 1);
        rect(bx + 2, y + 2, bw - 4, h - 4, mix(c, INK, 0.75f));
        if (k > 0) {
            rect(bx + 2, y + 2, (bw - 4) * k, h - 4, c);
            rect(bx + 2, y + h - 4, (bw - 4) * k, 2, mix(c, PAPER, 0.45f));
        }
    }
}
// Vinheta (bordas coloridas) para dano / furia
static void vignette(float r, float g, float b, float a) {
    if (a <= 0.001f) return;
    float m = std::min(W, H) * 0.28f;
    glBegin(GL_QUADS);
    auto edge = [&](float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
        glColor4f(r, g, b, a); glVertex2f(x0, y0); glVertex2f(x1, y1);
        glColor4f(r, g, b, 0); glVertex2f(x2, y2); glVertex2f(x3, y3);
    };
    edge(0, 0, W, 0, W - m, m, m, m);
    edge(W, H, 0, H, m, H - m, W - m, H - m);
    edge(0, H, 0, 0, m, m, m, H - m);
    edge(W, 0, W, H, W - m, H - m, W - m, m);
    glEnd();
}

// ---------------------------------------------------------------------------
// Fonte pixel 5x7 (ASCII 32..126, colunas, bit 0 = linha de cima)
// ---------------------------------------------------------------------------
static const unsigned char FONT[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x08,0x04,0x08,0x10,0x08},
};
// Letras com perna (descem 2 linhas abaixo da base): ate 9 linhas por coluna
struct Desc { char c; unsigned short col[5]; };
static const Desc DESCENDERS[] = {
    { 'g', { 0x038, 0x144, 0x144, 0x144, 0x0FC } }, { 'j', { 0x080, 0x100, 0x104, 0x0FD, 0x000 } },
    { 'p', { 0x1FC, 0x044, 0x044, 0x044, 0x038 } }, { 'q', { 0x038, 0x044, 0x044, 0x044, 0x1FC } },
    { 'y', { 0x03C, 0x140, 0x140, 0x140, 0x0FC } },
};
static const unsigned short DOTLESS_I[5] = { 0x00, 0x44, 0x7C, 0x40, 0x00 };
// Acentos: 2 linhas de 5 colunas (1 agudo, 2 grave, 3 circunflexo, 4 til, 5 cedilha)
static const char* ACCENT[6][2] = { { "", "" }, { "00010", "00100" }, { "01000", "00100" }, { "00100", "01010" },
                                    { "01101", "10010" }, { "00100", "01000" } };

static std::vector<unsigned> decodeUtf8(const std::string& s) {
    std::vector<unsigned> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        if (c < 0x80) { out.push_back(c); i++; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) { out.push_back(((c & 0x1F) << 6) | (s[i + 1] & 0x3F)); i += 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) { out.push_back(((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F)); i += 3; }
        else { out.push_back('?'); i++; }
    }
    return out;
}
// Letra acentuada = letra base + acento
static unsigned baseLetter(unsigned cp, int& accent) {
    accent = 0;
    struct M { unsigned cp; char base; int acc; };
    static const M map[] = {
        { 0xE1, 'a', 1 }, { 0xE0, 'a', 2 }, { 0xE2, 'a', 3 }, { 0xE3, 'a', 4 }, { 0xE9, 'e', 1 }, { 0xEA, 'e', 3 },
        { 0xED, 'i', 1 }, { 0xF3, 'o', 1 }, { 0xF4, 'o', 3 }, { 0xF5, 'o', 4 }, { 0xFA, 'u', 1 }, { 0xE7, 'c', 5 },
        { 0xC1, 'A', 1 }, { 0xC0, 'A', 2 }, { 0xC2, 'A', 3 }, { 0xC3, 'A', 4 }, { 0xC9, 'E', 1 }, { 0xCA, 'E', 3 },
        { 0xCD, 'I', 1 }, { 0xD3, 'O', 1 }, { 0xD4, 'O', 3 }, { 0xD5, 'O', 4 }, { 0xDA, 'U', 1 }, { 0xC7, 'C', 5 },
    };
    for (const M& m : map) if (m.cp == cp) { accent = m.acc; return (unsigned)m.base; }
    if (cp == 0xB7) return '.';
    return (cp >= 32 && cp < 127) ? cp : '?';
}
static const unsigned short* columns(unsigned base, int accent) {
    static unsigned short table[95][5];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 95; ++i) for (int k = 0; k < 5; ++k) table[i][k] = FONT[i][k];
        for (const Desc& d : DESCENDERS) for (int k = 0; k < 5; ++k) table[d.c - 32][k] = d.col[k];
        built = true;
    }
    if (base == 'i' && accent) return DOTLESS_I;
    return table[base - 32];
}
static void colRange(const unsigned short* c, int& c0, int& c1) {
    c0 = 5; c1 = -1;
    for (int i = 0; i < 5; ++i) if (c[i]) { c0 = std::min(c0, i); c1 = std::max(c1, i); }
}
static float advance(unsigned cp, int px) {
    int acc; unsigned b = baseLetter(cp, acc);
    if (b == ' ') return 3.0f * px;
    int c0, c1; colRange(columns(b, acc), c0, c1);
    if (c1 < 0) return 3.0f * px;
    return (c1 - c0 + 2) * (float)px;
}
static float textW(const std::string& s, int px) {
    float w = 0;
    for (unsigned cp : decodeUtf8(s)) w += advance(cp, px);
    return std::max(0.0f, w - px);
}
// Desenha so os pixels (y = linha de base, embaixo da linha 6 da letra)
static void rawText(float x, float y, const std::string& s, int px) {
    glBegin(GL_QUADS);
    for (unsigned cp : decodeUtf8(s)) {
        int acc; unsigned b = baseLetter(cp, acc);
        const unsigned short* c = columns(b, acc);
        int c0, c1; colRange(c, c0, c1);
        if (b != ' ' && c1 >= 0) {
            auto quadAt = [&](int col, int row, int len) {
                float qx = x + (col - c0) * px, qy = y + (6 - row) * px;
                glVertex2f(qx, qy); glVertex2f(qx + len * px, qy); glVertex2f(qx + len * px, qy + px); glVertex2f(qx, qy + px);
            };
            for (int row = 0; row < 9; ++row)
                for (int col = 0; col < 5;) {
                    if (!(c[col] >> row & 1)) { ++col; continue; }
                    int len = 1;
                    while (col + len < 5 && (c[col + len] >> row & 1)) ++len;
                    quadAt(col, row, len);
                    col += len;
                }
            if (acc) {
                bool upper = b >= 'A' && b <= 'Z';
                int r0 = acc == 5 ? 7 : (upper ? -3 : -1);
                for (int k = 0; k < 2; ++k)
                    for (int col = 0; col < 5; ++col)
                        if (ACCENT[acc][k][col] == '1') quadAt(col, r0 + k, 1);
            }
        }
        x += advance(cp, px);
    }
    glEnd();
}
static void text(float x, float y, const std::string& s, int px, const Col& c, float a = 1, bool shadow = true) {
    x = std::floor(x); y = std::floor(y);
    if (shadow) { glColor4f(INK.r, INK.g, INK.b, a * 0.9f); rawText(x + px, y - px, s, px); }
    glColor4f(c.r, c.g, c.b, a); rawText(x, y, s, px);
}
static void textC(float x, float y, const std::string& s, int px, const Col& c, float a = 1) { text(x - textW(s, px) / 2, y, s, px, c, a); }
static void textR(float x, float y, const std::string& s, int px, const Col& c, float a = 1) { text(x - textW(s, px), y, s, px, c, a); }
// Titulo grande: contorno preto + sombra chapada
static void title(float x, float y, const std::string& s, int px, const Col& c, float a = 1, bool center = true) {
    if (center) x -= textW(s, px) / 2;
    x = std::floor(x); y = std::floor(y);
    float o = std::max(2, px / 2);
    glColor4f(INK.r, INK.g, INK.b, a);
    rawText(x + o * 2, y - o * 2, s, px);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy) rawText(x + dx * o, y + dy * o, s, px);
    glColor4f(c.r, c.g, c.b, a); rawText(x, y, s, px);
}
static void textLines(float x, float y, const std::string& s, int px, float lh, const Col& c) {
    size_t st = 0;
    while (st <= s.size()) {
        size_t e = s.find('\n', st);
        if (e == std::string::npos) e = s.size();
        text(x, y, s.substr(st, e - st), px, c);
        y -= lh;
        st = e + 1;
    }
}
// Quebra o texto em linhas que cabem na largura
static std::vector<std::string> wrap(const std::string& s, float maxW, int px) {
    std::vector<std::string> out;
    std::string line, word;
    auto push = [&]() {
        if (word.empty()) return;
        std::string t = line.empty() ? word : line + " " + word;
        if (!line.empty() && textW(t, px) > maxW) { out.push_back(line); line = word; } else line = t;
        word.clear();
    };
    for (char ch : s) { if (ch == ' ') push(); else word += ch; }
    push();
    if (!line.empty()) out.push_back(line);
    return out;
}
// Etiqueta: caixa colorida com texto escuro. Retorna a largura.
static float tag(float x, float y, const std::string& s, int px, const Col& bg, const Col& fg, float a = 1, bool alignRight = false) {
    float pad = px * 2 + 2;
    float w = textW(s, px) + pad * 2, h = 7 * px + pad * 2;
    if (alignRight) x -= w;
    rect(x + 4, y - 4, w, h, INK, a);
    rect(x, y, w, h, bg, a);
    text(x + pad, y + pad, s, px, fg, a, false);
    return w;
}
static float tagH(int px) { return 7 * px + (px * 2 + 2) * 2; }

// Maiusculas que respeitam os acentos (UTF-8)
static std::string upper(const std::string& s) {
    std::string out;
    for (unsigned cp : decodeUtf8(s)) {
        if (cp >= 'a' && cp <= 'z') cp -= 32;
        else if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) cp -= 0x20;
        if (cp < 0x80) out += (char)cp;
        else { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    }
    return out;
}

static std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }
static std::string fmt2(const char* f, double a, double b2) { char b[96]; std::snprintf(b, sizeof b, f, a, b2); return b; }

// Projeta um ponto 3D para a tela virtual do HUD
static bool proj(const Vec3& p, float& x, float& y) {
    if (!Scene::project(p, x, y)) return false;
    x *= W / winW; y *= H / winH;
    return true;
}

// ---------------------------------------------------------------------------
// Pedacos de tela
// ---------------------------------------------------------------------------
static void messages(const Game& g) {
    float y = H * 0.62f;
    for (const Message& m : g.messages) {
        if (m.t < 0) continue;
        float k = m.t / m.life;
        float a = k < 0.75f ? 1.0f : 1.0f - (k - 0.75f) / 0.25f;
        float pop = 1.0f + 0.35f * std::max(0.0f, 1.0f - m.t * 6.0f);
        int px = std::max(3, (int)std::lround(6.5f * m.scale * pop));
        title(W / 2, y, m.text, px, { m.r, m.g, m.b }, a);
        y -= 70 * m.scale;
    }
}

static void popups(const Game& g) {
    for (const Popup& p : g.popups) {
        float sx, sy;
        if (!proj(p.pos, sx, sy)) continue;
        float a = 1.0f - p.t / p.life;
        textC(sx, sy, p.text, S2, { p.r, p.g, p.b }, a);
    }
}

static void fighterBars(const Fighter& f, bool left, const std::string& name) {
    float bw = 416, x = left ? 24 : W - 24 - bw, top = H - 24;
    const Col& corner = left ? RED : BLUE;
    // nome e quedas
    float th = tagH(S2);
    std::string up = upper(name);
    if (left) tag(x, top - th, up, S2, corner, INK);
    else tag(x + bw, top - th, up, S2, corner, INK, 1, true);
    std::string kd = "QUEDAS " + std::to_string(f.knockdowns) + "/3";
    if (left) textR(x + bw, top - th + 8, kd, S1, f.knockdowns ? GOLD : MUTED);
    else text(x, top - th + 8, kd, S1, f.knockdowns ? GOLD : MUTED);
    // vida (pisca quando esta baixa)
    float hp = f.hp / f.maxHp;
    Col hc = corner;
    if (hp < 0.25f && std::sin(T * 12) > 0) hc = mix(corner, PAPER, 0.5f);
    float hy = top - th - 12 - 28;
    bar(x, hy, bw, 28, hp, hc, !left);
    // folego
    float sw = 256, sy = hy - 12 - 16;
    bar(left ? x : x + bw - sw, sy, sw, 16, f.stamina / f.maxStamina, { 0.91f, 0.89f, 0.85f }, !left);
    // estado
    std::string st;
    Col stc = GOLD;
    if (f.status != ST_ACTIVE) st = "CAÍDO";
    else if (f.stunTimer > 0) st = "ATORDOADO";
    else if (f.stamina < f.maxStamina * 0.15f) st = "SEM FÔLEGO";
    else if (f.exposedTimer > 0) st = "EXPOSTO";
    if (f.furyTimer > 0) { st = "FÚRIA " + fmt("%.1fs", f.furyTimer); stc = { 1.0f, 0.45f, 0.1f }; }
    if (!st.empty()) {
        float ty = sy - 10 - tagH(S1);
        if (left) tag(x, ty, st, S1, stc, INK); else tag(x + bw, ty, st, S1, stc, INK, 1, true);
    }
}

static const char* defenseHint(PunchType t) {
    switch (t) {
        case RETO:   return "guarda (Espaço), inclinar (A/D) ou abaixar (S)";
        case GANCHO: return "guarda (Espaço), abaixar (S) ou inclinar";
        case UPPER:  return "INCLINE (A/D)!  abaixar não adianta";
        case CORPO:  return "guarda BAIXA (Espaço+S)";
        default: return "";
    }
}

static void telegraphHints(const Game& g) {
    if (!g.hints) return;
    for (int h = 0; h < 2; ++h) {
        const Hand& hd = g.opp.hands[h];
        if (hd.phase != HAND_WINDUP) continue;
        float sx, sy;
        if (!proj(g.opp.headPos() + Vec3(0, 0.32f, 0), sx, sy)) continue;
        std::string name = hd.feint ? "???" : punchName(hd.type);
        float side = (h == 0) ? -1.0f : 1.0f;   // mao esquerda do adversario aparece a DIREITA da tela
        std::string arrow = hd.type == GANCHO ? (side < 0 ? " vem da DIREITA" : " vem da ESQUERDA") : "";
        std::string s = "! " + name + arrow + " !";
        float blink = std::sin(T * 25) > 0 ? 1.0f : 0.75f;
        float w = textW(s, S2) + 2 * (S2 * 2 + 2);
        tag(sx - w / 2, sy, s, S2, mix(GOLD, PAPER, 1 - blink), INK);
        if (!hd.feint) textC(sx, sy - 22, defenseHint(hd.type), S1, PAPER, 0.95f);
    }
}

static void crosshair(const Game& g) {
    float cx = std::floor(W / 2), cy = std::floor(H / 2);
    bool counter = g.player.counterTimer > 0;
    Col c = counter ? GOLD : PAPER;
    float a = counter ? 1.0f : 0.5f;
    rect(cx - 12, cy - 2, 8, 4, c, a); rect(cx + 4, cy - 2, 8, 4, c, a);
    rect(cx - 2, cy - 12, 4, 8, c, a); rect(cx - 2, cy + 4, 4, 8, c, a);
    if (counter) {
        float w = textW("CONTRA-ATAQUE!", S2) + 2 * (S2 * 2 + 2);
        tag(cx - w / 2, cy - 60, "CONTRA-ATAQUE!", S2, GOLD, INK);
        bar(cx - 70, cy - 82, 140, 14, g.player.counterTimer / 0.7f, GOLD);
    }
}

static void controlsFooter() {
    rect(0, 0, W, 44, INK, 0.75f);
    textC(W / 2, 26, "J/clique esq.: soco esquerdo   L/clique dir.: soco direito   +W uppercut   +S corpo   +A/D gancho", S1, MUTED);
    textC(W / 2, 8, "Espaço: guarda (+S baixa)   A/D: inclinar   S: abaixar   F: fúria   P: pausa   H: dicas", S1, MUTED);
}

static void fightHud(const Game& g) {
    // vinhetas
    float lowHp = g.player.hp < g.player.maxHp * 0.3f ? (0.25f + 0.15f * std::sin(T * 6)) : 0;
    vignette(0.8f, 0, 0, std::max(g.redFlash * 0.8f, lowHp));
    if (g.player.furyTimer > 0) vignette(1.0f, 0.3f, 0.0f, 0.35f + 0.1f * std::sin(T * 12));
    if (g.whiteFlash > 0) rect(0, 0, W, H, PAPER, g.whiteFlash * 0.6f);

    fighterBars(g.player, true, "Você");
    fighterBars(g.opp, false, g.opp.name);

    // Round e relogio
    int t = (int)std::ceil(std::max(0.0f, g.roundTime));
    char clock[32]; std::snprintf(clock, sizeof clock, "%d:%02d", t / 60, t % 60);
    float cw = 136, ch = 84, cx = std::floor(W / 2 - cw / 2), cy = H - 16 - ch;
    rect(cx + 4, cy - 4, cw, ch, INK);
    rect(cx, cy, cw, ch, PAPER);
    rect(cx + 4, cy + 4, cw - 8, ch - 8, INK);
    textC(W / 2, cy + ch - 26, "ROUND " + std::to_string(g.round) + "/" + std::to_string(Game::NUM_ROUNDS), S1, GOLD);
    textC(W / 2, cy + 14, clock, 5, t <= 10 ? RED : PAPER);

    // Barra de furia
    float fw = 320, fx = std::floor(W / 2 - fw / 2), fy = g.hints ? 56 : 24;
    bool full = g.player.fury >= 100;
    text(fx, fy + 30, "FÚRIA", S1, GOLD);
    if (full) {
        bool on = std::sin(T * 10) > 0;
        textR(fx + fw, fy + 30, "PRONTA! APERTE [F]", S1, on ? PAPER : GOLD);
    } else textR(fx + fw, fy + 30, std::to_string((int)g.player.fury) + "%", S1, MUTED);
    bar(fx, fy, fw, 22, g.player.fury / 100.0f, full ? mix(GOLD, PAPER, std::sin(T * 10) > 0 ? 0.35f : 0.0f) : GOLD);

    crosshair(g);
    telegraphHints(g);

    // Contagem de knockdown
    if (g.whoDown == 1) {
        rect(0, 0, W, H, INK, 0.3f);
        title(W / 2, H * 0.3f + 10, "LEVANTE! ALTERNE J E L!", 4, GOLD);
        bar(W / 2 - 200, H * 0.3f - 40, 400, 28, g.mash, GREEN);
    }
    if (g.whoDown != 0 && g.pendingTKO < 0)
        textC(W / 2, H * 0.22f - 10, std::string("CONTAGEM: ") + std::to_string(g.count), S2, PAPER);
    if (g.hints) controlsFooter();
}

// ---------------------------------------------------------------------------
static void debugPanel(const Game& g) {
    float w = 600, h = 360, x = W - 24 - w, y = H - 150 - h;
    panel(x, y, w, h, GOLD, 0.88f);
    float ly = y + h - 34;
    auto line = [&](const std::string& s, const Col& c = PAPER) { text(x + 16, ly, s, S1, c); ly -= 19; };
    line("DEPURAÇÃO (F1)", GOLD);
    line(fmt("FPS: %.0f", g.fps));
    line(std::string("Sombras F2: ") + (g.shadows ? "on" : "off") + "   Neblina F3: " + (g.fog ? "on" : "off") +
         "   Smooth F4: " + (g.smooth ? "on" : "off"));
    line(std::string("Texturas F5: ") + (Textures::enabled ? "on" : "off") + "   Luz F6: " + (g.lighting ? "on" : "off") +
         "   Arame F7: " + (g.wire ? "on" : "off"));
    line(std::string("Cones de luz F8: ") + (g.cones ? "on" : "off") + "   PS1 F9: " + (g.ps1 ? "on" : "off") + "   Som (M): " + (Audio::isMuted() ? "mudo" : (Audio::available() ? "on" : "indisp.")));
    line("Arquibancada visível (frustum): " + std::to_string(g.crowdDrawn) + "/" + std::to_string(g.crowdTotal));
    if (g.state == GS_FIGHT) {
        ly -= 6;
        line("IA - estado: " + std::string(g.ai.modeName()), GOLD);
        line("pensa: " + g.ai.thought);
        float p; int n;
        int pred = g.ai.predictNext(p, n);
        line(pred >= 0 ? std::string("Markov: próximo golpe = ") + punchName((PunchType)pred) + fmt(" (p=%.2f)", p) + "  n=" + std::to_string(n)
                       : std::string("Markov: sem dados"));
        line(g.ai.predTotal > 0 ? fmt2("Acurácia da previsão: %.0f%% de %.0f", 100.0 * g.ai.predHits / g.ai.predTotal, (double)g.ai.predTotal)
                                : std::string("Acurácia da previsão: -"));
        line("Taxa de sucesso dos golpes da IA (UCB1):");
        for (int t = 0; t < NUM_PUNCH; ++t)
            line(std::string("   ") + punchName((PunchType)t) + fmt(": %.2f", g.ai.successRate(t)) + "  (" + std::to_string(g.ai.triesOf(t)) + " tentativas)");
        line(fmt2("Aprendizado: %.1f  Reação: %.2fs", g.ai.p.learning, g.ai.p.reaction));
    }
}

// ---------------------------------------------------------------------------
// Telas
// ---------------------------------------------------------------------------
static void footerHint(const std::string& s) {
    rect(0, 0, W, 34, INK, 0.7f);
    textC(W / 2, 11, s, S1, MUTED);
}

static void menuScreen(const Game& g) {
    rect(0, 0, 640, H, INK, 0.6f);
    float x = 64;
    text(x, H - 72, "TRABALHO FINAL - COMPUTAÇÃO GRÁFICA", S1, MUTED);
    title(x, H - 170, "PUNCH", 12, PAPER, 1, false);
    title(x, H - 270, "CLUB", 12, PAPER, 1, false);
    float cx = x + textW("CLUB", 12) + 28;
    rect(cx + 6, H - 282 - 6, textW("3D", 9) + 36, 9 * 7 + 28, INK);
    rect(cx, H - 282, textW("3D", 9) + 36, 9 * 7 + 28, RED);
    text(cx + 18, H - 268, "3D", 9, INK, 1, false);
    text(x, H - 318, "Boxe em primeira pessoa. Treine e suba até o cinturão.", S1, MUTED);

    const char* items[5] = { "NOVA CARREIRA", "CONTINUAR CARREIRA", "LUTA RÁPIDA", "COMO JOGAR", "SAIR" };
    float y = H - 400;
    for (int i = 0; i < 5; ++i) {
        bool sel = g.menuSel == i;
        bool dis = (i == 1 && !g.hasSave);
        char num[8]; std::snprintf(num, sizeof num, "%02d", i + 1);
        if (sel) {
            rect(x - 16 + 6, y - 12 - 6, 470, 46, INK);
            rect(x - 16, y - 12, 470, 46, RED);
            text(x, y, num, S1, mix(RED, INK, 0.6f), 1, false);
            text(x + 44, y, items[i], S2, INK, 1, false);
            text(x + 470 - 52, y, ">", S2, INK, 1, false);
        } else {
            text(x, y, num, S1, DIM);
            text(x + 44, y, items[i], S2, dis ? DIM : PAPER);
        }
        y -= 54;
    }
    if (g.hubNoticeT > 0) text(x, y - 4, g.hubNotice, S1, GOLD);
    footerHint("W/S ou setas: navegar   Enter: escolher   M: som   F1: depuração   F2-F9: recursos gráficos");
}

static void helpScreen() {
    rect(0, 0, W, H, INK, 0.55f);
    panel(24, 48, W - 48, H - 72, GOLD);
    title(W / 2, H - 82, "COMO JOGAR", 5, GOLD);
    std::string left =
        "ATAQUE (o golpe depende da sua postura):\n"
        "  J / clique esquerdo .... soco com a mão esquerda\n"
        "  L / clique direito ..... soco com a mão direita\n"
        "  parado + soco .......... DIRETO (rápido, barato)\n"
        "  segurando A ou D ....... GANCHO (curva por fora)\n"
        "  segurando W ............ UPPERCUT (forte, caro)\n"
        "  segurando S ............ CORPO (tira fôlego)\n"
        "\n"
        "DEFESA:\n"
        "  Espaço ......... guarda alta (protege a cabeça)\n"
        "  Espaço + S ..... guarda baixa (protege o corpo)\n"
        "  A / D .......... inclinar (esquiva lateral)\n"
        "  S .............. abaixar (esquiva diretos/ganchos)\n"
        "  Não é possível socar com a guarda fechada!\n"
        "\n"
        "LEIA O ADVERSÁRIO: a luva brilha no preparo:\n"
        "  branco = direto   amarelo = gancho\n"
        "  vermelho = uppercut   azul = corpo\n"
        "  O golpe vai onde você ESTAVA quando ele\n"
        "  começou a preparar: saia da linha a tempo!";
    std::string right =
        "MECÂNICAS ESPECIAIS:\n"
        "  CONTRA-ATAQUE: esquive e acerte logo = dano x1.6\n"
        "  APARAR: guarda no último instante = o atacante\n"
        "     fica atordoado e abre o contra-ataque\n"
        "  Golpe errado deixa quem errou EXPOSTO (x1.25)\n"
        "  Guarda gasta fôlego; sem fôlego ela QUEBRA\n"
        "  Sem fôlego seus socos ficam lentos e fracos\n"
        "  FÚRIA (F): enche acertando e esquivando.\n"
        "     Câmera lenta no adversário e golpes x1.5\n"
        "  KNOCKDOWN: alterne J e L para levantar\n"
        "  3 quedas = nocaute técnico. 3 rounds de 90s\n"
        "\n"
        "CARREIRA (estilo Punch Club):\n"
        "  Treine Força (saco), Agilidade (manoplas) e\n"
        "  Resistência (corrida). Trabalhe, coma e durma.\n"
        "  Atributos não treinados caem com o tempo!\n"
        "  Vença os 5 lutadores.\n"
        "\n"
        "IA: cada adversário tem personalidade. O campeão\n"
        "APRENDE seus padrões (Markov) e escolhe os\n"
        "golpes que funcionam (UCB1).";
    textLines(56, H - 138, left, S1, 22, PAPER);
    textLines(W / 2 + 16, H - 138, right, S1, 22, PAPER);
    textC(W / 2, 72, "Enter ou ESC para voltar", S2, GOLD);
}

static void statRow(float x, float y, float w, const std::string& label, float v, float maxv, const Col& c) {
    text(x, y, label, S1, MUTED);
    textR(x + w, y, fmt("%.1f", v), S1, PAPER);
    segBar(x + 150, y - 3, w - 150 - 56, 18, v, maxv, c);
}

static void opponentCard(const OpponentDef& d, float x, float y, float w, float h, const std::string& header) {
    panel(x, y, w, h, BLUE);
    float tx = x + 24, ty = y + h - 40;
    text(tx, ty, header, S1, { 0.56f, 0.71f, 1.0f });
    title(tx, ty - 50, d.name, 5, PAPER, 1, false);
    text(tx, ty - 76, std::string("\"") + d.nick + "\"", S1, GOLD);
    bool wide = w >= 700;
    float sx = wide ? x + w * 0.5f : tx, sw = wide ? w * 0.5f - 24 : w - 48;
    float sy = wide ? ty - 6 : ty - 140;
    std::vector<std::string> desc = wrap(d.desc, wide ? w * 0.5f - 60 : w - 48, S1);
    float dy = ty - 104;
    for (const std::string& l : desc) { text(tx, dy, l, S1, MUTED); dy -= 20; }
    statRow(sx, sy, sw, "Força", d.forca, 10, RED);
    statRow(sx, sy - 30, sw, "Agilidade", d.agilidade, 10, { 0.3f, 0.75f, 1.0f });
    statRow(sx, sy - 60, sw, "Resistência", d.resistencia, 10, GREEN);
    text(sx, sy - 96, "Prêmio: $" + std::to_string(d.reward), S1, GREEN);
    text(sx + 180, sy - 96, fmt("Aprendizado da IA: %.0f%%", d.p.learning * 100), S1, PAPER);
}

static void selectScreen(const Game& g) {
    rect(0, 0, W, H, INK, 0.35f);
    title(W / 2, H - 110, "LUTA RÁPIDA", 6, GOLD);
    const OpponentDef& d = Game::opponentDef(g.selectSel);
    opponentCard(d, W / 2 - 320, 170, 640, 300, "ADVERSÁRIO " + std::to_string(g.selectSel + 1) + "/" + std::to_string(Game::numOpponents()));
    textC(W / 2, 100, "<  A / D  >   escolher      Enter: lutar      ESC: voltar", S2, PAPER);
    footerHint("Na luta rápida você tem Força 6, Agilidade 6 e Resistência 6");
}

static void hubScreen(const Game& g) {
    const Career& c = g.career;
    // Cabecalho
    title(24, H - 72, "ACADEMIA", 6, PAPER, 1, false);
    float chx = 24 + textW("ACADEMIA", 6) + 40, chy = H - 70;
    chx += tag(chx, chy, "DIA " + std::to_string(c.day), S1, PANEL, PAPER) + 12;
    chx += tag(chx, chy, "$" + std::to_string(c.money), S1, PANEL, GREEN) + 12;
    chx += tag(chx, chy, "FAMA " + std::to_string(c.fame), S1, PANEL, PAPER) + 12;
    tag(chx, chy, "CARTEL " + std::to_string(c.wins) + "V " + std::to_string(c.losses) + "D", S1, PANEL, PAPER);
    float ex = W - 24 - 300;
    text(ex, H - 46, "ENERGIA", S1, MUTED);
    textR(W - 24, H - 46, std::to_string(c.energy) + " / 100", S1, PAPER);
    bar(ex, H - 82, 300, 24, c.energy / 100.0f, PAPER);

    // Seu lutador
    float px = 24, pw = 400, ptop = H - 104, ph = 424, py = ptop - ph;
    panel(px, py, pw, ph, RED);
    text(px + 24, ptop - 40, "SEU LUTADOR", S2, { 1.0f, 0.54f, 0.45f });
    const char* names[3] = { "FORÇA", "AGILIDADE", "RESISTÊNCIA" };
    const char* what[3] = { "dano", "velocidade", "vida e fôlego" };
    float vals[3] = { c.forca, c.agilidade, c.resistencia };
    for (int i = 0; i < 3; ++i) {
        float y = ptop - 92 - i * 100;
        text(px + 24, y, names[i], S2, PAPER);
        textR(px + pw - 24, y, fmt("%.2f", vals[i]), S3, PAPER);
        segBar(px + 24, y - 30, pw - 48, 18, vals[i], 10, RED);
        textR(px + pw - 24, y - 56, what[i], S1, DIM);
        if (c.trained[i]) text(px + 24, y - 56, "Treinado hoje", S1, GREEN);
        else text(px + 24, y - 56, "Cai ao dormir", S1, GOLD);
    }
    text(px + 24, py + 40, "O ganho de cada treino diminui", S1, DIM);
    text(px + 24, py + 20, "conforme o atributo sobe.", S1, DIM);

    // Acoes
    float ax = 448, aw = W - 24 - ax, atop = H - 104, ah = 8 * 34 + 64, ay = atop - ah;
    panel(ax, ay, aw, ah, PAPER);
    text(ax + 24, atop - 40, "O QUE FAZER HOJE?", S2, PAPER);
    for (int i = 0; i < 8; ++i) {
        float y = atop - 84 - i * 34;
        bool sel = g.hubSel == i;
        if (sel) { rect(ax + 16 + 4, y - 10 - 4, aw - 32, 32, INK); rect(ax + 16, y - 10, aw - 32, 32, RED); }
        text(ax + 28, y, std::to_string(i + 1), S1, sel ? INK : DIM, 1, !sel);
        text(ax + 56, y, hubItem(i), S1, sel ? INK : (i == 6 ? GOLD : PAPER), 1, !sel);
    }

    // Proximo adversario
    if (c.next < Game::numOpponents())
        opponentCard(Game::opponentDef(c.next), ax, 48, aw, ay - 48 - 20,
                     "PRÓXIMO ADVERSÁRIO (" + std::to_string(c.next + 1) + "/" + std::to_string(Game::numOpponents()) + ")");
    else {
        panel(ax, 48, aw, ay - 68, GOLD);
        title(ax + aw / 2, 48 + (ay - 68) / 2 - 10, "VOCÊ É O CAMPEÃO!", 5, GOLD);
    }
    // Aviso
    if (g.hubNoticeT > 0) {
        float a = clampf(g.hubNoticeT, 0, 1);
        std::vector<std::string> lines = wrap(g.hubNotice, pw - 48, S1);
        float nh = 24 + 20 * (float)lines.size();
        box(px, 48, pw, nh, mix(PANEL, GOLD, 0.15f), 0.95f * a);
        rect(px + 4, 52, 6, nh - 8, GOLD, a);
        float ly = 48 + nh - 28;
        for (const std::string& l : lines) { text(px + 24, ly, l, S1, PAPER, a); ly -= 20; }
    }
    footerHint("W/S ou setas: escolher    Enter ou 1-8: confirmar    ESC: salvar e sair");
}

static void prefightScreen(const Game& g) {
    const OpponentDef& d = Game::opponentDef(g.oppIndex);
    float a = clampf(g.stateTimer * 2, 0, 1);
    rect(0, H - 140, W, 140, INK, 0.75f * a);
    rect(0, H - 144, W, 4, GOLD, a);
    rect(0, 0, W, 110, INK, 0.75f * a);
    rect(0, 110, W, 4, GOLD, a);
    title(W * 0.25f, H - 92, "VOCÊ", 7, RED, a);
    title(W / 2, H - 96, "VS", 8, GOLD, a);
    title(W * 0.75f, H - 92, d.name, 7, BLUE, a);
    const Fighter& p = g.player;
    textC(W * 0.25f, H - 124, fmt2("Força %.1f   Agilidade %.1f", p.forca, p.agilidade) + fmt("   Resistência %.1f", p.resistencia), S1, MUTED, a);
    textC(W * 0.75f, H - 124, std::string("\"") + d.nick + "\"", S1, GOLD, a);
    textC(W / 2, 64, std::string(g.quickFight ? "Luta rápida" : "Carreira - luta " + std::to_string(g.oppIndex + 1)) + "   |   3 rounds de 90 segundos", S2, PAPER, a);
    textC(W / 2, 30, "Enter para pular", S1, MUTED, a);
}

// Tabela com barras espelhadas (voce x adversario)
static void statsTable(const Game& g, float x, float y, float w) {
    const Fighter& p = g.player; const Fighter& o = g.opp;
    std::string on = upper(o.name);
    rect(x, y + 2, 12, 12, RED); text(x + 20, y, "VOCÊ", S2, PAPER);
    textC(x + w / 2, y + 2, "ESTATÍSTICAS", S1, MUTED);
    rect(x + w - 12, y + 2, 12, 12, BLUE); textR(x + w - 20, y, on, S2, PAPER);
    auto row = [&](const std::string& lbl, float a, float b, const std::string& sa, const std::string& sb, float yy) {
        textC(x + w / 2, yy, lbl, S1, MUTED);
        text(x, yy, sa, S2, a > b ? PAPER : MUTED);
        textR(x + w, yy, sb, S2, b > a ? PAPER : MUTED);
        float m = std::max(std::max(a, b), 1.0f), half = w / 2 - 2;
        rect(x, yy - 12, half, 6, INK, 0.8f);
        rect(x + w - half, yy - 12, half, 6, INK, 0.8f);
        rect(x + half - half * a / m, yy - 12, half * a / m, 6, RED);
        rect(x + w - half, yy - 12, half * b / m, 6, BLUE);
    };
    auto pct = [](const Fighter& f) { return f.thrown ? 100.0f * f.landed / f.thrown : 0.0f; };
    auto pctS = [&](const Fighter& f) { return f.thrown ? std::to_string(100 * f.landed / f.thrown) + "%" : std::string("-"); };
    float yy = y - 40, dy = 36;
    row("Golpes lançados", (float)p.thrown, (float)o.thrown, std::to_string(p.thrown), std::to_string(o.thrown), yy); yy -= dy;
    row("Golpes conectados", (float)p.landed, (float)o.landed, std::to_string(p.landed), std::to_string(o.landed), yy); yy -= dy;
    row("Precisão", pct(p), pct(o), pctS(p), pctS(o), yy); yy -= dy;
    row("Dano causado", p.damageDealt, o.damageDealt, std::to_string((int)p.damageDealt), std::to_string((int)o.damageDealt), yy); yy -= dy;
    row("Golpes bloqueados", (float)p.blockedHits, (float)o.blockedHits, std::to_string(p.blockedHits), std::to_string(o.blockedHits), yy); yy -= dy;
    row("Esquivas", (float)p.dodges, (float)o.dodges, std::to_string(p.dodges), std::to_string(o.dodges), yy); yy -= dy;
    row("Contra-ataques", (float)p.counters, (float)o.counters, std::to_string(p.counters), std::to_string(o.counters), yy); yy -= dy;
    row("Quedas sofridas", (float)p.knockdowns, (float)o.knockdowns, std::to_string(p.knockdowns), std::to_string(o.knockdowns), yy);
}

static void roundBreakScreen(const Game& g) {
    rect(0, 0, W, H, INK, 0.4f);
    title(W / 2, H - 100, "FIM DO ROUND " + std::to_string(g.round), 6, GOLD);
    panel(W / 2 - 290, 140, 580, 400, PAPER);
    statsTable(g, W / 2 - 260, 490, 520);
    textC(W / 2, 96, fmt("Próximo round em %.0f s   (Enter para começar já)", std::max(0.0f, 6.0f - g.stateTimer)), S2, PAPER);
    textC(W / 2, 64, "No intervalo você recupera 30% da vida perdida e todo o fôlego.", S1, MUTED);
}

static void resultScreen(const Game& g) {
    rect(0, 0, W, H, INK, 0.4f);
    title(W / 2, H - 112, g.resultTitle, 9, g.playerWon ? GOLD : RED);
    textC(W / 2, H - 150, g.resultMethod, S3, PAPER);
    textC(W / 2, H - 180, g.resultExtra, S2, GOLD);
    panel(W / 2 - 290, 100, 580, 400, PAPER);
    statsTable(g, W / 2 - 260, 450, 520);
    if (g.stateTimer > 1 && std::sin(T * 4) > -0.3f) textC(W / 2, 50, "Enter para continuar", S2, PAPER);
}

static void trainHud(const Game& g) {
    const char* titles[3] = { "TREINO DE FORÇA - SACO DE PANCADAS", "TREINO DE AGILIDADE - MANOPLAS", "TREINO DE RESISTÊNCIA - CORRIDA" };
    const char* tips[3] = {
        "Soque o saco! Acertar quando ele VOLTA na sua direção vale 50% a mais. Cuidado com o fôlego.",
        "Acerte a manopla AMARELA com o golpe indicado. Às vezes o treinador ataca: esquive ou defenda!",
        "Aperte A e D alternadamente NO RITMO das marcas. Ritmo perfeito = mais velocidade."
    };
    const Col accent[3] = { RED, { 0.3f, 0.75f, 1.0f }, GREEN };
    float tw = std::min(W - 48, 1180.0f);
    panel(W / 2 - tw / 2, H - 96, tw, 80, accent[g.train]);
    textC(W / 2, H - 50, titles[g.train], S2, GOLD);
    textC(W / 2, H - 78, tips[g.train], S1, PAPER);

    // Painel de placar
    float px = 24, pw = 280, ptop = H - 120, ph = g.train != TR_RUN ? 270 : 220, py = ptop - ph;
    panel(px, py, pw, ph, accent[g.train]);
    float left = std::max(0.0f, 20.0f - g.trainTime);
    text(px + 24, ptop - 36, "TEMPO", S1, MUTED);
    text(px + 24, ptop - 82, fmt("%.1fs", g.trainStarted ? left : 20.0f), 6, PAPER);
    text(px + 24, ptop - 112, g.train == TR_RUN ? "DISTÂNCIA" : "PONTOS", S1, MUTED);
    text(px + 24, ptop - 150, g.train == TR_RUN ? fmt("%.0f m", g.runPos) : fmt("%.0f", g.trainScore), 5, PAPER);
    float cwid = tag(px + 24, ptop - 196, "x" + std::to_string(g.trainCombo), S2, GOLD, INK);
    text(px + 24 + cwid + 14, ptop - 182, "COMBO", S1, PAPER);
    text(px + 24 + cwid + 14, ptop - 200, "melhor " + std::to_string(g.trainBest), S1, MUTED);
    if (g.train != TR_RUN) {
        text(px + 24, ptop - 228, "FÔLEGO", S1, MUTED);
        bar(px + 110, ptop - 236, pw - 134, 18, g.player.stamina / g.player.maxStamina, { 0.91f, 0.89f, 0.85f });
    }
    if (!g.trainStarted) {
        int c = 3 - (int)g.stateTimer;
        title(W / 2, H / 2 - 40, c > 0 ? std::to_string(c) : "VAI!", 16, PAPER);
    }

    if (g.train == TR_MITTS && g.mittTarget >= 0) {
        float sx, sy;
        static const Vec3 P[6] = { Vec3(-0.14f, 1.58f, -0.72f), Vec3(0.14f, 1.58f, -0.72f), Vec3(-0.36f, 1.52f, -0.66f),
                                   Vec3(0.36f, 1.52f, -0.66f), Vec3(0.0f, 1.12f, -0.70f), Vec3(0.0f, 1.32f, -0.62f) };
        if (proj(P[g.mittTarget] + Vec3(0, 0.2f, 0), sx, sy)) {
            std::string l = Game::mittLabel(g.mittTarget);
            float w = textW(l, S2) + 2 * (S2 * 2 + 2);
            tag(sx - w / 2, sy, l, S2, GOLD, INK);
        }
        bar(W / 2 - 110, 110, 220, 18, g.mittTimer / std::max(0.1f, g.mittWindow), GOLD);
    }
    if (g.train == TR_MITTS && g.coachSwinging && g.hints) telegraphHints(g);

    if (g.train == TR_RUN) {
        // Trilha de ritmo: marcas se aproximam da linha central
        float cx = std::floor(W / 2), y = 120, w = 600;
        box(cx - w / 2, y - 26, w, 52, PANEL, 0.9f);
        rect(cx - 3, y - 30, 6, 60, PAPER);
        const float beat = 0.42f;
        int cur = (int)std::floor(g.runBeatT / beat);
        for (int n = cur - 1; n <= cur + 8; ++n) {
            if (n < 0) continue;
            float dtb = n * beat - g.runBeatT;       // tempo ate a batida n
            float x = std::floor(cx + dtb * 260);
            if (x < cx - w / 2 + 16 || x > cx + w / 2 - 16) continue;
            int foot = n % 2;
            bool done = n <= g.runLastBeat;
            float a = done ? 0.3f : 1.0f;
            Col fc = foot == 0 ? Col{ 0.3f, 0.6f, 1.0f } : Col{ 1.0f, 0.6f, 0.3f };
            rect(x - 12, y - 16, 24, 32, INK, a);
            rect(x - 10, y - 14, 20, 28, fc, a);
            textC(x, y - 6, foot == 0 ? "A" : "D", S1, INK, a);
        }
        int next = std::max(g.runLastBeat + 1, (int)std::floor(g.runBeatT / beat + 0.5f));
        textC(cx, y + 40, std::string("Próximo pé: ") + (next % 2 == 0 ? "A (esquerdo)" : "D (direito)"), S2, PAPER);
        if (g.runFeedbackT > 0) title(cx, y + 74, g.runFeedback, 4, GOLD, clampf(g.runFeedbackT * 2, 0, 1));
        float vx = W - 24 - 280;
        panel(vx, H - 220, 280, 100, GREEN);
        text(vx + 24, H - 156, "VELOCIDADE", S1, MUTED);
        textR(vx + 256, H - 156, fmt("%.1f m/s", g.runSpeed), S1, PAPER);
        bar(vx + 24, H - 196, 232, 22, g.runSpeed / 9.0f, GREEN);
    } else crosshair(g);
}

static void trainResultScreen(const Game& g) {
    rect(0, 0, W, H, INK, 0.45f);
    title(W / 2, H - 140, "TREINO CONCLUÍDO", 7, GREEN);
    panel(W / 2 - 320, H / 2 - 110, 640, 180, GREEN);
    textLines(W / 2 - 284, H / 2 + 20, g.trainResultText, S2, 34, PAPER);
    textC(W / 2, 80, "Enter para voltar à academia", S2, PAPER);
}

static void championScreen(const Game& g) {
    rect(0, 0, W, H, INK, 0.4f);
    bool on = std::sin(T * 3) > 0;
    title(W / 2, H * 0.6f, "CAMPEÃO!", 14, on ? GOLD : mix(GOLD, PAPER, 0.4f));
    const Career& c = g.career;
    textC(W / 2, H * 0.6f - 70, "Você venceu todos os adversários e conquistou o cinturão!", S3, PAPER);
    textC(W / 2, H * 0.6f - 110, "Dias de carreira: " + std::to_string(c.day) + "    Vitórias: " + std::to_string(c.wins) +
                                 "    Derrotas: " + std::to_string(c.losses) + "    Dinheiro: $" + std::to_string(c.money), S2, GOLD);
    textC(W / 2, 60, "Enter para voltar ao menu", S2, PAPER);
}

// ---------------------------------------------------------------------------
void render(const Game& g) {
    winW = (float)g.winW; winH = (float)g.winH; T = g.gameTime;
    H = 720; W = std::floor(H * winW / std::max(1.0f, winH));
    glViewport(0, 0, g.winW, g.winH);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, W, 0, H);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    switch (g.state) {
        case GS_MENU:         menuScreen(g); break;
        case GS_HELP:         helpScreen(); break;
        case GS_SELECT:       selectScreen(g); break;
        case GS_HUB:          hubScreen(g); break;
        case GS_PREFIGHT:     prefightScreen(g); break;
        case GS_FIGHT:        fightHud(g); break;
        case GS_ROUNDBREAK:   roundBreakScreen(g); break;
        case GS_RESULT:       resultScreen(g); break;
        case GS_TRAIN:        trainHud(g); break;
        case GS_TRAIN_RESULT: trainResultScreen(g); break;
        case GS_CHAMPION:     championScreen(g); break;
    }
    if (g.state == GS_FIGHT || g.state == GS_TRAIN) popups(g);
    messages(g);

    if (g.paused) {
        rect(0, 0, W, H, INK, 0.6f);
        title(W / 2, H / 2 + 10, "PAUSA", 10, PAPER);
        textC(W / 2, H / 2 - 50, "ESC ou P: continuar      Q: sair (sem resultado)", S2, PAPER);
    }
    if (g.showDebug) debugPanel(g);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

} // namespace Hud
