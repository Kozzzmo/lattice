#pragma once

#include "core/Model.h"

namespace lattice
{

/** One monophonic chip voice. Each tracker track owns exactly one, like on old sound chips. */
class ChipVoice
{
public:
    void prepare (double sampleRate) noexcept;
    void noteOn (int note, float velocity, const Instrument& ins, bool legato) noexcept;
    void noteOff() noexcept;
    void kill() noexcept;

    void setPitchOffset (float semitones) noexcept { pitchOffset = semitones; }
    void setVibrato (float rateHz, float depthSemis) noexcept { vibRate = rateHz; vibDepth = depthSemis; }
    void setVolume (float v) noexcept { targetVolume = v; }
    void setPan (float p) noexcept { pan = p; }

    /** Adds the voice into the two channel pointers (either may alias for mono). */
    void render (float* left, float* right, int numSamples) noexcept;

    bool isActive() const noexcept { return stage != Stage::Idle; }
    int currentNote() const noexcept { return note; }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    float nextSample() noexcept;
    float polyBlep (float t, float dt) const noexcept;

    double sr = 44100.0;
    Instrument ins;
    Stage stage = Stage::Idle;
    int note = -1;

    float env = 0.0f, releaseStart = 0.0f;
    double phase = 0.0;
    float pitch = 60.0f, glideFrom = 60.0f, glideTo = 60.0f;
    float glidePos = 1.0f, glideStep = 1.0f;
    float sweepPos = 1.0f;
    float pitchOffset = 0.0f, vibRate = 0.0f, vibDepth = 0.0f, vibPhase = 0.0f;
    float velocity = 1.0f, volume = 1.0f, targetVolume = 1.0f, pan = 0.0f;
    float noiseValue = 0.0f;
    uint32_t noiseState = 0x1234567u;
};

} // namespace lattice
