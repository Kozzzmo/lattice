#include "synth/ChipSynth.h"

#include <algorithm>
#include <cmath>

namespace lattice
{

namespace
{
    constexpr float kTwoPi = 6.283185307f;
    inline float noteToHz (float n) noexcept { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }
}

void ChipVoice::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    kill();
}

void ChipVoice::noteOn (int newNote, float vel, const Instrument& instrument, bool legato) noexcept
{
    const bool wasPlaying = stage != Stage::Idle && stage != Stage::Release;
    ins = instrument;
    velocity = std::clamp (vel, 0.0f, 1.0f);
    targetVolume = 1.0f;

    if (legato && wasPlaying && ins.glide > 0.0f)
    {
        glideFrom = pitch;
        glideTo = (float) newNote;
        glidePos = 0.0f;
        glideStep = (float) (1.0 / (ins.glide * sr));
        note = newNote;
        return; // keep the envelope running: true legato
    }

    if (legato && wasPlaying)
    {
        pitch = glideTo = glideFrom = (float) newNote;
        glidePos = 1.0f;
        note = newNote;
        return;
    }

    note = newNote;
    pitch = glideFrom = glideTo = (float) newNote;
    glidePos = 1.0f;
    sweepPos = std::abs (ins.sweep) > 1.0e-4f ? 0.0f : 1.0f;
    pitchOffset = 0.0f;
    vibRate = vibDepth = 0.0f;
    phase = ins.wave == Waveform::Triangle ? 0.25 : 0.0; // start triangle at zero crossing
    volume = 1.0f;
    stage = Stage::Attack;
    // retrigger from the current level avoids clicks when a note cuts the previous one
}

void ChipVoice::noteOff() noexcept
{
    if (stage == Stage::Idle || stage == Stage::Release)
        return;
    releaseStart = env;
    stage = Stage::Release;
}

void ChipVoice::kill() noexcept
{
    stage = Stage::Idle;
    env = 0.0f;
    note = -1;
}

float ChipVoice::polyBlep (float t, float dt) const noexcept
{
    if (t < dt)       { t /= dt;            return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

float ChipVoice::nextSample() noexcept
{
    // --- envelope -------------------------------------------------------------
    const float inv = (float) (1.0 / sr);
    switch (stage)
    {
        case Stage::Attack:
            env += ins.attack > 0.0005f ? inv / ins.attack : 1.0f;
            if (env >= 1.0f) { env = 1.0f; stage = Stage::Decay; }
            break;
        case Stage::Decay:
            env -= ins.decay > 0.0005f ? inv / ins.decay * (1.0f - ins.sustain) : 1.0f;
            if (env <= ins.sustain)
            {
                env = ins.sustain;
                stage = ins.sustain <= 0.0001f ? Stage::Idle : Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            break;
        case Stage::Release:
            env -= ins.release > 0.0005f ? inv / ins.release * std::max (releaseStart, 0.05f) : 1.0f;
            if (env <= 0.0f) { env = 0.0f; stage = Stage::Idle; }
            break;
        case Stage::Idle:
            return 0.0f;
    }

    // --- pitch ----------------------------------------------------------------
    if (glidePos < 1.0f)
    {
        glidePos = std::min (1.0f, glidePos + glideStep);
        pitch = glideFrom + (glideTo - glideFrom) * glidePos;
    }

    float p = pitch + pitchOffset;
    if (sweepPos < 1.0f)
    {
        sweepPos = std::min (1.0f, sweepPos + (float) (1.0 / (std::max (0.001f, ins.sweepTime) * sr)));
        const float k = 1.0f - sweepPos;
        p += ins.sweep * k * k;
    }
    if (vibDepth > 0.0f)
    {
        vibPhase += vibRate * inv;
        if (vibPhase >= 1.0f) vibPhase -= 1.0f;
        p += vibDepth * std::sin (vibPhase * kTwoPi);
    }

    const float hz = noteToHz (p);
    const float dt = std::min (0.49f, (float) (hz / sr));

    // --- oscillator -------------------------------------------------------------
    float s = 0.0f;
    const float t = (float) phase;
    switch (ins.wave)
    {
        case Waveform::Pulse:
        {
            const float duty = std::clamp (ins.duty, 0.05f, 0.95f);
            s = t < duty ? 1.0f : -1.0f;
            s += polyBlep (t, dt);
            float t2 = t - duty + 1.0f;
            if (t2 >= 1.0f) t2 -= 1.0f;
            s -= polyBlep (t2, dt);
            break;
        }
        case Waveform::Saw:
            s = 2.0f * t - 1.0f - polyBlep (t, dt);
            break;
        case Waveform::Triangle:
            s = 4.0f * std::abs (t - 0.5f) - 1.0f;
            break;
        case Waveform::Noise:
            break;
    }

    if (ins.wave == Waveform::Noise)
    {
        // sample & hold noise clocked by the note: low notes = gritty, high notes = hiss
        const double clock = std::min (sr * 0.5, (double) hz * 8.0);
        phase += clock / sr;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
            noiseValue = (float) (noiseState & 0xFFFF) / 32767.5f - 1.0f;
        }
        s = noiseValue;
    }
    else
    {
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
    }

    if (ins.crushBits < 16)
    {
        const float levels = (float) ((1 << std::max (2, ins.crushBits)) / 2);
        s = std::round (s * levels) / levels;
    }

    volume += (targetVolume - volume) * 0.002f;
    return s * env * velocity * volume * ins.gain;
}

void ChipVoice::render (float* left, float* right, int numSamples) noexcept
{
    if (stage == Stage::Idle)
        return;

    const float angle = (std::clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * 3.14159265f;
    const float gl = std::cos (angle) * 1.41421356f * 0.5f;
    const float gr = std::sin (angle) * 1.41421356f * 0.5f;

    for (int i = 0; i < numSamples && stage != Stage::Idle; ++i)
    {
        const float s = nextSample();
        left[i] += s * gl;
        if (right != left)
            right[i] += s * gr;
    }
}

} // namespace lattice
