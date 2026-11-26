// ============================================================================
//  Textures.cpp - Geracao procedural das texturas
// ============================================================================
#include "Textures.h"
#include "Common.h"
#include <vector>
#include <cstring>
#include <cstdint>

namespace Textures {

bool enabled = true;
static GLuint ids[NUM_TEX] = {};

// ---------------------------------------------------------------------------
// Imagem RGB simples
// ---------------------------------------------------------------------------
struct Image {
    int w, h;
    std::vector<unsigned char> px;
    Image(int W, int H) : w(W), h(H), px(W * H * 3, 0) {}
    void set(int x, int y, float r, float g, float b) {
        x = ((x % w) + w) % w; y = ((y % h) + h) % h;  // repete (wrap)
        unsigned char* p = &px[(y * w + x) * 3];
        p[0] = (unsigned char)clampf(r, 0, 255);
        p[1] = (unsigned char)clampf(g, 0, 255);
        p[2] = (unsigned char)clampf(b, 0, 255);
    }
    void get(int x, int y, float& r, float& g, float& b) const {
        x = ((x % w) + w) % w; y = ((y % h) + h) % h;
        const unsigned char* p = &px[(y * w + x) * 3];
        r = p[0]; g = p[1]; b = p[2];
    }
    void blend(int x, int y, float r, float g, float b, float a) {
        float R, G, B; get(x, y, R, G, B);
        set(x, y, lerpf(R, r, a), lerpf(G, g, a), lerpf(B, b, a));
    }
    void disc(float cx, float cy, float rad, float r, float g, float b, float a = 1) {
        for (int y = (int)(cy - rad - 1); y <= (int)(cy + rad + 1); ++y)
            for (int x = (int)(cx - rad - 1); x <= (int)(cx + rad + 1); ++x) {
                float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
                float cov = clampf(rad - d + 0.5f, 0, 1);   // borda suavizada
                if (cov > 0) blend(x, y, r, g, b, cov * a);
            }
    }
    void ellipse(float cx, float cy, float rx, float ry, float r, float g, float b) {
        for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); ++y)
            for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); ++x) {
                float dx = (x - cx) / rx, dy = (y - cy) / ry;
                if (dx * dx + dy * dy <= 1.0f) set(x, y, r, g, b);
            }
    }
};

// Ruido de valor 2D (hash + interpolacao) para variacoes organicas
static float hash2(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}
static float valueNoise(float x, float y, int seed, int period) {
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;
    auto H = [&](int a, int b) { return hash2(((a % period) + period) % period, ((b % period) + period) % period, seed); };
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float a = lerpf(H(x0, y0), H(x0 + 1, y0), sx);
    float b = lerpf(H(x0, y0 + 1), H(x0 + 1, y0 + 1), sx);
    return lerpf(a, b, sy);
}
// Ruido fractal (varias oitavas), periodico para a textura repetir sem costura
static float fbm(float x, float y, int seed, int basePeriod, int oct = 4) {
    float v = 0, amp = 0.5f, f = 1;
    for (int i = 0; i < oct; ++i) {
        v += amp * valueNoise(x * f, y * f, seed + i, basePeriod * (int)f);
        amp *= 0.5f; f *= 2;
    }
    return v;
}

// ---------------------------------------------------------------------------
// Fonte bitmap 5x7 para escrever textos DENTRO das texturas
// ---------------------------------------------------------------------------
static const char* glyph(char c) {
    switch (c) {
    case 'A': return "01110100011000111111100011000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01110100011000010000100001000101110";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01110100011000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "01110001000010000100001000010001110";
    case 'J': return "00111000100001000010000101001001100";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001100011100110101100111000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'Q': return "01110100011000110001101011001001101";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101000100";
    case 'W': return "10001100011000110101101011010101010";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case 'Z': return "11111000010001000100010001000011111";
    case '0': return "01110100011001110101110011000101110";
    case '1': return "00100011000010000100001000010001110";
    case '2': return "01110100010000100010001000100011111";
    case '3': return "11111000100010000010000011000101110";
    case '4': return "00010001100101010010111110001000010";
    case '5': return "11111100001111000001000011000101110";
    case '6': return "00110010001000011110100011000101110";
    case '7': return "11111000010001000100010000100001000";
    case '8': return "01110100011000101110100011000101110";
    case '9': return "01110100011000101111000010001001100";
    case '-': return "00000000000000011111000000000000000";
    case '!': return "00100001000010000100001000000000100";
    default:  return nullptr;
    }
}
// Escreve texto com pixels de tamanho 'sc'. (y cresce para cima na textura GL)
static void drawText(Image& img, int x, int y, int sc, const char* s, float r, float g, float b) {
    for (; *s; ++s) {
        const char* gl = glyph(*s);
        if (gl) {
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (gl[row * 5 + col] == '1')
                        for (int yy = 0; yy < sc; ++yy)
                            for (int xx = 0; xx < sc; ++xx)
                                img.set(x + col * sc + xx, y + (6 - row) * sc + yy, r, g, b);
        }
        x += 6 * sc;
    }
}
static int textWidth(const char* s, int sc) { return (int)std::strlen(s) * 6 * sc - sc; }

// ---------------------------------------------------------------------------
static GLuint upload(const Image& img) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, img.w, img.h, GL_RGB, GL_UNSIGNED_BYTE, img.px.data());
    return id;
}

// Lona do ringue com logotipo central
static Image makeCanvas() {
    const int N = 512;
    Image img(N, N);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float weave = ((x / 2 + y / 2) % 2) ? 1.0f : 0.93f;              // trama do tecido
            float n = fbm(x / 64.0f, y / 64.0f, 7, 8);                        // manchas de uso
            float k = weave * (0.85f + 0.3f * n);
            img.set(x, y, 40 * k, 78 * k, 150 * k);
        }
    // Faixa branca na borda
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x)
            if (x < 10 || y < 10 || x >= N - 10 || y >= N - 10) img.set(x, y, 225, 225, 230);
    // Logo central: anel + texto
    float c = N / 2.0f;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float d = std::sqrt((x - c) * (x - c) + (y - c) * (y - c));
            if (d < 150) img.blend(x, y, 20, 30, 60, 0.45f);
            if (d > 150 && d < 162) img.set(x, y, 235, 235, 240);
            if (d > 168 && d < 172) img.set(x, y, 200, 40, 40);
        }
    drawText(img, (int)c - textWidth("PUNCH", 7) / 2, (int)c + 12, 7, "PUNCH", 240, 240, 245);
    drawText(img, (int)c - textWidth("CLUB 3D", 5) / 2, (int)c - 50, 5, "CLUB 3D", 230, 60, 50);
    // Manchas de suor/sangue leves
    for (int i = 0; i < 25; ++i) img.disc(randf(40, N - 40), randf(40, N - 40), randf(3, 9), 90, 30, 40, 0.18f);
    return img;
}

// Saia do ringue (lateral da plataforma)
static Image makeApron() {
    Image img(512, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 512; ++x) {
            float g = 0.75f + 0.25f * (y / 128.0f);
            float n = 0.9f + 0.1f * fbm(x / 32.0f, y / 32.0f, 3, 16);
            img.set(x, y, 150 * g * n, 18 * g * n, 22 * g * n);
            if (y < 10 || y > 117) img.set(x, y, 230, 200, 60);
        }
    drawText(img, 256 - textWidth("PUNCH CLUB 3D", 6) / 2, 43, 6, "PUNCH CLUB 3D", 250, 250, 250);
    return img;
}

// Torcida: fileiras de pessoas (cabeca + ombros), as de tras mais escuras
static Image makeCrowd() {
    Image img(512, 256);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 512; ++x) img.set(x, y, 14, 12, 18);
    const int rows = 5;
    for (int row = rows - 1; row >= 0; --row) {           // de tras para frente
        float baseY = 18 + row * 48.0f;
        float shade = 1.0f - row * 0.13f;
        for (float x = randf(0, 10); x < 520; x += randf(20, 28)) {
            float px = x + randf(-3, 3), py = baseY + randf(-4, 4);
            float sr = randf(40, 230), sg = randf(30, 200), sb = randf(30, 220);
            if (chance(0.3f)) { sr = sg = sb = randf(30, 70); }          // roupas escuras
            img.ellipse(px, py, 11, 16, sr * shade, sg * shade, sb * shade);   // tronco
            float skin = randf(0.45f, 1.0f);
            img.disc(px, py + 21, 7.5f, 220 * skin * shade, 170 * skin * shade, 130 * skin * shade);
            if (chance(0.4f)) img.disc(px + randf(-2, 2), py + 26, 6, 30 * shade, 22 * shade, 18 * shade); // cabelo
            if (chance(0.12f)) {                                   // braco levantado
                float side = chance(0.5f) ? 1.0f : -1.0f;
                for (int k = 0; k < 18; ++k) img.disc(px + side * (9 + k * 0.35f), py + 8 + k, 2.5f, sr * shade, sg * shade, sb * shade);
            }
            if (chance(0.06f)) img.disc(px + 6, py + 20, 2.0f, 255, 255, 240);  // tela de celular
        }
    }
    return img;
}

static Image makeWood() {
    Image img(256, 256);
    for (int y = 0; y < 256; ++y) {
        int plank = y / 32;
        float tone = 0.75f + 0.35f * hash2(plank, 0, 11);
        for (int x = 0; x < 256; ++x) {
            float g = std::sin((x + plank * 50) * 0.08f + fbm(x / 32.0f, y / 8.0f, 5, 8) * 6.0f) * 0.5f + 0.5f;
            float k = tone * (0.8f + 0.2f * g);
            if (y % 32 < 2) k *= 0.45f;                                 // junta entre tabuas
            if ((x + plank * 97) % 256 < 2) k *= 0.5f;
            img.set(x, y, 120 * k, 74 * k, 40 * k);
        }
    }
    return img;
}

// Couro: quase branco (a cor vem do material via GL_MODULATE)
static Image makeLeather() {
    Image img(128, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            float n = fbm(x / 8.0f, y / 8.0f, 21, 16, 3);
            float v = 205 + 50 * n;
            img.set(x, y, v, v, v);
        }
    return img;
}

static Image makeBag() {
    Image img(256, 256);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            float n = fbm(x / 16.0f, y / 16.0f, 31, 16, 3);
            float v = 190 + 60 * n;
            if (x % 64 < 2) v *= 0.6f;                               // costuras verticais
            if (y > 112 && y < 144) v *= 0.35f;                      // faixa de fita
            img.set(x, y, v, v, v);
        }
    drawText(img, 128 - textWidth("PC3D", 4) / 2, 180, 4, "PC3D", 250, 250, 250);
    return img;
}

static Image makeRoad() {
    Image img(256, 256);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256; ++x) {
            float n = hash2(x, y, 41) * 0.5f + fbm(x / 16.0f, y / 16.0f, 43, 16) * 0.5f;
            float v = 55 + 40 * n;
            img.set(x, y, v, v, v * 1.05f);
            if (x > 124 && x < 132 && (y % 128) < 70) img.set(x, y, 230, 200, 40);  // faixa central
            if (x < 6 || x > 249) img.set(x, y, 220, 220, 220);
        }
    return img;
}

static Image makeGrass() {
    Image img(128, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            float n = fbm(x / 8.0f, y / 8.0f, 51, 16) * 0.6f + hash2(x, y, 52) * 0.4f;
            img.set(x, y, 40 + 40 * n, 100 + 70 * n, 30 + 30 * n);
        }
    return img;
}

static Image makeBanner() {
    Image img(256, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 256; ++x) {
            img.set(x, y, 230, 180, 30);
            if (y < 5 || y > 58) img.set(x, y, 120, 20, 20);
        }
    drawText(img, 128 - textWidth("CAMPEONATO", 3) / 2, 22, 3, "CAMPEONATO", 120, 20, 20);
    return img;
}

static Image makeConcrete() {
    Image img(128, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x) {
            float n = fbm(x / 16.0f, y / 16.0f, 61, 8) * 0.7f + hash2(x, y, 62) * 0.3f;
            float v = 60 + 50 * n;
            img.set(x, y, v, v * 0.97f, v * 0.95f);
        }
    return img;
}

void init() {
    std::srand(1234);  // texturas sempre iguais
    ids[TEX_CANVAS]   = upload(makeCanvas());
    ids[TEX_APRON]    = upload(makeApron());
    ids[TEX_CROWD]    = upload(makeCrowd());
    ids[TEX_WOOD]     = upload(makeWood());
    ids[TEX_LEATHER]  = upload(makeLeather());
    ids[TEX_BAG]      = upload(makeBag());
    ids[TEX_ROAD]     = upload(makeRoad());
    ids[TEX_GRASS]    = upload(makeGrass());
    ids[TEX_BANNER]   = upload(makeBanner());
    ids[TEX_CONCRETE] = upload(makeConcrete());
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // textura x iluminacao
}

GLuint get(TexId id) { return ids[id]; }

void bind(TexId id) {
    if (!enabled) { glDisable(GL_TEXTURE_2D); return; }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ids[id]);
}
void unbind() { glDisable(GL_TEXTURE_2D); }

} // namespace Textures
