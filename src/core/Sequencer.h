#pragma once

#include "core/Model.h"
#include "synth/ChipSynth.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <limits>

namespace lattice
{

struct TransportInfo
{
    bool hostPlaying = false;
    double ppq = 0.0;        // host position at the first sample of the block
    double bpm = 120.0;
    double sampleRate = 44100.0;
    int numSamples = 0;
};

/** Where the sequencer currently is; written by the audio thread, read by the UI. */
struct Playhead
{
    std::atomic<bool> active { false };
    std::atomic<int> order { 0 };
    std::atomic<int> row { 0 };
    std::atomic<int> rows[kNumTracks] {};  // per-track row (differs with polyrhythm)
    std::atomic<double> ppq { 0.0 };
    std::atomic<double> bpm { 120.0 };
    std::atomic<bool> hostPlaying { false };
};

/**
    Turns a Song into sound and MIDI, sample accurately, following the host clock.

    Time is counted in ticks: one beat = lpb * tpl ticks. Rows start on multiples of tpl.
    Song/Loop modes follow the host position; Trigger mode and the editor's preview run on a
    free clock so they also work while the host transport is stopped.
*/
class Sequencer
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    void process (const Song& song, const TransportInfo& transport,
                  juce::AudioBuffer<float>& audio, const juce::MidiBuffer& midiIn, juce::MidiBuffer& midiOut);

    /** Plays a note immediately on a track (editor audition), released after a short time. */
    void audition (int track, int note, int instrument) noexcept;

    void setPreviewPlaying (bool shouldPlay) noexcept { previewRequest.store (shouldPlay); }
    bool isPreviewPlaying() const noexcept { return previewRequest.load(); }
    void setLiveTrack (int t) noexcept { liveTrack.store (t); }

    Playhead playhead;

    // ---- exposed for the unit tests -------------------------------------------------
    struct Position
    {
        int order = 0, rowInPattern = 0, tickInRow = 0;
        int64_t rowsSinceStart = 0, loopCount = 0;
        const Pattern* pattern = nullptr;
    };
    static bool locate (const Song& song, int64_t tick, int fixedOrder, bool followSong, Position& out);
    static bool shouldPlay (const Song& song, const Cell& cell, int order, int row, int track, int64_t loopCount);

private:
    struct TrackState
    {
        int instrument = -1;
        int note = -1;            // sounding note (internal or MIDI)
        int midiNote = -1, midiChannel = 1;
        float velocity = 1.0f;
        uint8_t fxCmd = 0, fxVal = 0;
        int cutTick = -1, delayTick = -1;
        Cell delayed {};
        int arpOffset = 0;
        bool delayedAllowed = true;
        uint32_t delayedKey = 0;
        int auditionSamples = 0;
        int auditionNote = -1;
    };

    void processTick (const Song& song, int64_t tick, int fixedOrder, bool followSong, int sampleOffset, juce::MidiBuffer& out);
    void startRow (const Song& song, int t, const Cell& cell, const Position& pos, int row, int sampleOffset, juce::MidiBuffer& out);
    void applyCell (const Song& song, int t, const Cell& cell, bool noteAllowed, uint32_t humanKey, int sampleOffset, juce::MidiBuffer& out);
    void tickEffects (const Song& song, int t, int tickInRow, int sampleOffset, juce::MidiBuffer& out);
    void playNote (const Song& song, int t, int note, float velocity, bool legato, int sampleOffset, juce::MidiBuffer& out);
    void releaseNote (const Song& song, int t, int sampleOffset, juce::MidiBuffer& out);
    void allNotesOff (const Song& song, int sampleOffset, juce::MidiBuffer& out);
    void renderTo (juce::AudioBuffer<float>& audio, int from, int to);

    double sr = 44100.0;
    std::array<ChipVoice, kNumTracks> voices {};
    std::array<TrackState, kNumTracks> state {};

    bool wasRunning = false;
    int64_t lastTick = std::numeric_limits<int64_t>::min();
    double expectedStartTick = 0.0;
    double freeTick = 0.0;          // free running clock (trigger mode / preview)
    int triggerOrder = -1;          // active trigger entry, -1 = none
    double triggerStartTick = 0.0;
    int triggerNote = -1;
    bool previewWasPlaying = false;
    double previewStartTick = 0.0;
    std::atomic<bool> previewRequest { false };
    std::atomic<int> liveTrack { 0 };
    int liveNote = -1;

    struct PendingAudition { int track, note, instrument; };
    std::array<PendingAudition, 32> auditionQueue {};
    std::atomic<int> auditionWrite { 0 };
    int auditionRead = 0;
};

} // namespace lattice
