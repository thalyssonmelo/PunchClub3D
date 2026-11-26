// ============================================================================
//  Hud.cpp - Interface: barras, textos, menus, telas de resultado
//  Texto: fontes bitmap do GLUT (com conversao UTF-8 -> Latin-1 para acentos)
//  e fontes "stroke" (vetoriais, escalaveis) com acentos desenhados a mao.
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
static void* const F10 = GLUT_BITMAP_HELVETICA_10;
static void* const F12 = GLUT_BITMAP_HELVETICA_12;
static void* const F18 = GLUT_BITMAP_HELVETICA_18;
static void* const F24 = GLUT_BITMAP_TIMES_ROMAN_24;

// ---------------------------------------------------------------------------
// Texto
// ---------------------------------------------------------------------------
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

static int textW(const std::string& s, void* font) {
    int w = 0;
    for (unsigned cp : decodeUtf8(s)) w += glutBitmapWidth(font, cp < 256 ? (int)cp : '?');
    return w;
}
static void rawText(float x, float y, const std::string& s, void* font) {
    glRasterPos2f(x, y);
    for (unsigned cp : decodeUtf8(s)) glutBitmapCharacter(font, cp < 256 ? (int)cp : '?');
}
static void text(float x, float y, const std::string& s, void* font, float r, float g, float b, float a = 1) {
    if (x < 0) x = 0;
    glColor4f(0, 0, 0, a * 0.75f); rawText(x + 1.5f, y - 1.5f, s, font);
    glColor4f(r, g, b, a); rawText(x, y, s, font);
}
static void textC(float x, float y, const std::string& s, void* font, float r, float g, float b, float a = 1) {
    text(x - textW(s, font) / 2.0f, y, s, font, r, g, b, a);
}
static void textR(float x, float y, const std::string& s, void* font, float r, float g, float b, float a = 1) {
    text(x - textW(s, font), y, s, font, r, g, b, a);
}
// Varias linhas separadas por '\n'
static void textLines(float x, float y, const std::string& s, void* font, float lh, float r, float g, float b) {
    size_t st = 0;
    while (st <= s.size()) {
        size_t e = s.find('\n', st);
        if (e == std::string::npos) e = s.size();
        text(x, y, s.substr(st, e - st), font, r, g, b);
        y -= lh;
        st = e + 1;
    }
}

// Fonte vetorial: decompoe letras acentuadas em letra base + acento
static unsigned baseLetter(unsigned cp, int& accent, bool& upper) {
    // accent: 0 nenhum, 1 agudo, 2 grave, 3 circunflexo, 4 til, 5 cedilha
    accent = 0; upper = false;
    struct M { unsigned cp; char base; int acc; };
    static const M map[] = {
        { 0xE1, 'a', 1 }, { 0xE0, 'a', 2 }, { 0xE2, 'a', 3 }, { 0xE3, 'a', 4 }, { 0xE9, 'e', 1 }, { 0xEA, 'e', 3 },
        { 0xED, 'i', 1 }, { 0xF3, 'o', 1 }, { 0xF4, 'o', 3 }, { 0xF5, 'o', 4 }, { 0xFA, 'u', 1 }, { 0xE7, 'c', 5 },
        { 0xC1, 'A', 1 }, { 0xC0, 'A', 2 }, { 0xC2, 'A', 3 }, { 0xC3, 'A', 4 }, { 0xC9, 'E', 1 }, { 0xCA, 'E', 3 },
        { 0xCD, 'I', 1 }, { 0xD3, 'O', 1 }, { 0xD4, 'O', 3 }, { 0xD5, 'O', 4 }, { 0xDA, 'U', 1 }, { 0xC7, 'C', 5 },
    };
    for (const M& m : map) if (m.cp == cp) { accent = m.acc; upper = (m.base >= 'A' && m.base <= 'Z'); return (unsigned)m.base; }
    return cp < 128 ? cp : '?';
}
static float strokeW(const std::string& s) {
    float w = 0; int a; bool u;
    for (unsigned cp : decodeUtf8(s)) w += glutStrokeWidth(GLUT_STROKE_ROMAN, (int)baseLetter(cp, a, u));
    return w;
}
static void drawAccent(int acc, float cw, bool upper) {
    float cx = cw / 2, y0 = upper ? 112.0f : 82.0f;
    glBegin(GL_LINE_STRIP);
    switch (acc) {
        case 1: glVertex2f(cx - 10, y0); glVertex2f(cx + 14, y0 + 24); break;
        case 2: glVertex2f(cx - 14, y0 + 24); glVertex2f(cx + 10, y0); break;
        case 3: glVertex2f(cx - 18, y0); glVertex2f(cx, y0 + 20); glVertex2f(cx + 18, y0); break;
        case 4: glVertex2f(cx - 22, y0 + 4); glVertex2f(cx - 9, y0 + 16); glVertex2f(cx + 9, y0 + 5); glVertex2f(cx + 22, y0 + 17); break;
        case 5: glVertex2f(cx, 2); glVertex2f(cx, -12); glVertex2f(cx + 12, -20); glVertex2f(cx - 6, -30); break;
    }
    glEnd();
}
static void stroke(float x, float y, const std::string& s, float height, float r, float g, float b, float a,
                   float lw, bool center, bool outline = true) {
    float sc = height / 100.0f;
    float w = strokeW(s) * sc;
    // Espessura feita desenhando o texto varias vezes com pequenos deslocamentos
    // (funciona mesmo em drivers que limitam glLineWidth)
    auto pass = [&](float lwidth, float cr, float cg, float cb, float ca) {
        glColor4f(cr, cg, cb, ca);
        float rad = lwidth * 0.5f;
        int n = rad < 1.0f ? 1 : 10;
        for (int k = 0; k < n; ++k) {
        float ox = n == 1 ? 0 : std::cos(k * 2 * PI / n) * rad, oy = n == 1 ? 0 : std::sin(k * 2 * PI / n) * rad;
        glLineWidth(2.0f);
        glPushMatrix();
        glTranslatef((center ? x - w / 2 : x) + ox, y + oy, 0);
        glScalef(sc, sc, 1);
        for (unsigned cp : decodeUtf8(s)) {
            int acc; bool up;
            int base = (int)baseLetter(cp, acc, up);
            float cw = glutStrokeWidth(GLUT_STROKE_ROMAN, base);
            glutStrokeCharacter(GLUT_STROKE_ROMAN, base);
            if (acc) { glPushMatrix(); glTranslatef(-cw, 0, 0); drawAccent(acc, cw, up); glPopMatrix(); }
        }
        glPopMatrix();
        }
    };
    if (outline) pass(lw + 6, 0, 0, 0, a * 0.9f);
    pass(lw, r, g, b, a);
    pass(1.0f, std::min(1.0f, r + 0.35f), std::min(1.0f, g + 0.35f), std::min(1.0f, b + 0.35f), a);  // brilho interno
    glLineWidth(1);
}

// ---------------------------------------------------------------------------
// Formas
// ---------------------------------------------------------------------------
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void frame(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
static void panel(float x, float y, float w, float h, float a = 0.72f) {
    rect(x, y, w, h, 0.03f, 0.03f, 0.06f, a);
    rect(x, y + h - 3, w, 3, 0.85f, 0.15f, 0.12f, a);
    frame(x, y, w, h, 1, 1, 1, 0.15f);
}
static void bar(float x, float y, float w, float h, float frac, float r, float g, float b, bool rtl = false) {
    frac = clampf(frac, 0, 1);
    rect(x, y, w, h, 0, 0, 0, 0.6f);
    float fw = (w - 4) * frac;
    float fx = rtl ? x + 2 + (w - 4 - fw) : x + 2;
    glBegin(GL_QUADS);
    glColor4f(r * 0.6f, g * 0.6f, b * 0.6f, 0.95f); glVertex2f(fx, y + 2); glVertex2f(fx + fw, y + 2);
    glColor4f(r, g, b, 0.95f); glVertex2f(fx + fw, y + h - 2); glVertex2f(fx, y + h - 2);
    glEnd();
    frame(x, y, w, h, 1, 1, 1, 0.35f);
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

static std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }
static std::string fmt2(const char* f, double a, double b2) { char b[96]; std::snprintf(b, sizeof b, f, a, b2); return b; }

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
        stroke(W / 2, y, m.text, 46 * m.scale * pop * (H / 720.0f), m.r, m.g, m.b, a, 4, true);
        y -= 70 * m.scale * (H / 720.0f);
    }
}

static void popups(const Game& g) {
    for (const Popup& p : g.popups) {
        float sx, sy;
        if (!Scene::project(p.pos, sx, sy)) continue;
        float a = 1.0f - p.t / p.life;
        textC(sx, sy, p.text, F18, p.r, p.g, p.b, a);
    }
}

static void fighterBars(const Game& g, const Fighter& f, bool left, const std::string& name) {
    float bw = W * 0.32f, x = left ? 24 : W - 24 - bw, y = H - 46;
    float hp = f.hp / f.maxHp;
    float r = hp > 0.5f ? 0.2f + (1 - hp) * 1.2f : 1.0f, gg = hp > 0.5f ? 0.85f : hp * 1.6f;
    if (left) text(x, y + 22, name, F18, 1, 1, 1); else textR(x + bw, y + 22, name, F18, 1, 1, 1);
    bar(x, y, bw, 18, hp, r, gg, 0.15f, !left);
    bar(left ? x : x + bw * 0.35f, y - 14, bw * 0.65f, 10, f.stamina / f.maxStamina, 1.0f, 0.82f, 0.15f, !left);
    // quedas (knockdowns)
    for (int i = 0; i < 3; ++i) {
        float px = left ? x + bw * 0.68f + i * 16 : x + bw * 0.32f - 12 - i * 16;
        rect(px, y - 14, 10, 10, i < f.knockdowns ? 0.9f : 0.2f, 0.15f, 0.15f, 0.9f);
    }
    std::string st;
    if (f.status != ST_ACTIVE) st = "CAÍDO";
    else if (f.stunTimer > 0) st = "ATORDOADO";
    else if (f.stamina < f.maxStamina * 0.15f) st = "SEM FÔLEGO";
    else if (f.exposedTimer > 0) st = "EXPOSTO";
    if (f.furyTimer > 0) st = "FÚRIA " + fmt("%.1fs", f.furyTimer);
    if (!st.empty()) { if (left) text(x, y - 32, st, F12, 1, 0.5f, 0.3f); else textR(x + bw, y - 32, st, F12, 1, 0.5f, 0.3f); }
    (void)g;
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
        if (!Scene::project(g.opp.headPos() + Vec3(0, 0.32f, 0), sx, sy)) continue;
        std::string name = hd.feint ? "???" : punchName(hd.type);
        float side = (h == 0) ? -1.0f : 1.0f;   // mao esquerda do adversario aparece a DIREITA da tela
        std::string arrow = hd.type == GANCHO ? (side < 0 ? " vem da DIREITA" : " vem da ESQUERDA") : "";
        float pulse = 0.6f + 0.4f * std::sin(T * 25);
        textC(sx, sy, "! " + name + arrow + " !", F18, 1, 0.85f * pulse, 0.2f);
        if (!hd.feint) textC(sx, sy - 18, defenseHint(hd.type), F12, 1, 1, 1, 0.85f);
    }
}

static void crosshair(const Game& g) {
    float cx = W / 2, cy = H / 2;
    float a = 0.35f;
    float col[3] = { 1, 1, 1 };
    if (g.player.counterTimer > 0) { col[0] = 1; col[1] = 0.8f; col[2] = 0.1f; a = 0.9f; }
    glColor4f(col[0], col[1], col[2], a);
    glLineWidth(2);
    glBegin(GL_LINES);
    glVertex2f(cx - 10, cy); glVertex2f(cx - 4, cy); glVertex2f(cx + 4, cy); glVertex2f(cx + 10, cy);
    glVertex2f(cx, cy - 10); glVertex2f(cx, cy - 4); glVertex2f(cx, cy + 4); glVertex2f(cx, cy + 10);
    glEnd();
    glLineWidth(1);
    if (g.player.counterTimer > 0) {
        textC(cx, cy - 34, "JANELA DE CONTRA-ATAQUE!", F12, 1, 0.8f, 0.2f);
        bar(cx - 60, cy - 48, 120, 6, g.player.counterTimer / 0.7f, 1, 0.8f, 0.2f);
    }
}

static void controlsFooter() {
    std::string c = "J/Clique esq: soco esquerdo   L/Clique dir: soco direito   +W uppercut  +S corpo  +A/D gancho   "
                    "Espaço: guarda (+S baixa)   A/D: inclinar   S: abaixar   F: fúria   P: pausa   H: dicas";
    textC(W / 2, 12, c, F10, 0.85f, 0.85f, 0.85f, 0.8f);
}

static void fightHud(const Game& g) {
    // vinhetas
    float lowHp = g.player.hp < g.player.maxHp * 0.3f ? (0.25f + 0.15f * std::sin(T * 6)) : 0;
    vignette(0.8f, 0, 0, std::max(g.redFlash * 0.8f, lowHp));
    if (g.player.furyTimer > 0) vignette(1.0f, 0.3f, 0.0f, 0.35f + 0.1f * std::sin(T * 12));
    if (g.whiteFlash > 0) rect(0, 0, W, H, 1, 1, 1, g.whiteFlash * 0.6f);

    fighterBars(g, g.player, true, "VOCÊ");
    fighterBars(g, g.opp, false, g.opp.name);

    // Round e relogio
    int t = (int)std::ceil(std::max(0.0f, g.roundTime));
    char clock[32]; std::snprintf(clock, sizeof clock, "%d:%02d", t / 60, t % 60);
    panel(W / 2 - 70, H - 64, 140, 58, 0.6f);
    textC(W / 2, H - 26, "ROUND " + std::to_string(g.round) + "/" + std::to_string(Game::NUM_ROUNDS), F12, 1, 0.85f, 0.3f);
    textC(W / 2, H - 52, clock, F24, t <= 10 ? 1.0f : 1.0f, t <= 10 ? 0.3f : 1.0f, t <= 10 ? 0.3f : 1.0f);

    // Barra de furia
    float fw = 300, fx = W / 2 - fw / 2, fy = 34;
    bool full = g.player.fury >= 100;
    float pulse = full ? 0.7f + 0.3f * std::sin(T * 10) : 1.0f;
    bar(fx, fy, fw, 14, g.player.fury / 100.0f, 1.0f * pulse, 0.35f * pulse, 0.1f);
    textC(W / 2, fy + 20, full ? "FÚRIA PRONTA! aperte F" : "FÚRIA", F12, 1, full ? 0.9f : 0.7f, 0.3f);

    crosshair(g);
    telegraphHints(g);

    // Contagem de knockdown
    if (g.whoDown == 1) {
        rect(0, 0, W, H, 0, 0, 0, 0.25f);
        textC(W / 2, H * 0.3f, "LEVANTE! Aperte J e L ALTERNADAMENTE!", F18, 1, 1, 0.3f);
        bar(W / 2 - 200, H * 0.3f - 34, 400, 20, g.mash, 0.3f, 1, 0.4f);
    }
    if (g.whoDown != 0 && g.pendingTKO < 0)
        textC(W / 2, H * 0.22f, std::string("Contagem: ") + std::to_string(g.count), F18, 1, 1, 1);
    if (g.hints) controlsFooter();
}

// ---------------------------------------------------------------------------
static void debugPanel(const Game& g) {
    float x = W - 380, y = H - 120, w = 360, h = 300;
    panel(x, y - h, w, h, 0.8f);
    float ly = y - 20;
    auto line = [&](const std::string& s, float r = 0.85f, float gg = 0.9f, float b = 1) { text(x + 10, ly, s, F12, r, gg, b); ly -= 16; };
    line("DEPURAÇÃO (F1)", 1, 0.8f, 0.3f);
    line(fmt("FPS: %.0f", g.fps));
    line(std::string("Sombras F2: ") + (g.shadows ? "on" : "off") + "  Neblina F3: " + (g.fog ? "on" : "off") +
         "  Smooth F4: " + (g.smooth ? "on" : "off"));
    line(std::string("Texturas F5: ") + (Textures::enabled ? "on" : "off") + "  Luz F6: " + (g.lighting ? "on" : "off") +
         "  Arame F7: " + (g.wire ? "on" : "off"));
    line(std::string("Cones de luz F8: ") + (g.cones ? "on" : "off") + "  Som (M): " + (Audio::isMuted() ? "mudo" : (Audio::available() ? "on" : "indisp.")));
    line("Arquibancada visível (frustum): " + std::to_string(g.crowdDrawn) + "/" + std::to_string(g.crowdTotal));
    if (g.state == GS_FIGHT) {
        ly -= 6;
        line("IA - estado: " + std::string(g.ai.modeName()), 1, 0.8f, 0.3f);
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
static void menuScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.35f);
    stroke(W / 2, H * 0.72f, "PUNCH CLUB 3D", 90 * H / 720, 0.95f, 0.2f, 0.15f, 1, 6, true);
    textC(W / 2, H * 0.72f - 40, "Boxe em primeira pessoa  -  Trabalho Final de Computação Gráfica", F18, 1, 0.9f, 0.7f);
    const char* items[5] = { "Nova carreira", "Continuar carreira", "Luta rápida", "Como jogar", "Sair" };
    float y = H * 0.48f;
    for (int i = 0; i < 5; ++i) {
        bool sel = g.menuSel == i;
        bool dis = (i == 1 && !g.hasSave);
        if (sel) rect(W / 2 - 170, y - 10, 340, 34, 0.85f, 0.15f, 0.12f, 0.6f + 0.2f * std::sin(T * 5));
        textC(W / 2, y, items[i], F24, dis ? 0.5f : 1, dis ? 0.5f : 1, dis ? 0.5f : 1);
        y -= 46;
    }
    if (g.hubNoticeT > 0) textC(W / 2, y - 10, g.hubNotice, F18, 1, 0.6f, 0.3f);
    textC(W / 2, 30, "W/S ou setas: navegar   Enter: escolher   M: som   F1: depuração   F2-F8: recursos gráficos", F12, 0.8f, 0.8f, 0.8f);
}

static void helpScreen() {
    rect(0, 0, W, H, 0, 0, 0, 0.55f);
    float x = W / 2 - 520, y = H - 60;
    panel(x - 20, 40, 1080, H - 80, 0.85f);
    stroke(W / 2, y - 20, "COMO JOGAR", 40, 1, 0.85f, 0.2f, 1, 3, true);
    std::string left =
        "ATAQUE (o golpe depende da sua postura):\n"
        "  J / clique esquerdo ......... soco com a mão esquerda\n"
        "  L / clique direito .......... soco com a mão direita\n"
        "  parado + soco ............... DIRETO (rápido, barato)\n"
        "  segurando A ou D + soco ..... GANCHO (curva por fora)\n"
        "  segurando W + soco .......... UPPERCUT (forte, caro)\n"
        "  segurando S + soco .......... golpe no CORPO (tira fôlego)\n"
        "\n"
        "DEFESA:\n"
        "  Espaço ...................... guarda alta (protege a cabeça)\n"
        "  Espaço + S .................. guarda baixa (protege o corpo)\n"
        "  A / D ....................... inclinar (esquiva lateral)\n"
        "  S ........................... abaixar (esquiva diretos/ganchos)\n"
        "  Não é possível socar com a guarda fechada!\n"
        "\n"
        "LEIA O ADVERSÁRIO: a luva dele brilha durante o preparo:\n"
        "  branco = direto   amarelo = gancho   vermelho = uppercut\n"
        "  azul = corpo.  O golpe vai onde você ESTAVA quando ele\n"
        "  começou a preparar: saia da linha a tempo!";
    std::string right =
        "MECÂNICAS ESPECIAIS:\n"
        "  CONTRA-ATAQUE: esquive e acerte logo depois = dano x1.6\n"
        "  APARAR: levante a guarda no último instante = o atacante\n"
        "     fica atordoado e você ganha a janela de contra-ataque\n"
        "  Golpe errado deixa quem errou EXPOSTO (dano x1.25)\n"
        "  Guarda gasta fôlego; sem fôlego a guarda QUEBRA\n"
        "  Sem fôlego seus socos ficam lentos e fracos\n"
        "  FÚRIA (F): enche acertando e esquivando. Deixa o\n"
        "     adversário em câmera lenta e seus golpes x1.5\n"
        "  KNOCKDOWN: alterne J e L rápido para levantar antes do 10\n"
        "  3 quedas = nocaute técnico. 3 rounds de 90s, senão pontos\n"
        "\n"
        "CARREIRA (estilo Punch Club):\n"
        "  Treine Força (saco), Agilidade (manoplas) e Resistência\n"
        "  (corrida). Trabalhe para ganhar dinheiro, coma para ter\n"
        "  energia e durma para passar o dia. Atributos não\n"
        "  treinados caem com o tempo! Vença os 5 lutadores.\n"
        "\n"
        "IA: cada adversário tem personalidade. O campeão APRENDE\n"
        "seus padrões (Markov) e escolhe golpes que funcionam (UCB1).";
    textLines(x, y - 70, left, F12, 19, 0.95f, 0.95f, 0.95f);
    textLines(x + 540, y - 70, right, F12, 19, 0.95f, 0.95f, 0.95f);
    textC(W / 2, 56, "Enter ou ESC para voltar", F18, 1, 0.8f, 0.3f);
}

static void statBar(float x, float y, const std::string& label, float v, float maxv, float r, float g, float b, float w = 220) {
    text(x, y, label, F12, 0.9f, 0.9f, 0.9f);
    bar(x + 100, y - 3, w, 14, v / maxv, r, g, b);
    text(x + 108 + w, y, fmt("%.2f", v), F12, 1, 1, 1);
}

static void opponentCard(const OpponentDef& d, float x, float y, float w, float h, const std::string& header) {
    panel(x, y, w, h, 0.8f);
    text(x + 16, y + h - 26, header, F12, 1, 0.7f, 0.3f);
    text(x + 16, y + h - 52, d.name, F24, 1, 1, 1);
    text(x + 16, y + h - 72, std::string("\"") + d.nick + "\"", F12, 1, 0.85f, 0.4f);
    text(x + 16, y + h - 94, d.desc, F12, 0.85f, 0.85f, 0.85f);
    statBar(x + 16, y + h - 120, "Força", d.forca, 10, 1, 0.35f, 0.2f);
    statBar(x + 16, y + h - 140, "Agilidade", d.agilidade, 10, 0.3f, 0.8f, 1);
    statBar(x + 16, y + h - 160, "Resistência", d.resistencia, 10, 0.4f, 1, 0.4f);
    text(x + 16, y + h - 184, "Prêmio: $" + std::to_string(d.reward) + fmt("    Aprendizado da IA: %.0f%%", d.p.learning * 100), F12, 0.9f, 0.9f, 0.5f);
}

static void selectScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.3f);
    stroke(W / 2, H - 90, "LUTA RÁPIDA", 50, 1, 0.85f, 0.2f, 1, 4, true);
    const OpponentDef& d = Game::opponentDef(g.selectSel);
    opponentCard(d, W / 2 - 280, 120, 560, 220, "ADVERSÁRIO " + std::to_string(g.selectSel + 1) + "/" + std::to_string(Game::numOpponents()));
    textC(W / 2, 92, "<  A / D  >   escolher      Enter: lutar      ESC: voltar", F18, 1, 1, 1);
    textC(W / 2, 66, "Na luta rápida você tem Força 6, Agilidade 6 e Resistência 6", F12, 0.8f, 0.8f, 0.8f);
}

static void hubScreen(const Game& g) {
    const Career& c = g.career;
    stroke(W / 2, H - 70, "ACADEMIA", 46, 1, 0.85f, 0.2f, 1, 4, true);
    // Painel do lutador
    float px = 30, py = H - 380, pw = 430, ph = 290;
    panel(px, py, pw, ph);
    text(px + 16, py + ph - 28, "SEU LUTADOR", F18, 1, 0.8f, 0.3f);
    text(px + 16, py + ph - 56, "Dia " + std::to_string(c.day) + "     Dinheiro: $" + std::to_string(c.money) + "     Fama: " + std::to_string(c.fame), F12, 1, 1, 1);
    text(px + 16, py + ph - 76, "Cartel: " + std::to_string(c.wins) + " vitórias / " + std::to_string(c.losses) + " derrotas", F12, 1, 1, 1);
    text(px + 16, py + ph - 104, "Energia", F12, 0.9f, 0.9f, 0.9f);
    bar(px + 116, py + ph - 107, 220, 14, c.energy / 100.0f, 0.3f, 0.7f, 1);
    text(px + 344, py + ph - 104, std::to_string(c.energy) + "/100", F12, 1, 1, 1);
    statBar(px + 16, py + ph - 134, std::string("Força") + (c.trained[0] ? " *" : ""), c.forca, 10, 1, 0.35f, 0.2f);
    statBar(px + 16, py + ph - 156, std::string("Agilidade") + (c.trained[1] ? " *" : ""), c.agilidade, 10, 0.3f, 0.8f, 1);
    statBar(px + 16, py + ph - 178, std::string("Resistência") + (c.trained[2] ? " *" : ""), c.resistencia, 10, 0.4f, 1, 0.4f);
    textLines(px + 16, py + ph - 206,
              "* treinado hoje. Atributos não treinados caem ao dormir.\n"
              "Força: dano.  Agilidade: velocidade de socos e esquivas.\n"
              "Resistência: vida, fôlego e recuperação.", F10, 14, 0.75f, 0.75f, 0.75f);
    // Acoes
    float ax = W - 560, ay = H - 420, aw = 530, ah = 330;
    panel(ax, ay, aw, ah);
    text(ax + 16, ay + ah - 28, "O QUE FAZER HOJE?", F18, 1, 0.8f, 0.3f);
    for (int i = 0; i < 8; ++i) {
        float y = ay + ah - 62 - i * 32;
        if (g.hubSel == i) rect(ax + 8, y - 9, aw - 16, 28, 0.85f, 0.15f, 0.12f, 0.55f + 0.2f * std::sin(T * 5));
        text(ax + 20, y, std::to_string(i + 1) + ".  " + hubItem(i), F12, 1, 1, 1);
    }
    // Proximo adversario
    if (c.next < Game::numOpponents())
        opponentCard(Game::opponentDef(c.next), 30, 60, 560, 200, "PRÓXIMO ADVERSÁRIO (" + std::to_string(c.next + 1) + "/" + std::to_string(Game::numOpponents()) + ")");
    else { panel(30, 160, 560, 100); text(46, 210, "Você é o CAMPEÃO! Continue treinando ou revanche.", F18, 1, 0.85f, 0.2f); }
    if (g.hubNoticeT > 0) {
        float a = clampf(g.hubNoticeT, 0, 1);
        panel(W - 560, 150, 530, 46, 0.8f * a);
        textC(W - 295, 167, g.hubNotice, F12, 1, 0.9f, 0.5f, a);
    }
    textC(W / 2, 22, "W/S ou setas: escolher    Enter ou 1-8: confirmar    ESC: salvar e sair", F12, 0.8f, 0.8f, 0.8f);
}

static void prefightScreen(const Game& g) {
    const OpponentDef& d = Game::opponentDef(g.oppIndex);
    float a = clampf(g.stateTimer * 2, 0, 1);
    rect(0, 0, W, 130, 0, 0, 0, 0.6f * a);
    rect(0, H - 130, W, 130, 0, 0, 0, 0.6f * a);
    stroke(W * 0.25f, H - 95, "VOCÊ", 54, 0.3f, 0.6f, 1, a, 4, true);
    stroke(W / 2, H - 100, "VS", 70, 1, 0.85f, 0.2f, a, 6, true);
    stroke(W * 0.75f, H - 95, d.name, 54, 1, 0.3f, 0.25f, a, 4, true);
    textC(W * 0.75f, H - 120, std::string("\"") + d.nick + "\" - " + d.desc, F12, 1, 0.9f, 0.7f, a);
    const Fighter& p = g.player;
    textC(W * 0.25f, H - 120, fmt2("Força %.1f   Agilidade %.1f", p.forca, p.agilidade) + fmt("   Resistência %.1f", p.resistencia), F12, 0.8f, 0.9f, 1, a);
    textC(W / 2, 60, std::string(g.quickFight ? "Luta rápida" : "Carreira - luta " + std::to_string(g.oppIndex + 1)) + "   |   3 rounds de 90 segundos", F18, 1, 1, 1, a);
    textC(W / 2, 32, "Enter para pular", F12, 0.8f, 0.8f, 0.8f, a);
}

static void statsTable(const Game& g, float x, float y) {
    auto row = [&](const std::string& lbl, const std::string& a, const std::string& b, float yy) {
        text(x, yy, lbl, F12, 0.85f, 0.85f, 0.85f);
        textR(x + 330, yy, a, F12, 0.5f, 0.8f, 1);
        textR(x + 450, yy, b, F12, 1, 0.5f, 0.4f);
    };
    const Fighter& p = g.player; const Fighter& o = g.opp;
    row("", "VOCÊ", o.name, y);
    auto pct = [](const Fighter& f) { return f.thrown ? std::to_string(100 * f.landed / f.thrown) + "%" : std::string("-"); };
    row("Golpes lançados", std::to_string(p.thrown), std::to_string(o.thrown), y - 20);
    row("Golpes conectados", std::to_string(p.landed), std::to_string(o.landed), y - 40);
    row("Precisão", pct(p), pct(o), y - 60);
    row("Dano causado", std::to_string((int)p.damageDealt), std::to_string((int)o.damageDealt), y - 80);
    row("Golpes bloqueados", std::to_string(p.blockedHits), std::to_string(o.blockedHits), y - 100);
    row("Esquivas", std::to_string(p.dodges), std::to_string(o.dodges), y - 120);
    row("Contra-ataques", std::to_string(p.counters), std::to_string(o.counters), y - 140);
    row("Quedas sofridas", std::to_string(p.knockdowns), std::to_string(o.knockdowns), y - 160);
}

static void roundBreakScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.35f);
    stroke(W / 2, H - 110, "FIM DO ROUND " + std::to_string(g.round), 54, 1, 0.85f, 0.2f, 1, 4, true);
    panel(W / 2 - 250, H / 2 - 170, 500, 240);
    statsTable(g, W / 2 - 225, H / 2 + 40);
    textC(W / 2, 80, fmt("Próximo round em %.0f s   (Enter para começar já)", std::max(0.0f, 6.0f - g.stateTimer)), F18, 1, 1, 1);
    textC(W / 2, 54, "No intervalo você recupera 30% da vida perdida e todo o fôlego.", F12, 0.8f, 0.8f, 0.8f);
}

static void resultScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.35f);
    stroke(W / 2, H - 120, g.resultTitle, 80, g.playerWon ? 0.3f : 1, g.playerWon ? 1 : 0.3f, 0.3f, 1, 6, true);
    textC(W / 2, H - 150, g.resultMethod, F24, 1, 1, 1);
    textC(W / 2, H - 176, g.resultExtra, F18, 1, 0.85f, 0.4f);
    panel(W / 2 - 250, H / 2 - 190, 500, 240);
    statsTable(g, W / 2 - 225, H / 2 + 20);
    if (g.stateTimer > 1) textC(W / 2, 60, "Enter para continuar", F18, 1, 1, 1, 0.6f + 0.4f * std::sin(T * 4));
}

static void trainHud(const Game& g) {
    const char* titles[3] = { "TREINO DE FORÇA - SACO DE PANCADAS", "TREINO DE AGILIDADE - MANOPLAS", "TREINO DE RESISTÊNCIA - CORRIDA" };
    const char* tips[3] = {
        "Soque o saco! Acertar quando ele VOLTA na sua direção vale 50% a mais. Cuidado com o fôlego.",
        "Acerte a manopla AMARELA com o golpe indicado. Às vezes o treinador ataca: esquive ou defenda!",
        "Aperte A e D alternadamente NO RITMO das marcas. Ritmo perfeito = mais velocidade."
    };
    panel(W / 2 - 400, H - 80, 800, 70, 0.7f);
    textC(W / 2, H - 36, titles[g.train], F18, 1, 0.85f, 0.3f);
    textC(W / 2, H - 60, tips[g.train], F12, 1, 1, 1);
    float left = std::max(0.0f, 20.0f - g.trainTime);
    text(30, H - 120, fmt("Tempo: %.1f s", g.trainStarted ? left : 20.0f), F18, 1, 1, 1);
    if (g.train == TR_RUN) text(30, H - 146, fmt("Distância: %.0f m", g.runPos), F18, 1, 1, 1);
    else text(30, H - 146, fmt("Pontos: %.0f", g.trainScore), F18, 1, 1, 1);
    text(30, H - 172, "Combo: " + std::to_string(g.trainCombo) + "   (melhor " + std::to_string(g.trainBest) + ")", F12, 1, 0.85f, 0.4f);
    if (g.train != TR_RUN) {
        text(30, H - 196, "Fôlego", F12, 0.9f, 0.9f, 0.9f);
        bar(90, H - 200, 200, 12, g.player.stamina / g.player.maxStamina, 1, 0.82f, 0.15f);
    }
    if (!g.trainStarted) {
        int c = 3 - (int)g.stateTimer;
        stroke(W / 2, H / 2, c > 0 ? std::to_string(c) : "VAI!", 120, 1, 1, 1, 1, 6, true);
    }

    if (g.train == TR_MITTS && g.mittTarget >= 0) {
        float sx, sy;
        static const Vec3 P[6] = { Vec3(-0.14f, 1.58f, -0.72f), Vec3(0.14f, 1.58f, -0.72f), Vec3(-0.36f, 1.52f, -0.66f),
                                   Vec3(0.36f, 1.52f, -0.66f), Vec3(0.0f, 1.12f, -0.70f), Vec3(0.0f, 1.32f, -0.62f) };
        if (Scene::project(P[g.mittTarget] + Vec3(0, 0.2f, 0), sx, sy)) textC(sx, sy, Game::mittLabel(g.mittTarget), F18, 1, 0.9f, 0.2f);
        bar(W / 2 - 100, 120, 200, 10, g.mittTimer / std::max(0.1f, g.mittWindow), 1, 0.8f, 0.2f);
    }
    if (g.train == TR_MITTS && g.coachSwinging && g.hints) telegraphHints(g);

    if (g.train == TR_RUN) {
        // Trilha de ritmo: marcas se aproximam da linha central
        float cx = W / 2, y = 120, w = 600;
        rect(cx - w / 2, y - 22, w, 44, 0, 0, 0, 0.55f);
        rect(cx - 2, y - 26, 4, 52, 1, 1, 1, 0.9f);
        const float beat = 0.42f;
        int cur = (int)std::floor(g.runBeatT / beat);
        for (int n = cur - 1; n <= cur + 8; ++n) {
            if (n < 0) continue;
            float dtb = n * beat - g.runBeatT;       // tempo ate a batida n
            float x = cx + dtb * 260;
            if (x < cx - w / 2 || x > cx + w / 2) continue;
            int foot = n % 2;
            bool done = n <= g.runLastBeat;
            float a = done ? 0.25f : 0.9f;
            rect(x - 9, y - 14, 18, 28, foot == 0 ? 0.3f : 1.0f, 0.6f, foot == 0 ? 1.0f : 0.3f, a);
            textC(x, y - 5, foot == 0 ? "A" : "D", F12, 0, 0, 0, a);
        }
        int next = std::max(g.runLastBeat + 1, (int)std::floor(g.runBeatT / beat + 0.5f));
        textC(cx, y + 34, std::string("Próximo pé: ") + (next % 2 == 0 ? "A (esquerdo)" : "D (direito)"), F18, 1, 1, 1);
        if (g.runFeedbackT > 0) textC(cx, y + 60, g.runFeedback, F24, 1, 0.9f, 0.3f, clampf(g.runFeedbackT * 2, 0, 1));
        text(W - 260, H - 120, fmt("Velocidade: %.1f m/s", g.runSpeed), F18, 1, 1, 1);
        bar(W - 260, H - 146, 220, 12, g.runSpeed / 9.0f, 0.4f, 1, 0.4f);
    } else crosshair(g);
}

static void trainResultScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.4f);
    stroke(W / 2, H - 130, "TREINO CONCLUÍDO", 60, 0.3f, 1, 0.4f, 1, 5, true);
    panel(W / 2 - 300, H / 2 - 80, 600, 140);
    textLines(W / 2 - 270, H / 2 + 20, g.trainResultText, F18, 30, 1, 1, 1);
    textC(W / 2, 80, "Enter para voltar à academia", F18, 1, 1, 1);
}

static void championScreen(const Game& g) {
    rect(0, 0, W, H, 0, 0, 0, 0.35f);
    float pulse = 0.85f + 0.15f * std::sin(T * 3);
    stroke(W / 2, H * 0.6f, "CAMPEÃO!", 120 * pulse, 1, 0.85f, 0.15f, 1, 7, true);
    const Career& c = g.career;
    textC(W / 2, H * 0.6f - 60, "Você venceu todos os adversários e conquistou o cinturão!", F24, 1, 1, 1);
    textC(W / 2, H * 0.6f - 92, "Dias de carreira: " + std::to_string(c.day) + "    Vitórias: " + std::to_string(c.wins) +
                                "    Derrotas: " + std::to_string(c.losses) + "    Dinheiro: $" + std::to_string(c.money), F18, 1, 0.9f, 0.6f);
    textC(W / 2, 60, "Enter para voltar ao menu", F18, 1, 1, 1);
}

// ---------------------------------------------------------------------------
void render(const Game& g) {
    W = (float)g.winW; H = (float)g.winH; T = g.gameTime;
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
        rect(0, 0, W, H, 0, 0, 0, 0.55f);
        stroke(W / 2, H / 2 + 20, "PAUSA", 80, 1, 1, 1, 1, 5, true);
        textC(W / 2, H / 2 - 40, "ESC ou P: continuar      Q: sair (sem resultado)", F18, 1, 1, 1);
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
