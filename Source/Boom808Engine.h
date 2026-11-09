#pragma once

// Epsilon 808 sound engine. Plain C++17, no JUCE, no allocation after prepare().
//
// Voice (runs at 4x the host rate so the distortion stays clean):
//   note stack (last / low / high priority) -> glide -> pitch "punch" envelope -> Analog droop and drift
//   -> sine body + 2nd/3rd harmonics (Tone) + sub octave -> amp envelope -> click layer
//   -> drive (Tape / Tube / Hard / Fold, anti-aliased) with optional Split (clean sub under the drive)
//   -> two halfband stages back to the host rate
// Then, at the host rate:
//   DC block -> low-pass filter -> Translate (phone-speaker harmonics) -> Glue compressor
//   -> mono-safe width -> Kick Lock duck -> output gain -> anti-aliased soft clipper
//   -> Phone Check device model (monitoring only)
//
// Hybrid layers (v1.1): two sample layers (A = character, B = texture) are read at 4x with cubic
// interpolation, follow the played pitch (glide, bend, tune; not the punch), have their own envelopes and
// a Linkwitz-Riley low cut, and are summed with the synth body before the drive. Their stereo side signal
// (above 150 Hz) is added at the host rate. Phase Lock starts the synth body on the phase that matches
// layer A's fundamental once the punch has settled, so the two add up instead of cancelling.
//
// 808 character (v1.1, from the TR-808 bass drum circuit): Snap (the ~6 ms attack shift of the bridged-T),
// Sigh (the slow pitch settle over ~300 ms), Vary (per-hit tolerances), Vel > Length (accent rings longer)
// and Tone Decay (harmonics fade before the body).
//
// Kick Lock: a kick on the sidechain input re-aligns the 808's phase with the kick and ducks it briefly.
// In Exact mode the processor delays MIDI by lookaheadFor() so the engine sees each kick before
// it has to render the matching 808 sample; the processor reports that delay to the host as latency.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace boom
{
enum class DriveMode { tape = 0, tube = 1, hard = 2, fold = 3 };
enum class GlideMode { legato = 0, always = 1, off = 2 };
enum class Priority { last = 0, low = 1, high = 2 };
enum class PhoneModel { phone = 0, laptop = 1, earbuds = 2, car = 3 };
enum class KickMode { exact = 0, live = 1 };

// A sample the engine can play as a layer. The engine never owns or frees it; the owner keeps it alive.
struct LayerSample
{
    const float* mid = nullptr;     // (L + R) / 2
    const float* side = nullptr;    // (L - R) / 2, may be null
    int length = 0;                 // frames
    double rate = 44100.0;
    float rootNote = 24.0f;         // MIDI note the sample plays at its own speed
    float rootHz = 32.7032f;        // its measured fundamental: playback is tuned to this, so samples land in tune
};

// A morph source: single-cycle waves taken from a sample every hopSeconds (see WaveAnalysis.h).
struct MorphSource
{
    const float* tables = nullptr;   // frames * size
    int frames = 0, size = 2048;
    double hopSeconds = 0.02;
};

struct LayerSettings
{
    bool on = false;
    float levelDb = 0.0f, tune = 0.0f;
    float lowCutHz = 0.0f;          // 0 = full range
    float startMs = 0.0f;
    float attackMs = 1.0f, holdMs = 0.0f;
    float decayMs = 10000.0f;       // >= 9999 = Natural: the sample keeps its own shape
    float sustain = 1.0f, releaseMs = 150.0f;
    float width = 0.5f;             // 0..1: how much of the sample's stereo side is kept
    bool track = true;              // follow the played note
    bool invert = false;
};

struct Settings
{
    float tuneSemitones = 0.0f, fineCents = 0.0f;
    float punchSemitones = 12.0f, punchMs = 45.0f;
    float tone = 0.15f, sub = 0.0f, click = 0.15f;
    float analog = 0.0f;                         // 0..1: pitch droop over the tail, drift, rounder attack
    float attackMs = 1.0f, lengthMs = 1600.0f, sustain = 0.0f, releaseMs = 120.0f;
    float glideMs = 90.0f; GlideMode glideMode = GlideMode::legato; float bendRange = 2.0f;
    bool retrigger = false;                      // fresh punch on legato notes
    float velPunch = 0.0f, velDrive = 0.0f;      // 0..1: how much velocity scales Punch and Drive
    float driveDb = 6.0f; DriveMode driveMode = DriveMode::tape; float driveMix = 100.0f;
    float splitHz = 0.0f;                        // 0 = off; otherwise the sub below this stays clean
    float cutoffHz = 20000.0f, resonance = 0.0f;
    float translate = 0.0f, glue = 30.0f, width = 0.0f, outputDb = 0.0f;
    bool kickLock = false; KickMode kickMode = KickMode::exact; float duckMs = 60.0f;
    bool clip = true, phoneCheck = false; PhoneModel phoneModel = PhoneModel::phone;

    // v1.1 hybrid
    std::array<LayerSettings, 2> layers {};
    float engineDb = 0.0f;                       // synth body + click level; <= -48 = off
    float morph = 0.0f;                          // 0..1: the engine's sine merged into the morph source's wave
    bool phaseLock = true; float phaseDeg = 0.0f;
    // v1.1 808 character
    float snap = 0.0f, sigh = 0.0f, vary = 0.0f, velLength = 0.0f, toneDecay = 0.0f;   // all 0..1
};

struct MeterFrame
{
    float frequencyHz = 0.0f;   // fundamental currently playing (0 when silent)
    float note = -1.0f;         // fractional MIDI note (glides show in between)
    float envelope = 0.0f;      // amp envelope, 0..1
    float peakL = 0.0f, peakR = 0.0f;
    float gainReductionDb = 0.0f;
    float harmonics = 0.0f;     // level added by Translate, linear peak
    float lowPeak = 0.0f;       // peak below ~120 Hz, drives the speaker-cone picture
    float meanSquare = 0.0f;    // K-weighted mean square summed over both channels (ITU-R BS.1770): LUFS = -0.691 + 10 log10
    float duck = 0.0f;          // deepest Kick Lock duck in the frame, 0..1
    bool kick = false;          // a kick was locked in this frame
};

class Engine
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setPriority(Priority p) noexcept { priority = p; }
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void allNotesOff() noexcept;
    void pitchBend(float normalised) noexcept { bend = normalised; }   // -1..1

    // Renders and *replaces* the contents of left/right. sidechain (mono, may be null) feeds Kick Lock.
    void render(float* left, float* right, int samples, const Settings& settings,
                const float* sidechain = nullptr) noexcept;

    // MIDI delay the processor must apply (and report as latency) for these settings.
    static int lookaheadFor(const Settings& s, double sampleRate) noexcept
    {
        return s.kickLock && s.kickMode == KickMode::exact ? static_cast<int>(0.005 * sampleRate + 0.5) : 0;
    }

    // Point a layer slot (0 = A, 1 = B) at a sample, or nullptr. Safe to call from any thread;
    // the sample must stay alive while the engine may read it.
    void setLayerSample(int slot, const LayerSample* sample) noexcept
    {
        if (slot >= 0 && slot < 2) layerSource[static_cast<std::size_t>(slot)].store(sample, std::memory_order_release);
    }

    // The wave the engine's oscillator blends into (Blend). nullptr = pure sine. Same lifetime rule as layers.
    void setMorphSource(const MorphSource* source) noexcept { morphSource.store(source, std::memory_order_release); }

    void setMeteringEnabled(bool enabled) noexcept { metering.store(enabled, std::memory_order_relaxed); }
    bool readMeterFrame(MeterFrame& frame) noexcept;
    static constexpr std::size_t scopeSize = 16384;
    int copyScope(float* destination, int count) const noexcept;   // most recent output (mono)

private:
    static constexpr int oversample = 4;
    static constexpr int halfbandTaps = 47;

    struct Halfband
    {
        std::array<float, halfbandTaps> history {};
        int position = 0;
        float push(float x) noexcept;      // write one input sample
        float output(const std::array<float, halfbandTaps>& kernel) const noexcept;
    };

    struct Svf { float ic1 = 0.0f, ic2 = 0.0f; };

    struct LayerVoice
    {
        double pos = 0.0;          // frames into the sample
        float env = 0.0f;
        int stage = 0;             // 0 idle, 1 attack, 2 hold, 3 decay/sustain, 4 release
        float holdLeft = 0.0f;
        Svf hp1, hp2;
    };

    struct Voice
    {
        double phase = 0.0, subPhase = 0.0;
        float env = 0.0f;
        int stage = 0;          // 0 idle, 1 attack, 2 decay/sustain, 3 release
        float velocity = 1.0f;
        float punchTime = 1.0e9f;   // seconds since the punch started
        float clickTime = 1.0e9f;
        float clickNoiseLp = 0.0f, clickNoiseHp = 0.0f, clickPrev = 0.0f;
        float drift = 0.0f;         // per-note detune in cents (Analog)
        // Vary: this hit's own small differences
        float varLevel = 1.0f, varLength = 1.0f, varPunch = 1.0f, varTone = 1.0f, varCents = 0.0f;
        std::array<LayerVoice, 2> layer {};
        bool layersDone() const noexcept { return layer[0].stage == 0 && layer[1].stage == 0; }
    };

    // First-order anti-derivative anti-aliasing state for one waveshaper.
    struct Adaa { double x1 = 0.0, f1 = 0.0; };

    // 2x up / down around the output clipper (halfband FIR both ways).
    struct ClipStage
    {
        Halfband up, down;
        Adaa adaa, guard;
        float process(float x, bool clipOn, const std::array<float, halfbandTaps>& kernel) noexcept;
    };
public:
    // Fixed output delay of the clipper stage, in host samples (reported to the host as latency).
    static constexpr int clipLatency = (halfbandTaps - 1) / 2;   // 23 samples at 2x, twice
private:

    float renderVoiceSample(Voice& v, const Settings& s, double hz, float dtOs, const double* layerStep, float& side) noexcept;
    float renderLayers(Voice& v, const Settings& s, float dtOs, const double* layerStep, float& side) noexcept;
    void startLayers(Voice& v, const Settings& s) noexcept;
    double alignedStartPhase(const Settings& s, double noteSemis, float velocity) const noexcept;
    double layerHz(const Settings& s, int slot, double trackedSemis) const noexcept;
    float driveSample(float x, const Settings& s, float gain, float norm) noexcept;
    float shape(float x, DriveMode mode) const noexcept;
    static double shapeIntegral(double x, DriveMode mode) noexcept;
    static float clipAdaa(float x, Adaa& state) noexcept;
    void startNote(int note, float velocity, bool fromSilence, const Settings& s) noexcept;
    int chooseNote() const noexcept;
    void queueNote(int note, float velocity, bool legato, bool isReturn) noexcept;
    void processSidechain(float sample) noexcept;
    void lockPhase() noexcept;
    void publish() noexcept;

    double sr = 44100.0, srOs = 176400.0;
    std::array<float, halfbandTaps> kernel {};
    Halfband stage1, stage2;

    Voice voice, tail;
    std::array<std::atomic<const LayerSample*>, 2> layerSource {};
    std::atomic<const MorphSource*> morphSource { nullptr };
    const MorphSource* morphNow = nullptr;
    std::array<const LayerSample*, 2> layerNow {};   // what this block plays
    std::array<float, 2> layerGain {}, layerG {};    // per block: level, low-cut coefficient
    float engineGain = 1.0f;
    Svf sideHp1, sideHp2;
    float tailGain = 0.0f, tailStep = 0.0f;
    std::array<int, 16> held {};
    int heldCount = 0, soundingNote = -1;
    Priority priority = Priority::last;
    float targetNote = 48.0f, glideFrom = 48.0f, currentNote = 48.0f;
    float glideElapsed = 0.0f, glideTotal = 0.0f;
    float bend = 0.0f, tailNote = 48.0f;
    int pendingNote = 48;
    float pendingVelocity = 1.0f;
    bool pendingLegato = false, pendingReturn = false, pendingEvent = false, releaseAfterPending = false;
    std::uint32_t noiseSeed = 12345u;
    double driftPhase = 0.0;

    // drive
    Adaa driveAdaa;
    DriveMode adaaMode = DriveMode::tape;
    ClipStage clipL, clipR;
    float dryPrev = 0.0f;
    Svf splitLp1, splitLp2, splitHp1, splitHp2;
    float cachedVelocity = -1.0f, cachedDriveDb = -1000.0f, cachedGain = 1.0f, cachedNorm = 1.0f;
    DriveMode cachedMode = DriveMode::tape;

    float dcX = 0.0f, dcY = 0.0f;
    Svf filter, lowSplit1, lowSplit2, harmHp1, harmHp2, harmLp, widthHp, widthHp2, coneLp;
    Svf phoneA1, phoneA2, phoneB1, phoneB2, phoneC;
    float smoothCutoff = 20000.0f;
    float compEnv = 0.0f, compGainDb = 0.0f, transEnv = 0.0f;
    std::array<float, 2048> widthDelay {};
    int widthPos = 0;

    // Kick Lock
    int lookahead = 0;
    std::int64_t sampleCounter = 0;
    Svf kickLp;
    float kickPrev = 0.0f, kickFast = 0.0f, kickSlow = 0.0f;
    std::int64_t lastUpCross = -1000000, lastOnset = -1000000;
    bool waitingForCross = false;
    std::int64_t waitStarted = 0;
    std::array<std::int64_t, 8> lockQueue {};
    int lockCount = 0;
    int kickGroupDelay = 0;
    float duckGain = 1.0f;
    bool duckFalling = false;
    bool kickInFrame = false;

    // metering
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        double run(double x) noexcept { const auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    std::array<Biquad, 4> kWeight {};   // shelf + high-pass, left and right
    std::atomic<bool> metering { false };
    MeterFrame window;
    double windowSquares = 0.0;
    int windowCount = 0, framePeriod = 735;
    std::array<MeterFrame, 3> slots {};
    unsigned writeIndex = 0, readIndex = 2;
    std::atomic<unsigned> middle { 1 };
    std::array<float, scopeSize> scope {};
    std::atomic<std::uint32_t> scopeWrite { 0 };
};
}
