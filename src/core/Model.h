#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lattice
{

constexpr int kNumTracks      = 8;
constexpr int kMaxRows        = 256;
constexpr int kDefaultRows    = 64;
constexpr int kNumInstruments = 16;
constexpr int kMaxPatterns    = 128;
constexpr int kMaxOrder       = 128;

constexpr uint8_t kNoteEmpty = 255;
constexpr uint8_t kNoteOff   = 254;
constexpr uint8_t kNoteMax   = 119; // B-8 (C-4 = 60)
constexpr uint8_t kNone      = 255; // empty instrument / volume / prob

/** One step of one track. POD and exactly 6 bytes so patterns serialise as raw blocks. */
struct Cell
{
    uint8_t note       = kNoteEmpty; // 0..119, kNoteOff or kNoteEmpty
    uint8_t instrument = kNone;      // 0..15 or kNone
    uint8_t volume     = kNone;      // 0..127 or kNone
    uint8_t prob       = kNone;      // 0..100 (%) or kNone (= 100)
    uint8_t fxCmd      = 0;          // 0 = none, else 'A'..'Z'
    uint8_t fxVal      = 0;          // 00..FF

    bool isEmpty() const noexcept
    {
        return note == kNoteEmpty && instrument == kNone && volume == kNone && prob == kNone && fxCmd == 0;
    }
    bool hasNote() const noexcept { return note <= kNoteMax; }
    bool operator== (const Cell&) const = default;
};
static_assert (sizeof (Cell) == 6, "Cell must stay 6 bytes");

struct Pattern
{
    std::string name { "Pattern" };
    int numRows = kDefaultRows;
    std::array<std::array<Cell, kMaxRows>, kNumTracks> cells {};

    Cell&       at (int track, int row)       { return cells[(size_t) track][(size_t) row]; }
    const Cell& at (int track, int row) const { return cells[(size_t) track][(size_t) row]; }
};

enum class Waveform : uint8_t { Pulse = 0, Triangle, Saw, Noise };

struct Instrument
{
    std::string name { "Init" };
    Waveform wave = Waveform::Pulse;
    float duty      = 0.5f;   // pulse width 0.05..0.95
    float attack    = 0.002f; // seconds
    float decay     = 0.15f;
    float sustain   = 0.6f;   // 0..1
    float release   = 0.12f;
    float sweep     = 0.0f;   // semitones added at note start, decays to 0
    float sweepTime = 0.05f;  // seconds
    int   crushBits = 16;     // 2..16 (16 = off)
    float glide     = 0.0f;   // seconds of portamento between legato notes
    float gain      = 0.8f;   // 0..1
};

enum class OutputKind : uint8_t { Internal = 0, Midi };

struct Track
{
    std::string name { "Track" };
    OutputKind output = OutputKind::Internal;
    int midiChannel   = 1;   // 1..16 when output == Midi
    int instrument    = 0;   // default instrument
    int length        = 0;   // 0 = pattern length, else rows (polyrhythm)
    int humanize      = 0;   // ± velocity randomisation 0..64
    bool mute = false, solo = false;
    float pan = 0.0f;        // -1..1
};

enum class PlayMode : uint8_t { Song = 0, Loop, Trigger };

/** Parameters of the euclidean generator (kept per track so it survives reloads). */
struct EuclidSettings
{
    int steps = 16, hits = 4, rotation = 0, prob = 100;
};

struct Song
{
    int lpb = 4;  // lines per beat
    int tpl = 6;  // ticks per line
    PlayMode mode = PlayMode::Loop;
    int selectedOrder = 0;  // the entry edited in the UI and looped in Loop mode
    uint32_t seed = 0x3F;
    bool varyEachLoop = false;
    int triggerBaseNote = 48; // C-3 triggers order entry 0

    std::vector<std::shared_ptr<const Pattern>> patterns;
    std::vector<int> order; // indices into patterns
    std::array<Track, kNumTracks> tracks {};
    std::array<Instrument, kNumInstruments> instruments {};
    std::array<EuclidSettings, kNumTracks> euclid {};

    const Pattern* patternForOrder (int orderIndex) const noexcept;
    int numOrders() const noexcept { return (int) order.size(); }
    bool anySolo() const noexcept;
    bool trackAudible (int t) const noexcept;
    int trackLength (int t, const Pattern& p) const noexcept;

    static std::shared_ptr<Song> createDefault();
};

// ---- text helpers shared by UI and tests -----------------------------------------
std::string noteToString (uint8_t note);       // "C-4", "F#3", "OFF", "---"
std::string hex2 (int v);                      // "0F"
uint32_t hashMix (uint32_t a, uint32_t b) noexcept;
float hashToUnit (uint32_t h) noexcept;        // [0,1)

} // namespace lattice
