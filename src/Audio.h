// ============================================================================
//  Audio.h - Audio com a biblioteca miniaudio (cabecalho unico, multiplataforma)
//
//  Os efeitos sonoros sao SINTETIZADOS no inicio do programa (osciladores,
//  ruido filtrado e envelopes), entao o jogo nao depende de arquivos .wav.
//  Compile com -DNO_AUDIO para desativar o audio completamente.
// ============================================================================
#pragma once

enum SoundId {
    SND_PUNCH_HEAD, SND_PUNCH_BODY, SND_BLOCK, SND_WHOOSH, SND_BELL, SND_CHEER,
    SND_KO, SND_UI, SND_COUNT, SND_BAG, SND_MITT, SND_HEART, SND_PARRY, SND_STEP,
    SND_CROWD_LOOP, NUM_SND
};

namespace Audio {
    bool init();
    void shutdown();
    void play(SoundId id, float volume = 1.0f, float pitch = 1.0f);
    void playDelayed(SoundId id, float delay, float volume = 1.0f, float pitch = 1.0f);
    void update(float dt);
    void setCrowdLevel(float level);  // 0..1 volume do ambiente da torcida
    void toggleMute();
    bool isMuted();
    bool available();
}
