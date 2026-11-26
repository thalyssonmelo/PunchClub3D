// ============================================================================
//  Audio.cpp - Sintese dos efeitos + reproducao com miniaudio
// ============================================================================
#include "Audio.h"
#include "Common.h"
#include <vector>
#include <cstdio>

#ifndef NO_AUDIO
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_FLAC
#define MA_NO_MP3
#include "miniaudio.h"
#endif

namespace Audio {

static bool ok = false;
static bool muted = false;
static float crowdLevel = 0.3f;
static float sampleRate = 44100.0f;
static std::vector<float> buffers[NUM_SND];

struct Delayed { SoundId id; float t, vol, pitch; };
static std::vector<Delayed> delayed;

// ---------------------------------------------------------------------------
// SINTESE
// ---------------------------------------------------------------------------
struct Synth {
    std::vector<float> s;
    float sr;
    Synth(float seconds, float rate) : s((size_t)(seconds * rate), 0.0f), sr(rate) {}
    float t(size_t i) const { return i / sr; }
    void normalize(float peak) {
        float m = 1e-6f;
        for (float v : s) m = std::max(m, std::fabs(v));
        for (float& v : s) v = v / m * peak;
    }
};

static float noise() { return randf(-1, 1); }

// Batida grave com queda de frequencia (som de impacto)
static void addThump(Synth& sy, float f0, float f1, float decay, float amp, float start = 0) {
    float phase = 0;
    for (size_t i = (size_t)(start * sy.sr); i < sy.s.size(); ++i) {
        float tt = sy.t(i) - start;
        float f = f1 + (f0 - f1) * std::exp(-tt * 25.0f);
        phase += 2 * PI * f / sy.sr;
        sy.s[i] += std::sin(phase) * std::exp(-tt * decay) * amp;
    }
}
// Ruido filtrado (passa-baixa de 1 polo) com envelope exponencial
static void addNoise(Synth& sy, float lp, float decay, float amp, float start = 0, bool highpass = false) {
    float y = 0;
    for (size_t i = (size_t)(start * sy.sr); i < sy.s.size(); ++i) {
        float tt = sy.t(i) - start;
        float x = noise();
        y += lp * (x - y);
        float v = highpass ? (x - y) : y;
        sy.s[i] += v * std::exp(-tt * decay) * amp;
    }
}
static void addTone(Synth& sy, float f, float decay, float amp, float start = 0) {
    for (size_t i = (size_t)(start * sy.sr); i < sy.s.size(); ++i) {
        float tt = sy.t(i) - start;
        sy.s[i] += std::sin(2 * PI * f * tt) * std::exp(-tt * decay) * amp;
    }
}

static std::vector<float> synth(SoundId id, float sr) {
    switch (id) {
    case SND_PUNCH_HEAD: { Synth s(0.28f, sr); addThump(s, 140, 48, 16, 1.0f); addNoise(s, 0.18f, 55, 0.9f); addNoise(s, 0.5f, 140, 0.5f, 0, true); s.normalize(0.95f); return s.s; }
    case SND_PUNCH_BODY: { Synth s(0.32f, sr); addThump(s, 90, 34, 11, 1.0f); addNoise(s, 0.07f, 28, 1.0f); s.normalize(0.95f); return s.s; }
    case SND_BLOCK:      { Synth s(0.18f, sr); addThump(s, 220, 120, 35, 0.6f); addNoise(s, 0.3f, 50, 0.8f); s.normalize(0.7f); return s.s; }
    case SND_PARRY:      { Synth s(0.45f, sr); addTone(s, 1180, 11, 0.4f); addTone(s, 1790, 14, 0.3f); addThump(s, 220, 120, 35, 0.6f); addNoise(s, 0.3f, 60, 0.6f); s.normalize(0.8f); return s.s; }
    case SND_WHOOSH: {
        Synth s(0.22f, sr); float y = 0;
        for (size_t i = 0; i < s.s.size(); ++i) {
            float u = s.t(i) / 0.22f;
            float lp = 0.03f + 0.25f * std::sin(PI * u);
            y += lp * (noise() - y);
            float env = std::pow(std::sin(PI * u), 2.0f);
            s.s[i] = y * env;
        }
        s.normalize(0.45f); return s.s;
    }
    case SND_BELL: {
        Synth s(2.2f, sr);
        const float base = 640, ratio[4] = { 1.0f, 2.76f, 5.40f, 8.93f }, amp[4] = { 1.0f, 0.6f, 0.35f, 0.2f };
        for (int k = 0; k < 4; ++k) addTone(s, base * ratio[k], 1.3f + k * 0.9f, amp[k]);
        addNoise(s, 0.6f, 80, 0.4f, 0, true);
        s.normalize(0.85f); return s.s;
    }
    case SND_CHEER: {
        Synth s(2.6f, sr); float y1 = 0, y2 = 0, babble = 0;
        for (size_t i = 0; i < s.s.size(); ++i) {
            float tt = s.t(i);
            float env = clampf(tt / 0.35f, 0, 1) * clampf((2.6f - tt) / 1.4f, 0, 1);
            float x = noise();
            y1 += 0.10f * (x - y1); y2 += 0.35f * (x - y2);
            if (i % 600 == 0) babble = randf(0.6f, 1.0f);
            s.s[i] = (y1 * 1.2f + (y2 - y1) * 0.5f) * env * babble;
        }
        for (int v = 0; v < 6; ++v) {   // "vozes" com vibrato
            float f = randf(260, 520), st = randf(0.1f, 0.6f);
            for (size_t i = (size_t)(st * sr); i < s.s.size(); ++i) {
                float tt = s.t(i) - st;
                float env = clampf(tt / 0.2f, 0, 1) * std::exp(-tt * 1.2f);
                s.s[i] += std::sin(2 * PI * f * tt + 3 * std::sin(2 * PI * 5.5f * tt)) * env * 0.05f;
            }
        }
        s.normalize(0.7f); return s.s;
    }
    case SND_CROWD_LOOP: {
        const float len = 6.0f; Synth s(len, sr); float y1 = 0, y2 = 0;
        for (size_t i = 0; i < s.s.size(); ++i) {
            float tt = s.t(i);
            float x = noise();
            y1 += 0.05f * (x - y1); y2 += 0.25f * (x - y2);
            float am = 0.75f + 0.15f * std::sin(2 * PI * tt / len * 2) + 0.1f * std::sin(2 * PI * tt / len * 5);
            s.s[i] = (y1 * 1.5f + (y2 - y1) * 0.3f) * am;
        }
        // costura do loop: cruza o final com o inicio
        size_t fade = (size_t)(0.3f * sr), n = s.s.size();
        for (size_t i = 0; i < fade; ++i) {
            float a = i / (float)fade;
            s.s[i] = s.s[i] * a + s.s[n - fade + i] * (1 - a);
        }
        s.s.resize(n - fade);
        s.normalize(0.5f); return s.s;
    }
    case SND_KO:    { Synth s(1.6f, sr); addThump(s, 120, 38, 3.5f, 1.0f); addNoise(s, 0.05f, 4, 0.7f); s.normalize(0.95f); return s.s; }
    case SND_UI:    {
        Synth s(0.07f, sr);
        for (size_t i = 0; i < s.s.size(); ++i) { float tt = s.t(i); s.s[i] = (std::fmod(tt * 880, 1.0f) < 0.5f ? 0.3f : -0.3f) * std::exp(-tt * 40); }
        return s.s;
    }
    case SND_COUNT: { Synth s(0.14f, sr); addTone(s, 420, 38, 0.8f); addNoise(s, 0.5f, 200, 0.4f); s.normalize(0.7f); return s.s; }
    case SND_BAG: {
        Synth s(0.45f, sr); addThump(s, 95, 40, 10, 1.0f); addNoise(s, 0.08f, 25, 0.9f);
        for (int k = 0; k < 5; ++k) addTone(s, randf(2800, 5200), 14, 0.06f, 0.03f + k * 0.015f);  // corrente
        s.normalize(0.9f); return s.s;
    }
    case SND_MITT:  { Synth s(0.14f, sr); addNoise(s, 0.6f, 90, 1.0f, 0, true); addThump(s, 320, 180, 45, 0.7f); s.normalize(0.85f); return s.s; }
    case SND_HEART: { Synth s(0.55f, sr); addThump(s, 70, 45, 18, 1.0f); addThump(s, 65, 42, 20, 0.7f, 0.18f); s.normalize(0.8f); return s.s; }
    case SND_STEP:  { Synth s(0.09f, sr); addThump(s, 110, 70, 40, 0.8f); addNoise(s, 0.2f, 70, 0.4f); s.normalize(0.5f); return s.s; }
    default: return std::vector<float>(10, 0.0f);
    }
}

#ifndef NO_AUDIO
// ---------------------------------------------------------------------------
// REPRODUCAO (pool de vozes para tocar varios sons simultaneos)
// ---------------------------------------------------------------------------
static ma_engine engine;
struct Voice { ma_audio_buffer_ref ref; ma_sound sound; bool inUse = false; };
static const int MAX_VOICES = 32;
static Voice voices[MAX_VOICES];
static Voice loopVoice;

static bool startVoice(Voice& v, SoundId id, float vol, float pitch, bool loop) {
    if (v.inUse) { ma_sound_uninit(&v.sound); ma_audio_buffer_ref_uninit(&v.ref); v.inUse = false; }
    const std::vector<float>& b = buffers[id];
    if (ma_audio_buffer_ref_init(ma_format_f32, 1, b.data(), b.size(), &v.ref) != MA_SUCCESS) return false;
    v.ref.sampleRate = (ma_uint32)sampleRate;
    if (ma_sound_init_from_data_source(&engine, &v.ref, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &v.sound) != MA_SUCCESS) {
        ma_audio_buffer_ref_uninit(&v.ref);
        return false;
    }
    v.inUse = true;
    ma_sound_set_volume(&v.sound, vol);
    ma_sound_set_pitch(&v.sound, pitch);
    ma_sound_set_looping(&v.sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(&v.sound);
    return true;
}
#endif

bool init() {
#ifndef NO_AUDIO
    if (ma_engine_init(nullptr, &engine) != MA_SUCCESS) {
        std::printf("[audio] nenhum dispositivo de audio - jogo sem som\n");
        return false;
    }
    sampleRate = (float)ma_engine_get_sample_rate(&engine);
    for (int i = 0; i < NUM_SND; ++i) buffers[i] = synth((SoundId)i, sampleRate);
    ok = true;
    startVoice(loopVoice, SND_CROWD_LOOP, crowdLevel, 1.0f, true);
    std::printf("[audio] miniaudio iniciado (%d Hz), %d efeitos sintetizados\n", (int)sampleRate, NUM_SND);
    return true;
#else
    return false;
#endif
}

void shutdown() {
#ifndef NO_AUDIO
    if (!ok) return;
    for (auto& v : voices) if (v.inUse) { ma_sound_uninit(&v.sound); ma_audio_buffer_ref_uninit(&v.ref); }
    if (loopVoice.inUse) { ma_sound_uninit(&loopVoice.sound); ma_audio_buffer_ref_uninit(&loopVoice.ref); }
    ma_engine_uninit(&engine);
    ok = false;
#endif
}

void play(SoundId id, float volume, float pitch) {
#ifndef NO_AUDIO
    if (!ok || muted) return;
    for (auto& v : voices) {
        if (!v.inUse || !ma_sound_is_playing(&v.sound)) {
            startVoice(v, id, volume, pitch, false);
            return;
        }
    }
#else
    (void)id; (void)volume; (void)pitch;
#endif
}

void playDelayed(SoundId id, float delay, float volume, float pitch) {
    delayed.push_back({ id, delay, volume, pitch });
}

void update(float dt) {
    for (size_t i = 0; i < delayed.size();) {
        delayed[i].t -= dt;
        if (delayed[i].t <= 0) { play(delayed[i].id, delayed[i].vol, delayed[i].pitch); delayed.erase(delayed.begin() + i); }
        else ++i;
    }
#ifndef NO_AUDIO
    if (ok && loopVoice.inUse) ma_sound_set_volume(&loopVoice.sound, muted ? 0.0f : crowdLevel);
#endif
}

void setCrowdLevel(float level) { crowdLevel = clampf(level, 0, 1); }
void toggleMute() { muted = !muted; }
bool isMuted() { return muted; }
bool available() { return ok; }

} // namespace Audio
