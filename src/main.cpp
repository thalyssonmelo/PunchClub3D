// ============================================================================
//  PUNCH CLUB 3D
//  Trabalho Final - Computacao Grafica
//
//  Jogo de boxe em primeira pessoa (3D com jogabilidade 2D: o combate
//  acontece no plano entre os dois lutadores), inspirado no gerenciamento
//  de carreira de Punch Club e na luta do boxe do Wii Sports.
//
//  main.cpp: janela GLUT, callbacks e laco de tempo.
// ============================================================================
#include "GLInclude.h"
#include "Game.h"
#include "Scene.h"
#include "Hud.h"
#include "Textures.h"
#include "Audio.h"
#include <cstdio>
#include <cstring>

static int lastTime = 0;
static int frames = 0, fpsTime = 0;
static int screenshotFrame = -1;   // --screenshot N: salva um PPM no quadro N e sai (testes)
static const char* screenshotName = "screenshot.ppm";
static int frameCount = 0;

static void saveScreenshot(const char* path) {
    int w = game.winW, h = game.winH;
    std::vector<unsigned char> px(w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = std::fopen(path, "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) std::fwrite(&px[y * w * 3], 1, w * 3, f);
    std::fclose(f);
}

static void display() {
    Scene::render(game);
    Hud::render(game);
    if (frameCount == screenshotFrame) {
        glFinish();
        saveScreenshot(screenshotName);
        Audio::shutdown();
        std::exit(0);
    }
    glutSwapBuffers();
    frameCount++;
}

static void reshape(int w, int h) {
    game.winW = std::max(1, w);
    game.winH = std::max(1, h);
    glViewport(0, 0, game.winW, game.winH);
}

static void timer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastTime) / 1000.0f;
    lastTime = now;
    if (screenshotFrame >= 0) dt = 1.0f / 60.0f;   // tempo deterministico nos testes
    game.update(dt);

    frames++;
    if (now - fpsTime >= 500) {
        game.fps = frames * 1000.0f / (now - fpsTime);
        frames = 0; fpsTime = now;
    }
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

static void keyboard(unsigned char k, int, int) { game.keyDown(k); }
static void keyboardUp(unsigned char k, int, int) { game.keyUp(k); }
static void special(int k, int, int) { game.specialDown(k); }
static void mouse(int button, int state, int, int) { game.mouseButton(button, state == GLUT_DOWN); }

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Punch Club 3D - Computacao Grafica");

    // Parametros de teste (capturas de tela automaticas)
    std::vector<std::string> script;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--screenshot") && i + 2 < argc) { screenshotFrame = std::atoi(argv[i + 1]); screenshotName = argv[i + 2]; i += 2; }
        else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) { script.push_back(argv[++i]); }
    }

    std::printf("OpenGL: %s | %s\n", (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
    Textures::init();
    Scene::init();
    Audio::init();
    game.init();

    // --keys "s,enter,..." : sequencia de teclas para testes automatizados
    for (const std::string& seq : script) {
        size_t st = 0;
        while (st < seq.size()) {
            size_t e = seq.find(',', st); if (e == std::string::npos) e = seq.size();
            std::string k = seq.substr(st, e - st);
            if (k == "enter") { game.keyDown(13); game.keyUp(13); }
            else if (k == "esc") { game.keyDown(27); game.keyUp(27); }
            else if (k.rfind("wait", 0) == 0) { int n = std::atoi(k.c_str() + 4); for (int j = 0; j < n; ++j) game.update(1.0f / 60.0f); }
            else if (k.rfind("hold", 0) == 0) { game.keyDown((unsigned char)k[4]); }
            else if (k.rfind("rel", 0) == 0) { game.keyUp((unsigned char)k[3]); }
            else if (k.rfind("F", 0) == 0 && k.size() > 1) { game.specialDown(GLUT_KEY_F1 + std::atoi(k.c_str() + 1) - 1); }
            else if (!k.empty()) { game.keyDown((unsigned char)k[0]); game.keyUp((unsigned char)k[0]); }
            st = e + 1;
        }
    }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(special);
    glutMouseFunc(mouse);
    glutIgnoreKeyRepeat(1);
    lastTime = fpsTime = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);

    std::printf("Punch Club 3D iniciado. Boa luta!\n");
    glutMainLoop();
    Audio::shutdown();
    return 0;
}
