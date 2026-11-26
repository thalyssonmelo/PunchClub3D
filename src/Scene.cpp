// ============================================================================
//  Scene.cpp - Tudo que e desenhado em 3D
//
//  Recursos de OpenGL usados aqui:
//   - Iluminacao: holofote (spotlight) sobre o ringue, luz de preenchimento
//     direcional, luz pontual dos flashes da plateia e materiais com brilho
//     especular. GL_NORMALIZE por causa das escalas nao uniformes.
//   - Sombreamento: Gouraud (GL_SMOOTH) ou flat (F4). Pisos subdivididos em
//     grade para que a iluminacao por vertice mostre o cone do holofote.
//   - Texturas procedurais com mipmaps, modo GL_MODULATE (textura x luz).
//   - Sombras planares projetadas com matriz de sombra + STENCIL BUFFER
//     (a sombra so aparece sobre a lona e sem dupla mistura).
//   - Visibilidade: Z-buffer, back-face culling, neblina (GL_FOG) e
//     descarte por FRUSTUM das secoes da arquibancada fora do campo de visao.
//   - Transparencia/mistura: cones de luz aditivos e particulas.
//   - Modelagem hierarquica: os lutadores sao montados a partir de pontos
//     articulados (ombros, cotovelos via cinematica inversa, luvas...).
// ============================================================================
#include "Scene.h"
#include "Game.h"
#include "Textures.h"
#include "GLInclude.h"

namespace Scene {

static GLUquadric* Q = nullptr;
static bool gShadow = false;          // desenhando a sombra (sem materiais)
static GLdouble MV[16], PJ[16];
static GLint VP[4];
static float gTime = 0;

void init() {
    Q = gluNewQuadric();
    gluQuadricNormals(Q, GLU_SMOOTH);
    gluQuadricTexture(Q, GL_TRUE);
}

bool project(const Vec3& p, float& sx, float& sy) {
    GLdouble x, y, z;
    if (gluProject(p.x, p.y, p.z, MV, PJ, VP, &x, &y, &z) != GL_TRUE) return false;
    sx = (float)x; sy = (float)y;
    return z > 0 && z < 1;
}

// ---------------------------------------------------------------------------
// Utilitarios de desenho
// ---------------------------------------------------------------------------
static void mat(float r, float g, float b, float spec = 0.15f, float shin = 16, float er = 0, float eg = 0, float eb = 0) {
    if (gShadow) return;
    GLfloat a[4] = { r * 0.55f, g * 0.55f, b * 0.55f, 1 }, d[4] = { r, g, b, 1 };
    GLfloat s[4] = { spec, spec, spec, 1 }, e[4] = { er, eg, eb, 1 };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, a);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, d);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, s);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shin);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, e);
    glColor3f(std::min(1.0f, r + er), std::min(1.0f, g + eg), std::min(1.0f, b + eb));
}
static void tex(TexId t) { if (!gShadow) Textures::bind(t); }
static void notex() { Textures::unbind(); }

static void sphere(float r, int sl = 18, int st = 12) { gluSphere(Q, r, sl, st); }

static void translate(const Vec3& p) { glTranslatef(p.x, p.y, p.z); }

// Multiplica pela matriz de um referencial (colunas = eixos)
static void multFrame(const Vec3& pos, const Vec3& R, const Vec3& U, const Vec3& F) {
    GLfloat m[16] = { R.x, R.y, R.z, 0, U.x, U.y, U.z, 0, F.x, F.y, F.z, 0, pos.x, pos.y, pos.z, 1 };
    glMultMatrixf(m);
}

// Cilindro (membro) ligando dois pontos
static void limb(const Vec3& A, const Vec3& B, float rA, float rB, int slices = 12) {
    Vec3 d = B - A;
    float L = length(d);
    if (L < 1e-4f) return;
    Vec3 z = d / L;
    glPushMatrix();
    translate(A);
    Vec3 axis = cross(Vec3(0, 0, 1), z);
    float ang = std::acos(clampf(z.z, -1, 1)) * RAD2DEG;
    if (length(axis) < 1e-5f) { if (z.z < 0) glRotatef(180, 1, 0, 0); }
    else glRotatef(ang, axis.x, axis.y, axis.z);
    gluCylinder(Q, rA, rB, L, slices, 1);
    glPopMatrix();
}

// Caixa centrada com normais e coordenadas de textura
static void box(float sx, float sy, float sz, float rep = 1) {
    float x = sx / 2, y = sy / 2, z = sz / 2;
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glTexCoord2f(0, 0); glVertex3f(-x, -y, z); glTexCoord2f(rep, 0); glVertex3f(x, -y, z); glTexCoord2f(rep, rep); glVertex3f(x, y, z); glTexCoord2f(0, rep); glVertex3f(-x, y, z);
    glNormal3f(0, 0, -1); glTexCoord2f(0, 0); glVertex3f(x, -y, -z); glTexCoord2f(rep, 0); glVertex3f(-x, -y, -z); glTexCoord2f(rep, rep); glVertex3f(-x, y, -z); glTexCoord2f(0, rep); glVertex3f(x, y, -z);
    glNormal3f(1, 0, 0);  glTexCoord2f(0, 0); glVertex3f(x, -y, z); glTexCoord2f(rep, 0); glVertex3f(x, -y, -z); glTexCoord2f(rep, rep); glVertex3f(x, y, -z); glTexCoord2f(0, rep); glVertex3f(x, y, z);
    glNormal3f(-1, 0, 0); glTexCoord2f(0, 0); glVertex3f(-x, -y, -z); glTexCoord2f(rep, 0); glVertex3f(-x, -y, z); glTexCoord2f(rep, rep); glVertex3f(-x, y, z); glTexCoord2f(0, rep); glVertex3f(-x, y, -z);
    glNormal3f(0, 1, 0);  glTexCoord2f(0, 0); glVertex3f(-x, y, z); glTexCoord2f(rep, 0); glVertex3f(x, y, z); glTexCoord2f(rep, rep); glVertex3f(x, y, -z); glTexCoord2f(0, rep); glVertex3f(-x, y, -z);
    glNormal3f(0, -1, 0); glTexCoord2f(0, 0); glVertex3f(-x, -y, -z); glTexCoord2f(rep, 0); glVertex3f(x, -y, -z); glTexCoord2f(rep, rep); glVertex3f(x, -y, z); glTexCoord2f(0, rep); glVertex3f(-x, -y, z);
    glEnd();
}

// Piso horizontal subdividido (a iluminacao por vertice precisa de vertices!)
static void gridFloor(float x0, float z0, float x1, float z1, float y, int n, float u0, float v0, float u1, float v1) {
    glNormal3f(0, 1, 0);
    for (int i = 0; i < n; ++i) {
        float za = lerpf(z0, z1, i / (float)n), zb = lerpf(z0, z1, (i + 1) / (float)n);
        float va = lerpf(v0, v1, i / (float)n), vb = lerpf(v0, v1, (i + 1) / (float)n);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= n; ++j) {
            float x = lerpf(x0, x1, j / (float)n), u = lerpf(u0, u1, j / (float)n);
            glTexCoord2f(u, vb); glVertex3f(x, y, zb);
            glTexCoord2f(u, va); glVertex3f(x, y, za);
        }
        glEnd();
    }
}

// Quad generico (4 vertices em ordem anti-horaria vista pela frente)
static void quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float u0, float v0, float u1, float v1) {
    Vec3 n = normalize(cross(b - a, d - a));
    glBegin(GL_QUADS);
    glNormal3f(n.x, n.y, n.z);
    glTexCoord2f(u0, v0); glVertex3f(a.x, a.y, a.z);
    glTexCoord2f(u1, v0); glVertex3f(b.x, b.y, b.z);
    glTexCoord2f(u1, v1); glVertex3f(c.x, c.y, c.z);
    glTexCoord2f(u0, v1); glVertex3f(d.x, d.y, d.z);
    glEnd();
}

// Matriz que projeta a geometria no plano a partir da posicao da luz
static void shadowMatrix(GLfloat m[16], const GLfloat plane[4], const GLfloat light[4]) {
    float d = plane[0] * light[0] + plane[1] * light[1] + plane[2] * light[2] + plane[3] * light[3];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m[c * 4 + r] = (r == c ? d : 0.0f) - light[r] * plane[c];
}

// Cinematica inversa de 2 ossos (ombro-cotovelo-mao)
static Vec3 solveElbow(const Vec3& S, const Vec3& T, float a, float b, const Vec3& pole) {
    Vec3 d = T - S;
    float L = length(d);
    Vec3 dir = L > 1e-5f ? d / L : Vec3(0, 0, 1);
    L = clampf(L, std::fabs(a - b) + 0.01f, a + b - 0.001f);
    float cosA = clampf((a * a + L * L - b * b) / (2 * a * L), -1, 1);
    float sinA = std::sqrt(std::max(0.0f, 1 - cosA * cosA));
    Vec3 p = pole - S;
    Vec3 perp = normalize(p - dir * dot(p, dir));
    if (length(perp) < 0.5f) perp = normalize(cross(dir, Vec3(1, 0, 0)));
    return S + dir * (a * cosA) + perp * (a * sinA);
}

// ---------------------------------------------------------------------------
// LUVAS e MANOPLAS
// ---------------------------------------------------------------------------
static void glove(const Vec3& pos, const Vec3& dir, const Vec3& upHint, float side, const float* col,
                  float er, float eg, float eb) {
    Vec3 F = normalize(dir);
    Vec3 R = normalize(cross(F, upHint));
    if (length(R) < 0.5f) R = Vec3(1, 0, 0);
    Vec3 U = cross(R, F);
    glPushMatrix();
    multFrame(pos, R, U, F);
    tex(TEX_LEATHER);
    mat(col[0], col[1], col[2], 0.6f, 40, er, eg, eb);
    glPushMatrix(); glScalef(0.095f, 0.088f, 0.115f); sphere(1, 20, 14); glPopMatrix();
    glPushMatrix(); glTranslatef(side * 0.072f, 0.025f, -0.01f); glScalef(0.035f, 0.04f, 0.06f); sphere(1, 10, 8); glPopMatrix();
    notex();
    mat(0.92f, 0.92f, 0.92f, 0.1f, 8);   // fita/punho branco
    glPushMatrix(); glTranslatef(0, 0, -0.17f); gluCylinder(Q, 0.06f, 0.07f, 0.08f, 14, 1); glPopMatrix();
    glPopMatrix();
}

static void mittPad(const Vec3& pos, const Vec3& facing, bool lit) {
    Vec3 F = normalize(facing);
    Vec3 R = normalize(cross(F, Vec3(0, 1, 0)));
    Vec3 U = cross(R, F);
    glPushMatrix();
    multFrame(pos, R, U, F);
    float pulse = 0.5f + 0.5f * std::sin(gTime * 14);
    if (lit) mat(1.0f, 0.85f, 0.1f, 0.4f, 30, 0.6f + 0.3f * pulse, 0.45f + 0.2f * pulse, 0.0f);
    else mat(0.75f, 0.1f, 0.1f, 0.3f, 20);
    tex(TEX_LEATHER);
    glPushMatrix(); glScalef(0.13f, 0.13f, 0.045f); sphere(1, 18, 10); glPopMatrix();
    notex();
    mat(0.95f, 0.95f, 0.95f, 0.1f, 10, lit ? 0.4f : 0, lit ? 0.4f : 0, lit ? 0.4f : 0);
    glPushMatrix(); glTranslatef(0, 0, 0.035f); gluDisk(Q, 0.06f, 0.08f, 20, 1); glPopMatrix();
    glPopMatrix();
}

// ---------------------------------------------------------------------------
// LUTADOR (modelo hierarquico montado a partir das articulacoes)
// ---------------------------------------------------------------------------
enum DrawMode { DRAW_FULL, DRAW_ARMS };

static void telegraphColor(PunchType t, float& r, float& g, float& b) {
    switch (t) {
        case RETO:   r = 1.0f; g = 1.0f; b = 1.0f; break;
        case GANCHO: r = 1.0f; g = 0.8f; b = 0.1f; break;
        case UPPER:  r = 1.0f; g = 0.2f; b = 0.1f; break;
        case CORPO:  r = 0.2f; g = 0.8f; b = 1.0f; break;
        default:     r = g = b = 1; break;
    }
}

static void drawFighter(const Fighter& f, const Appearance& L, DrawMode mode, int litMitt = -1) {
    Vec3 R = f.right(), F = f.fwd(), U(0, 1, 0);
    float bulk = f.bulk;
    float dmg = 1.0f - clampf(f.hp / f.maxHp, 0, 1);
    // Pele fica avermelhada com o dano; pisca vermelho ao ser atingido
    float flash = f.hitFlash * 3.0f;
    float sk[3] = { lerpf(L.skin[0], 0.75f, dmg * 0.3f) + flash * 0.3f,
                    lerpf(L.skin[1], 0.35f, dmg * 0.35f),
                    lerpf(L.skin[2], 0.4f, dmg * 0.25f) };
    for (float& c : sk) c = clampf(c, 0, 1);

    Vec3 head = f.headPos();
    Vec3 hips = f.local(f.lean * 0.05f, 0.95f - f.duck * 0.15f, 0);
    if (f.downAnim > 0.001f) hips = lerp(hips, f.local(0, 0.2f, -0.35f), easeInOut(f.downAnim));
    Vec3 spineUp = normalize(head - hips);
    Vec3 bodyF = normalize(cross(spineUp, R));
    Vec3 bodyR = normalize(cross(bodyF, spineUp));

    if (mode == DRAW_FULL) {
        // ---- Pernas ----
        Vec3 footL = f.local(-0.17f * bulk, 0.0f, 0.12f), footR = f.local(0.17f * bulk, 0.0f, -0.1f);
        for (int s = 0; s < 2; ++s) {
            float sd = s == 0 ? -1.0f : 1.0f;
            Vec3 hip = hips + bodyR * (sd * 0.1f * bulk);
            Vec3 foot = s == 0 ? footL : footR;
            Vec3 knee = solveElbow(hip, foot + Vec3(0, 0.08f, 0), 0.47f, 0.47f, hip + F * 1.0f + Vec3(0, -0.3f, 0));
            mat(sk[0], sk[1], sk[2], 0.2f, 12);
            limb(hip, knee, 0.078f * bulk, 0.06f * bulk);
            glPushMatrix(); translate(knee); sphere(0.06f * bulk, 10, 8); glPopMatrix();
            limb(knee, foot + Vec3(0, 0.08f, 0), 0.06f * bulk, 0.045f);
            // tenis
            mat(0.1f, 0.1f, 0.12f, 0.3f, 20);
            glPushMatrix(); translate(foot + Vec3(0, 0.045f, 0) + F * 0.04f);
            multFrame(Vec3(), R, U, F); box(0.11f, 0.09f, 0.26f);
            glPopMatrix();
            // perna do calcao
            mat(L.shorts[0], L.shorts[1], L.shorts[2], 0.35f, 30);
            limb(hip, lerp(hip, knee, 0.45f), 0.1f * bulk, 0.095f * bulk);
        }
        // ---- Calcao e tronco ----
        glPushMatrix();
        multFrame(hips + spineUp * 0.03f, bodyR, spineUp, bodyF);
        mat(L.shorts[0], L.shorts[1], L.shorts[2], 0.35f, 30);
        glPushMatrix(); glScalef(0.21f * bulk, 0.16f, 0.15f * bulk); sphere(1); glPopMatrix();
        mat(0.95f, 0.95f, 0.95f, 0.2f, 10);   // elastico
        glPushMatrix(); glTranslatef(0, 0.1f, 0); glRotatef(-90, 1, 0, 0); gluCylinder(Q, 0.19f * bulk, 0.185f * bulk, 0.05f, 20, 1); glPopMatrix();
        mat(sk[0], sk[1], sk[2], 0.12f, 14);
        glPushMatrix(); glTranslatef(0, 0.2f, 0.0f); glScalef(0.18f * bulk, 0.15f, 0.12f * bulk); sphere(1); glPopMatrix();    // abdomen
        glPushMatrix(); glTranslatef(0, 0.36f, 0.0f); glScalef(0.23f * bulk, 0.2f, 0.14f * bulk); sphere(1); glPopMatrix();    // peito
        for (int s = -1; s <= 1; s += 2) {   // peitorais
            glPushMatrix(); glTranslatef(s * 0.085f * bulk, 0.38f, 0.07f * bulk); glScalef(0.09f, 0.07f, 0.06f); sphere(1, 12, 8); glPopMatrix();
        }
        glPopMatrix();
        // ---- Pescoco e cabeca ----
        mat(sk[0], sk[1], sk[2], 0.12f, 14);
        limb(hips + spineUp * 0.5f, head - spineUp * 0.1f, 0.085f * bulk, 0.07f);

        Vec3 hUp = normalize(spineUp - f.snap * 2.5f);
        Vec3 hF = normalize(cross(hUp, bodyR));
        Vec3 hR = normalize(cross(hF, hUp));
        glPushMatrix();
        multFrame(head, hR, hUp, hF);
        mat(sk[0], sk[1], sk[2], 0.3f, 18);
        glPushMatrix(); glScalef(0.125f, 0.15f, 0.135f); sphere(1, 20, 16); glPopMatrix();
        glPushMatrix(); glTranslatef(0, -0.075f, 0.015f); glScalef(0.09f, 0.06f, 0.09f); sphere(1, 14, 10); glPopMatrix();   // mandibula
        for (int s = -1; s <= 1; s += 2) { glPushMatrix(); glTranslatef(s * 0.125f, 0, -0.01f); glScalef(0.025f, 0.04f, 0.03f); sphere(1, 8, 6); glPopMatrix(); }
        glPushMatrix(); glTranslatef(0, -0.005f, 0.135f); glScalef(0.022f, 0.032f, 0.026f); sphere(1, 10, 8); glPopMatrix(); // nariz
        // olhos (o esquerdo incha com muito dano)
        for (int s = -1; s <= 1; s += 2) {
            bool swollen = (s == -1 && dmg > 0.6f);
            glPushMatrix(); glTranslatef(s * 0.045f, 0.025f, 0.112f);
            if (swollen) {
                mat(0.55f, 0.25f, 0.4f, 0.4f, 20);
                glPushMatrix(); glScalef(0.03f, 0.022f, 0.02f); sphere(1, 10, 8); glPopMatrix();
            } else {
                mat(0.95f, 0.95f, 0.95f, 0.6f, 50);
                glPushMatrix(); glScalef(0.022f, 0.016f, 0.012f); sphere(1, 10, 8); glPopMatrix();
                mat(0.05f, 0.04f, 0.03f, 0.8f, 60);
                glPushMatrix(); glTranslatef(0, 0, 0.011f); sphere(0.009f, 8, 6); glPopMatrix();
            }
            glPopMatrix();
            // sobrancelhas franzidas
            mat(L.hair[0] * 0.6f, L.hair[1] * 0.6f, L.hair[2] * 0.6f, 0.05f, 4);
            glPushMatrix(); glTranslatef(s * 0.047f, 0.058f, 0.122f); glRotatef(s * -14.0f, 0, 0, 1); box(0.045f, 0.011f, 0.012f); glPopMatrix();
        }
        mat(0.45f, 0.1f, 0.1f, 0.3f, 20);    // boca (protetor bucal)
        glPushMatrix(); glTranslatef(0, -0.06f, 0.118f); glScalef(0.04f, 0.011f, 0.012f); sphere(1, 10, 6); glPopMatrix();
        // cabelo / bone
        mat(L.hair[0], L.hair[1], L.hair[2], 0.25f, 20);
        switch (L.hairStyle) {
        case 1:
            glPushMatrix(); glTranslatef(0, 0.03f, -0.01f); glScalef(0.132f, 0.135f, 0.138f);
            { GLdouble eq[4] = { 0, 1, 0, -0.1 }; glClipPlane(GL_CLIP_PLANE0, eq); glEnable(GL_CLIP_PLANE0); sphere(1, 18, 12); glDisable(GL_CLIP_PLANE0); }
            glPopMatrix(); break;
        case 2:
            for (int i = 0; i < 6; ++i) {
                float a = (-40 + i * 22) * DEG2RAD;
                glPushMatrix(); glTranslatef(0, std::cos(a) * 0.15f, std::sin(a) * 0.14f - 0.01f);
                glRotatef(-a * RAD2DEG, 1, 0, 0); box(0.035f, 0.07f, 0.035f); glPopMatrix();
            }
            break;
        case 3:
            glPushMatrix(); glTranslatef(0, 0.03f, -0.01f); glScalef(0.133f, 0.137f, 0.14f);
            { GLdouble eq[4] = { 0, 1, 0, -0.05 }; glClipPlane(GL_CLIP_PLANE0, eq); glEnable(GL_CLIP_PLANE0); sphere(1, 18, 12); glDisable(GL_CLIP_PLANE0); }
            glPopMatrix();
            for (int i = 0; i < 4; ++i) { glPushMatrix(); glTranslatef(0, 0.06f - i * 0.05f, -0.15f - i * 0.025f); sphere(0.035f - i * 0.004f, 10, 8); glPopMatrix(); }
            break;
        case 4:
            glPushMatrix(); glTranslatef(0, 0.04f, 0);
            { GLdouble eq[4] = { 0, 1, 0, 0.0 }; glClipPlane(GL_CLIP_PLANE0, eq); glEnable(GL_CLIP_PLANE0); glScalef(0.135f, 0.12f, 0.14f); sphere(1, 18, 12); glDisable(GL_CLIP_PLANE0); }
            glPopMatrix();
            glPushMatrix(); glTranslatef(0, 0.045f, 0.15f); box(0.17f, 0.015f, 0.1f); glPopMatrix();
            break;
        default: break;
        }
        if (L.beard) {
            mat(L.hair[0], L.hair[1], L.hair[2], 0.1f, 6);
            glPushMatrix(); glTranslatef(0, -0.085f, 0.05f); glScalef(0.1f, 0.06f, 0.085f); sphere(1, 14, 10); glPopMatrix();
        }
        glPopMatrix();
    }

    // ---- Bracos (IK) e luvas ----
    for (int h = 0; h < 2; ++h) {
        float side = h == 0 ? -1.0f : 1.0f;
        const Hand& hd = f.hands[h];
        Vec3 S = f.shoulderPos(h);
        Vec3 G = hd.pos;
        Vec3 pole = S + R * (side * 0.35f) + U * -0.5f + F * -0.15f;
        Vec3 E = solveElbow(S, G, 0.31f * std::sqrt(bulk), 0.30f, pole);
        Vec3 wrist = G - normalize(G - E) * 0.13f;
        mat(sk[0], sk[1], sk[2], 0.12f, 14);
        if (mode == DRAW_FULL) { glPushMatrix(); translate(S); sphere(0.078f * bulk, 12, 10); glPopMatrix(); }
        limb(S, E, 0.062f * bulk, 0.05f * bulk);
        glPushMatrix(); translate(E); sphere(0.05f * bulk, 10, 8); glPopMatrix();
        limb(E, wrist, 0.05f * bulk, 0.042f);

        if (f.useMitts && L.mitts) {
            mittPad(G, -F, h == litMitt);
            continue;
        }
        // Brilho de "telegrafo" durante o preparo + brilho vermelho da furia
        float er = 0, eg = 0, eb = 0;
        if (hd.phase == HAND_WINDUP && !hd.feint) {
            float tr, tg, tb; telegraphColor(hd.type, tr, tg, tb);
            float k = (0.25f + 0.45f * hd.t) * (0.7f + 0.3f * std::sin(gTime * 40));
            er = tr * k; eg = tg * k; eb = tb * k;
        } else if (hd.phase == HAND_WINDUP && hd.feint) {
            er = eg = eb = 0.25f;   // finta tambem brilha (para enganar!)
        }
        if (f.furyTimer > 0) { er += 0.45f; eg += 0.05f; }
        glove(G, G - E, spineUp, side, L.gloves, er, eg, eb);
    }
}

// ---------------------------------------------------------------------------
// SACO DE PANCADAS
// ---------------------------------------------------------------------------
static void drawBag(const Bag& b) {
    Vec3 c = b.center(), ax = b.axis();
    Vec3 top = c + ax * 0.55f, bot = c - ax * 0.55f;
    mat(0.6f, 0.6f, 0.65f, 0.9f, 80);
    limb(b.anchor, top + ax * 0.12f, 0.012f, 0.012f, 6);   // corrente
    for (int s = -1; s <= 1; s += 2) limb(top + ax * 0.12f, top + Vec3((float)s * 0.18f, 0, 0), 0.008f, 0.008f, 5);
    tex(TEX_BAG);
    mat(0.55f, 0.12f, 0.1f, 0.35f, 25);
    limb(bot, top, 0.26f, 0.26f, 22);
    notex();
    mat(0.45f, 0.1f, 0.08f, 0.35f, 25);
    glPushMatrix(); translate(top); glScalef(0.26f, 0.06f, 0.26f); sphere(1, 18, 8); glPopMatrix();
    glPushMatrix(); translate(bot); glScalef(0.26f, 0.08f, 0.26f); sphere(1, 18, 8); glPopMatrix();
}

// ---------------------------------------------------------------------------
// ARENA / ACADEMIA
// ---------------------------------------------------------------------------
static const float RX0 = -3.0f, RX1 = 3.0f, RZ0 = -3.6f, RZ1 = 2.4f;
static const float RCX = 0.0f, RCZ = -0.6f;

static void drawRing(bool writeStencil) {
    // Lona: escreve 1 no stencil (so ali as sombras serao desenhadas)
    if (writeStencil) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }
    tex(TEX_CANVAS);
    mat(1, 1, 1, 0.08f, 6);
    gridFloor(RX0, RZ0, RX1, RZ1, 0.0f, 28, 0, 1, 1, 0);   // v invertido: logo legivel do lado do jogador
    if (writeStencil) glDisable(GL_STENCIL_TEST);

    // Saia do ringue
    tex(TEX_APRON);
    mat(1, 1, 1, 0.1f, 8);
    quad(Vec3(RX0, -1, RZ1), Vec3(RX1, -1, RZ1), Vec3(RX1, 0, RZ1), Vec3(RX0, 0, RZ1), 0, 0, 1.2f, 1);
    quad(Vec3(RX1, -1, RZ0), Vec3(RX0, -1, RZ0), Vec3(RX0, 0, RZ0), Vec3(RX1, 0, RZ0), 0, 0, 1.2f, 1);
    quad(Vec3(RX1, -1, RZ1), Vec3(RX1, -1, RZ0), Vec3(RX1, 0, RZ0), Vec3(RX1, 0, RZ1), 0, 0, 1.2f, 1);
    quad(Vec3(RX0, -1, RZ0), Vec3(RX0, -1, RZ1), Vec3(RX0, 0, RZ1), Vec3(RX0, 0, RZ0), 0, 0, 1.2f, 1);
    notex();

    // Postes (corner vermelho, azul e dois neutros)
    const float px = 2.85f, pz0 = -3.45f, pz1 = 2.25f;
    Vec3 posts[4] = { Vec3(-px, 0, pz0), Vec3(px, 0, pz0), Vec3(px, 0, pz1), Vec3(-px, 0, pz1) };
    float pc[4][3] = { { 0.8f, 0.1f, 0.1f }, { 0.95f, 0.95f, 0.95f }, { 0.1f, 0.2f, 0.8f }, { 0.95f, 0.95f, 0.95f } };
    for (int i = 0; i < 4; ++i) {
        mat(0.7f, 0.7f, 0.75f, 0.9f, 90);
        glPushMatrix(); translate(posts[i]); glRotatef(-90, 1, 0, 0); gluCylinder(Q, 0.07f, 0.07f, 1.45f, 14, 1); glPopMatrix();
        mat(pc[i][0], pc[i][1], pc[i][2], 0.3f, 25);
        glPushMatrix(); translate(posts[i] + Vec3(0, 0.85f, 0)); box(0.22f, 0.95f, 0.22f); glPopMatrix();
    }
    // Cordas (3 niveis) com leve curvatura
    float ropeY[3] = { 0.45f, 0.85f, 1.25f };
    float rc[3][3] = { { 0.15f, 0.25f, 0.85f }, { 0.95f, 0.95f, 0.95f }, { 0.85f, 0.12f, 0.12f } };
    for (int l = 0; l < 3; ++l) {
        mat(rc[l][0], rc[l][1], rc[l][2], 0.5f, 40);
        for (int i = 0; i < 4; ++i) {
            Vec3 a = posts[i] + Vec3(0, ropeY[l], 0), b = posts[(i + 1) % 4] + Vec3(0, ropeY[l], 0);
            Vec3 m = (a + b) * 0.5f + Vec3(0, -0.04f, 0);
            limb(a, m, 0.022f, 0.022f, 8);
            limb(m, b, 0.022f, 0.022f, 8);
        }
    }
}

// Arquibancada com descarte por frustum. Retorna quantas secoes desenhou.
static void drawCrowd(const Game& g, int& drawn, int& total) {
    drawn = total = 0;
    Vec3 eye = g.cam.eye;
    Vec3 fwd = normalize(g.cam.target - g.cam.eye);
    float aspect = g.winW / (float)std::max(1, g.winH);
    float tv = std::tan(g.cam.fov * 0.5f * DEG2RAD);
    float halfDiag = std::atan(tv * std::sqrt(1 + aspect * aspect));

    tex(TEX_CROWD);
    const int segs = 4;
    const float half = 8.0f;
    struct Tier { float d0, d1, y0, y1; };
    Tier tiers[2] = { { 6.5f, 11.0f, -0.9f, 2.6f }, { 11.4f, 16.0f, 3.0f, 7.6f } };
    int idx = 0;
    for (int side = 0; side < 4; ++side) {
        // direcao para fora (n) e tangente (t) do lado
        Vec3 n = side == 0 ? Vec3(0, 0, 1) : side == 1 ? Vec3(0, 0, -1) : side == 2 ? Vec3(1, 0, 0) : Vec3(-1, 0, 0);
        Vec3 t = cross(Vec3(0, 1, 0), n);
        Vec3 C(RCX, 0, RCZ);
        for (int ti = 0; ti < 2; ++ti) {
            const Tier& T = tiers[ti];
            for (int s = 0; s < segs; ++s, ++idx) {
                ++total;
                float a = -half + s * (2 * half / segs), b = a + 2 * half / segs;
                Vec3 p0 = C + n * T.d0 + t * a + Vec3(0, T.y0, 0);
                Vec3 p1 = C + n * T.d0 + t * b + Vec3(0, T.y0, 0);
                Vec3 p2 = C + n * T.d1 + t * b + Vec3(0, T.y1, 0);
                Vec3 p3 = C + n * T.d1 + t * a + Vec3(0, T.y1, 0);
                // --- Teste de visibilidade (cone do frustum x esfera envolvente) ---
                Vec3 center = (p0 + p1 + p2 + p3) * 0.25f;
                float radius = length(p2 - p0) * 0.5f;
                Vec3 toC = center - eye;
                float dist = length(toC);
                if (dist > radius) {
                    float ang = std::acos(clampf(dot(toC / dist, fwd), -1, 1));
                    float angR = std::asin(clampf(radius / dist, 0, 1));
                    if (ang - angR > halfDiag) continue;   // fora do campo de visao: nao desenha
                }
                ++drawn;
                float bob = std::sin(gTime * 9.0f + idx * 1.7f) * 0.06f * g.crowdHype;
                float k = 0.75f + 0.25f * g.crowdHype;
                mat(k, k, k, 0.0f, 1);
                glPushMatrix(); glTranslatef(0, bob, 0);
                quad(p0, p1, p2, p3, s * 0.4f, 0, s * 0.4f + 0.4f, 2.0f);   // ordem: aponta para o ringue
                glPopMatrix();
            }
        }
    }
    notex();
    // Faixas no alto da arquibancada
    tex(TEX_BANNER);
    mat(1, 1, 1, 0, 1, 0.25f, 0.2f, 0.05f);
    glDisable(GL_CULL_FACE);
    quad(Vec3(-3, 8.2f, RCZ - 16.2f), Vec3(3, 8.2f, RCZ - 16.2f), Vec3(3, 9.7f, RCZ - 16.2f), Vec3(-3, 9.7f, RCZ - 16.2f), 0, 0, 1, 1);
    quad(Vec3(3, 8.2f, RCZ + 16.2f), Vec3(-3, 8.2f, RCZ + 16.2f), Vec3(-3, 9.7f, RCZ + 16.2f), Vec3(3, 9.7f, RCZ + 16.2f), 0, 0, 1, 1);
    notex();
}

static void drawTruss(const Game& g, bool gym) {
    const float y = gym ? 4.6f : 5.4f;
    mat(0.25f, 0.25f, 0.28f, 0.6f, 50);
    float x0 = -2.6f, x1 = 2.6f, z0 = -3.2f, z1 = 2.0f;
    glPushMatrix(); glTranslatef(0, y, z0); box(x1 - x0, 0.15f, 0.15f); glPopMatrix();
    glPushMatrix(); glTranslatef(0, y, z1); box(x1 - x0, 0.15f, 0.15f); glPopMatrix();
    glPushMatrix(); glTranslatef(x0, y, (z0 + z1) / 2); box(0.15f, 0.15f, z1 - z0); glPopMatrix();
    glPushMatrix(); glTranslatef(x1, y, (z0 + z1) / 2); box(0.15f, 0.15f, z1 - z0); glPopMatrix();
    for (int i = 0; i < 4; ++i) {   // cabos ate o teto
        float x = (i % 2) ? x1 : x0, z = (i / 2) ? z1 : z0;
        limb(Vec3(x, y, z), Vec3(x, 12, z), 0.01f, 0.01f, 4);
    }
    // Refletores (lente emissiva)
    for (int i = 0; i < 4; ++i) {
        float x = (i % 2) ? x1 - 0.5f : x0 + 0.5f, z = (i / 2) ? z1 - 0.4f : z0 + 0.4f;
        mat(0.15f, 0.15f, 0.15f, 0.5f, 40);
        glPushMatrix(); glTranslatef(x, y - 0.1f, z); glRotatef(90, 1, 0, 0); gluCylinder(Q, 0.18f, 0.22f, 0.3f, 16, 1);
        mat(1, 1, 0.9f, 0, 1, 1.0f, 0.95f, 0.8f);
        glTranslatef(0, 0, 0.28f); gluDisk(Q, 0, 0.2f, 16, 1);
        glPopMatrix();
    }
    (void)g;
}

// Cones de luz "volumetricos" falsos (mistura aditiva, sem escrever no Z)
static void drawLightCones(bool gym) {
    const float y = gym ? 4.3f : 5.1f;
    glDisable(GL_LIGHTING);
    notex();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < 4; ++i) {
        float x = (i % 2) ? 2.1f : -2.1f, z = (i / 2) ? 1.6f : -2.8f;
        Vec3 apex(x, y, z), base(x * 0.35f, 0.0f, RCZ + (z - RCZ) * 0.35f);
        glBegin(GL_TRIANGLE_FAN);
        glColor4f(1.0f, 0.95f, 0.8f, gym ? 0.05f : 0.09f);
        glVertex3f(apex.x, apex.y, apex.z);
        for (int k = 0; k <= 24; ++k) {
            float a = k / 24.0f * 2 * PI;
            glColor4f(1.0f, 0.95f, 0.8f, 0.0f);
            glVertex3f(base.x + std::cos(a) * 1.6f, base.y, base.z + std::sin(a) * 1.6f);
        }
        glEnd();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void drawGymProps() {
    // Paredes de concreto
    tex(TEX_CONCRETE);
    mat(0.9f, 0.88f, 0.85f, 0, 1);
    float d = 7.0f, y0 = -1.0f, y1 = 5.5f;
    quad(Vec3(-d, y0, RCZ - d), Vec3(d, y0, RCZ - d), Vec3(d, y1, RCZ - d), Vec3(-d, y1, RCZ - d), 0, 0, 4, 2);
    quad(Vec3(d, y0, RCZ + d), Vec3(-d, y0, RCZ + d), Vec3(-d, y1, RCZ + d), Vec3(d, y1, RCZ + d), 0, 0, 4, 2);
    quad(Vec3(-d, y0, RCZ + d), Vec3(-d, y0, RCZ - d), Vec3(-d, y1, RCZ - d), Vec3(-d, y1, RCZ + d), 0, 0, 4, 2);
    quad(Vec3(d, y0, RCZ - d), Vec3(d, y0, RCZ + d), Vec3(d, y1, RCZ + d), Vec3(d, y1, RCZ - d), 0, 0, 4, 2);
    notex();
    // Janelas (emissivas)
    for (int i = 0; i < 3; ++i) {
        mat(0.6f, 0.75f, 0.9f, 0, 1, 0.5f, 0.6f, 0.75f);
        float x = -4 + i * 4.0f;
        quad(Vec3(x - 1, 2.5f, RCZ - d + 0.02f), Vec3(x + 1, 2.5f, RCZ - d + 0.02f), Vec3(x + 1, 4.2f, RCZ - d + 0.02f), Vec3(x - 1, 4.2f, RCZ - d + 0.02f), 0, 0, 1, 1);
    }
    // Faixa na parede
    tex(TEX_BANNER);
    mat(1, 1, 1, 0, 1, 0.2f, 0.15f, 0.05f);
    quad(Vec3(-d + 0.02f, 2.4f, RCZ + 2), Vec3(-d + 0.02f, 2.4f, RCZ - 2), Vec3(-d + 0.02f, 3.4f, RCZ - 2), Vec3(-d + 0.02f, 3.4f, RCZ + 2), 0, 0, 1, 1);
    notex();
    // Supino com barra e anilhas
    mat(0.15f, 0.15f, 0.18f, 0.3f, 20);
    glPushMatrix(); glTranslatef(4.6f, -0.6f, -3.5f); box(0.5f, 0.15f, 1.6f); glPopMatrix();
    mat(0.4f, 0.4f, 0.42f, 0.8f, 80);
    limb(Vec3(3.7f, -0.05f, -3.0f), Vec3(5.5f, -0.05f, -3.0f), 0.02f, 0.02f, 8);
    mat(0.1f, 0.1f, 0.1f, 0.4f, 30);
    for (int s = 0; s < 2; ++s) {
        glPushMatrix(); glTranslatef(s ? 5.3f : 3.9f, -0.05f, -3.0f); glRotatef(90, 0, 1, 0);
        gluCylinder(Q, 0.22f, 0.22f, 0.06f, 20, 1); gluDisk(Q, 0.02f, 0.22f, 20, 1); glPopMatrix();
    }
    // Saco decorativo
    Bag deco; deco.anchor = Vec3(-4.5f, 4.0f, -3.5f); deco.L = 3.6f;
    drawBag(deco);
}

// ---------------------------------------------------------------------------
// RUA (treino de corrida)
// ---------------------------------------------------------------------------
static float h1(int i) { unsigned u = (unsigned)i * 2654435761u; u ^= u >> 13; return (u & 1023) / 1023.0f; }

static void drawRunWorld(const Game& g, bool shadowOnly) {
    float zNear = -g.runPos + 6, zFar = -g.runPos - 95;
    // postes
    for (int i = (int)(-zNear / 12) - 1; i < (int)(-zFar / 12) + 1; ++i) {
        float z = -i * 12.0f;
        for (int s = -1; s <= 1; s += 2) {
            mat(0.3f, 0.3f, 0.32f, 0.6f, 50);
            limb(Vec3(s * 3.7f, 0, z), Vec3(s * 3.7f, 4.5f, z), 0.07f, 0.05f, 8);
            limb(Vec3(s * 3.7f, 4.5f, z), Vec3(s * 2.9f, 4.6f, z), 0.04f, 0.04f, 6);
            if (!shadowOnly) mat(1, 1, 0.8f, 0, 1, 0.8f, 0.8f, 0.6f);
            glPushMatrix(); glTranslatef(s * 2.9f, 4.5f, z); glScalef(0.25f, 0.08f, 0.15f); sphere(1, 10, 6); glPopMatrix();
        }
    }
    // arvores
    for (int i = (int)(-zNear / 8) - 1; i < (int)(-zFar / 8) + 1; ++i) {
        float z = -i * 8.0f - 4.0f;
        for (int s = -1; s <= 1; s += 2) {
            float x = s * (6.0f + h1(i * 2 + (s > 0)) * 5.0f);
            float hgt = 2.5f + h1(i * 7 + (s > 0)) * 2.0f;
            mat(0.35f, 0.22f, 0.1f, 0.1f, 8);
            limb(Vec3(x, 0, z), Vec3(x, hgt * 0.5f, z), 0.15f, 0.12f, 8);
            mat(0.15f, 0.45f + h1(i) * 0.2f, 0.15f, 0.1f, 8);
            glPushMatrix(); glTranslatef(x, hgt * 0.4f, z); glRotatef(-90, 1, 0, 0); glutSolidCone(1.3f, hgt, 14, 4); glPopMatrix();
        }
    }
    // casas ao longe
    for (int i = (int)(-zNear / 15) - 1; i < (int)(-zFar / 15) + 1; ++i) {
        float z = -i * 15.0f;
        for (int s = -1; s <= 1; s += 2) {
            float w = 6 + h1(i * 3 + s) * 4, hh = 4 + h1(i * 5 + s) * 6;
            mat(0.6f + h1(i * 11 + s) * 0.35f, 0.5f + h1(i * 13 + s) * 0.3f, 0.45f + h1(i * 17 + s) * 0.3f, 0.05f, 4);
            glPushMatrix(); glTranslatef(s * 20.0f, hh / 2, z); box(w, hh, 10); glPopMatrix();
        }
    }
}

static void drawRunGround(const Game& g) {
    float zNear = -g.runPos + 6, zFar = -g.runPos - 95;
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    tex(TEX_ROAD);
    mat(1, 1, 1, 0.05f, 4);
    gridFloor(-3, zFar, 3, zNear, 0, 20, 0, zFar / 6.0f, 1, zNear / 6.0f);
    tex(TEX_GRASS);
    gridFloor(-40, zFar, -3, zNear, 0, 10, 0, zFar / 4.0f, 9, zNear / 4.0f);
    gridFloor(3, zFar, 40, zNear, 0, 10, 0, zFar / 4.0f, 9, zNear / 4.0f);
    notex();
    glDisable(GL_STENCIL_TEST);
}

// ---------------------------------------------------------------------------
// Sombras planares
// ---------------------------------------------------------------------------
static void beginShadow(const GLfloat light[4], float alpha) {
    GLfloat plane[4] = { 0, 1, 0, -0.004f };
    GLfloat m[16];
    shadowMatrix(m, plane, light);
    glDisable(GL_LIGHTING);
    notex();
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xFF);          // so sobre o chao marcado
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);     // cada pixel escurece uma unica vez
    glDepthMask(GL_FALSE);
    glColor4f(0, 0, 0, alpha);
    gShadow = true;
    glPushMatrix();
    glMultMatrixf(m);
}
static void endShadow(bool lighting) {
    glPopMatrix();
    gShadow = false;
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    if (lighting) glEnable(GL_LIGHTING);
}

// ---------------------------------------------------------------------------
// Luzes
// ---------------------------------------------------------------------------
static void setLight(GLenum L, const Vec3& pos, float w, float r, float g, float b, float spec) {
    GLfloat p[4] = { pos.x, pos.y, pos.z, w };
    GLfloat d[4] = { r, g, b, 1 }, s[4] = { spec, spec, spec, 1 }, a[4] = { 0, 0, 0, 1 };
    glLightfv(L, GL_POSITION, p);
    glLightfv(L, GL_DIFFUSE, d);
    glLightfv(L, GL_SPECULAR, s);
    glLightfv(L, GL_AMBIENT, a);
    glEnable(L);
}

static Vec3 setupLights(const Game& g, int sceneKind) {
    glEnable(GL_LIGHTING);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    Vec3 mainPos;
    for (int i = 0; i < 4; ++i) glDisable(GL_LIGHT0 + i);
    if (sceneKind == 2) {   // rua: sol
        GLfloat amb[4] = { 0.38f, 0.38f, 0.42f, 1 };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
        mainPos = Vec3(0.35f, 1.0f, 0.25f);
        setLight(GL_LIGHT0, mainPos, 0, 0.95f, 0.9f, 0.8f, 0.5f);
        glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 180);
        return mainPos;
    }
    bool gym = sceneKind == 1;
    GLfloat amb[4] = { gym ? 0.24f : 0.13f, gym ? 0.23f : 0.13f, gym ? 0.22f : 0.17f, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    // LIGHT0: holofote principal sobre o ringue
    mainPos = Vec3(0.3f, 6.5f, RCZ + 0.6f);
    setLight(GL_LIGHT0, mainPos, 1, gym ? 0.85f : 1.05f, gym ? 0.8f : 1.0f, gym ? 0.65f : 0.9f, 1.0f);
    GLfloat dir[3] = { -0.05f, -1, -0.1f };
    glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, dir);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, gym ? 60.0f : 42.0f);
    glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, gym ? 2.0f : 5.0f);
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.015f);
    // LIGHT1: preenchimento direcional azulado
    setLight(GL_LIGHT1, Vec3(-0.4f, 0.7f, 0.6f), 0, 0.30f, 0.28f, 0.32f, 0.1f);
    glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 180);
    // LIGHT2: contra-luz quente vinda do fundo (destaca a silhueta do adversario)
    setLight(GL_LIGHT2, Vec3(0, 3.0f, -6.0f), 1, 0.35f, 0.25f, 0.15f, 0.4f);
    glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, 180);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.02f);
    // LIGHT3: flash de camera na plateia
    if (g.flashTimer > 0 && !gym) {
        float k = g.flashTimer / 0.12f * 1.5f;
        setLight(GL_LIGHT3, g.flashPos, 1, k, k, k, k);
        glLightf(GL_LIGHT3, GL_SPOT_CUTOFF, 180);
        glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, 0.01f);
    }
    return mainPos;
}

// ---------------------------------------------------------------------------
// RENDER PRINCIPAL
// ---------------------------------------------------------------------------
void render(const Game& g) {
    gTime = g.gameTime;
    bool run = (g.state == GS_TRAIN && g.train == TR_RUN);
    bool gym = (g.state == GS_HUB || g.state == GS_TRAIN || g.state == GS_TRAIN_RESULT) && !run;
    int kind = run ? 2 : (gym ? 1 : 0);
    bool firstPerson = (g.state == GS_FIGHT || g.state == GS_TRAIN);

    float sky[3] = { 0.03f, 0.03f, 0.05f };
    if (gym) { sky[0] = 0.08f; sky[1] = 0.08f; sky[2] = 0.09f; }
    if (run) { sky[0] = 0.55f; sky[1] = 0.74f; sky[2] = 0.95f; }
    glClearColor(sky[0], sky[1], sky[2], 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glViewport(0, 0, g.winW, g.winH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(g.cam.fov, g.winW / (double)std::max(1, g.winH), firstPerson ? 0.03 : 0.1, 150.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(g.cam.eye.x, g.cam.eye.y, g.cam.eye.z, g.cam.target.x, g.cam.target.y, g.cam.target.z,
              g.cam.up.x, g.cam.up.y, g.cam.up.z);
    glGetDoublev(GL_MODELVIEW_MATRIX, MV);
    glGetDoublev(GL_PROJECTION_MATRIX, PJ);
    glGetIntegerv(GL_VIEWPORT, VP);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_NORMALIZE);
    glShadeModel(g.smooth ? GL_SMOOTH : GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK, g.wire ? GL_LINE : GL_FILL);
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);

    // Neblina (atmosfera da arena, profundidade da rua)
    if (g.fog) {
        glEnable(GL_FOG);
        GLfloat fc[4] = { sky[0], sky[1], sky[2], 1 };
        glFogfv(GL_FOG_COLOR, fc);
        if (run) { glFogi(GL_FOG_MODE, GL_LINEAR); glFogf(GL_FOG_START, 25); glFogf(GL_FOG_END, 95); }
        else { glFogi(GL_FOG_MODE, GL_EXP2); glFogf(GL_FOG_DENSITY, gym ? 0.03f : 0.045f); }
    } else glDisable(GL_FOG);

    Vec3 mainLight = setupLights(g, kind);
    if (!g.lighting) glDisable(GL_LIGHTING);

    // ---------------- Cenario ----------------
    if (run) {
        drawRunGround(g);
        drawRunWorld(g, false);
        if (g.shadows) {
            GLfloat L[4] = { mainLight.x, mainLight.y, mainLight.z, 0 };   // luz direcional (sol)
            beginShadow(L, 0.35f);
            drawRunWorld(g, true);
            endShadow(g.lighting);
        }
    } else {
        tex(gym ? TEX_WOOD : TEX_CONCRETE);
        mat(gym ? 0.9f : 0.6f, gym ? 0.9f : 0.6f, gym ? 0.9f : 0.65f, 0.05f, 4);
        gridFloor(-20, -20.6f, 20, 19.4f, -1.0f, 30, 0, 0, 12, 12);
        notex();
        drawRing(true);
        if (gym) drawGymProps();
        else {
            int d = 0, t = 0;
            drawCrowd(g, d, t);
            const_cast<Game&>(g).crowdDrawn = d;   // estatistica para o HUD de depuracao
            const_cast<Game&>(g).crowdTotal = t;
        }
        drawTruss(g, gym);

        // ---------------- Sombras ----------------
        bool showOpp = !(g.state == GS_TRAIN && g.train == TR_BAG) && g.state != GS_HUB && g.state != GS_TRAIN_RESULT;
        bool showPlayer = firstPerson;
        if (g.shadows) {
            GLfloat L[4] = { mainLight.x, mainLight.y, mainLight.z, 1 };
            beginShadow(L, 0.5f);
            if (showOpp) drawFighter(g.opp, g.oppLook(), DRAW_FULL, g.litMittHand());
            if (showPlayer) drawFighter(g.player, Game::playerLook(), DRAW_FULL);
            if (g.state == GS_TRAIN && g.train == TR_BAG) drawBag(g.bag);
            endShadow(g.lighting);
        }

        // ---------------- Personagens ----------------
        glEnable(GL_CULL_FACE);
        if (showOpp) drawFighter(g.opp, g.oppLook(), DRAW_FULL, g.litMittHand());
        if (g.state == GS_TRAIN && g.train == TR_BAG) drawBag(g.bag);
        glDisable(GL_CULL_FACE);
        if (showPlayer) {
            // Primeira pessoa: so os bracos e luvas do jogador
            drawFighter(g.player, Game::playerLook(), DRAW_ARMS);
        }
        if (g.cones && g.lighting) drawLightCones(gym);
    }

    // ---------------- Particulas (suor) ----------------
    if (!g.particles.empty()) {
        glDisable(GL_LIGHTING); notex();
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(4.0f);
        glBegin(GL_POINTS);
        for (const Particle& p : g.particles) {
            glColor4f(p.r, p.g, p.b, clampf(p.life / p.maxLife, 0, 1) * 0.8f);
            glVertex3f(p.p.x, p.p.y, p.p.z);
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_FOG);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
}

} // namespace Scene
