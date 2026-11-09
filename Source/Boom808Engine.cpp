#include "Boom808Engine.h"

#include <algorithm>
#include <cmath>

namespace boom
{
namespace
{
constexpr double twoPi = 6.283185307179586;
constexpr float pi = 3.14159265f;

float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
float gainToDb(float g) noexcept { return 20.0f * std::log10(std::max(g, 1.0e-9f)); }

double besselI0(double x) noexcept
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 30; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

// Trapezoidal state-variable filter (Simper). Returns low-pass, band-pass and high-pass at once.
struct SvfOut { float low, band, high; };
template <typename State>
SvfOut svf(State& s, float input, float g, float k) noexcept
{
    const auto a1 = 1.0f / (1.0f + g * (g + k));
    const auto a2 = g * a1, a3 = g * a2;
    const auto v3 = input - s.ic2;
    const auto v1 = a1 * s.ic1 + a2 * v3;
    const auto v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
    s.ic1 = 2.0f * v1 - s.ic1;
    s.ic2 = 2.0f * v2 - s.ic2;
    return { v2, v1, input - k * v1 - v2 };
}

constexpr float butterworthK = 1.41421356f;
constexpr float headroom = 0.6f;   // -4.4 dB

// log(cosh(x)) without overflow: the integral of tanh.
double logCosh(double x) noexcept
{
    const auto a = std::abs(x);
    return a + std::log1p(std::exp(-2.0 * a)) - 0.6931471805599453;
}

// Hard mode: linear to 0.8, a quadratic knee to 1.2, flat above. Its integral has a closed form,
// so it can be anti-aliased like the others.
constexpr double hardA = 0.8, hardB = 1.2, hardW = hardB - hardA;
double hardShape(double x) noexcept
{
    const auto a = std::abs(x);
    const auto y = a <= hardA ? a : a < hardB ? a - (a - hardA) * (a - hardA) / (2.0 * hardW) : 1.0;
    return std::copysign(y, x);
}
double hardIntegral(double x) noexcept
{
    const auto a = std::abs(x);
    if (a <= hardA) return 0.5 * a * a;
    if (a < hardB) return 0.5 * a * a - (a - hardA) * (a - hardA) * (a - hardA) / (6.0 * hardW);
    return 0.5 * hardB * hardB - hardW * hardW / 6.0 + (a - hardB);
}

// Output clipper: clean to 0.9, then a tanh knee into 1.0, scaled to a 0.92 ceiling so the overshoot
// of the 2x down-sampling filter has room; a gentle guard knee after it catches the rest.
constexpr double clipCeiling = 0.92;
double softClipUnit(double x) noexcept
{
    const auto a = std::abs(x);
    return a < 0.9 ? x : std::copysign(0.9 + 0.1 * std::tanh((a - 0.9) / 0.1), x);
}
double softClipUnitIntegral(double x) noexcept
{
    const auto a = std::abs(x);
    if (a < 0.9) return 0.5 * a * a;
    return 0.405 + 0.9 * (a - 0.9) + 0.01 * logCosh((a - 0.9) / 0.1);
}
double softClip(double x) noexcept { return clipCeiling * softClipUnit(x / clipCeiling); }
// guard knee: linear to 0.95, tanh into 1.0
double guardShape(double x) noexcept
{
    const auto a = std::abs(x);
    return a < 0.95 ? x : std::copysign(0.95 + 0.05 * std::tanh((a - 0.95) / 0.05), x);
}
double guardIntegral(double x) noexcept
{
    const auto a = std::abs(x);
    if (a < 0.95) return 0.5 * a * a;
    return 0.45125 + 0.95 * (a - 0.95) + 0.0025 * logCosh((a - 0.95) / 0.05);
}
double softClipIntegral(double x) noexcept { return clipCeiling * clipCeiling * softClipUnitIntegral(x / clipCeiling); }
}

float Engine::Halfband::push(float x) noexcept
{
    history[static_cast<std::size_t>(position)] = x;
    position = (position + 1) % halfbandTaps;
    return x;
}

float Engine::Halfband::output(const std::array<float, halfbandTaps>& h) const noexcept
{
    float sum = 0.0f;
    int index = position;   // oldest sample
    for (int k = halfbandTaps - 1; k >= 0; --k)
    {
        sum += h[static_cast<std::size_t>(k)] * history[static_cast<std::size_t>(index)];
        index = index + 1 == halfbandTaps ? 0 : index + 1;
    }
    return sum;
}

void Engine::prepare(double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    srOs = sr * oversample;
    framePeriod = std::max(1, static_cast<int>(sr / 60.0));

    // Halfband low-pass (cutoff at a quarter of its input rate), Kaiser-windowed sinc, beta 8.
    const int centre = (halfbandTaps - 1) / 2;
    double total = 0.0;
    for (int n = 0; n < halfbandTaps; ++n)
    {
        const auto m = n - centre;
        const auto x = 0.5 * m;
        const auto sinc = m == 0 ? 1.0 : std::sin(3.141592653589793 * x) / (3.141592653589793 * x);
        const auto r = 2.0 * n / (halfbandTaps - 1) - 1.0;
        const auto kaiser = besselI0(8.0 * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(8.0);
        kernel[static_cast<std::size_t>(n)] = static_cast<float>(0.5 * sinc * kaiser);
        total += kernel[static_cast<std::size_t>(n)];
    }
    for (auto& h : kernel)
        h = static_cast<float>(h / total);

    // The kick detector's low-pass and the two halfband stages delay what we see and what we play;
    // Kick Lock schedules its phase reset this much earlier.
    const auto lowpassDelay = butterworthK / (twoPi * 250.0) * sr;
    const auto halfbandDelay = (halfbandTaps - 1) * 0.5 / 4.0 + (halfbandTaps - 1) * 0.5 / 2.0;   // in host samples
    kickGroupDelay = static_cast<int>(lowpassDelay + halfbandDelay + clipLatency + 0.5);

    // K-weighting for the loudness meter (BS.1770 pre-filter, designed for any sample rate)
    {
        const auto design = [this](Biquad& q, double f0, double gainDb, double quality, bool shelf)
        {
            const auto kk = std::tan(3.141592653589793 * f0 / sr);
            if (shelf)
            {
                const auto vh = std::pow(10.0, gainDb / 20.0), vb = std::pow(vh, 0.4996667741545416);
                const auto a0 = 1.0 + kk / quality + kk * kk;
                q.b0 = (vh + vb * kk / quality + kk * kk) / a0;
                q.b1 = 2.0 * (kk * kk - vh) / a0;
                q.b2 = (vh - vb * kk / quality + kk * kk) / a0;
                q.a1 = 2.0 * (kk * kk - 1.0) / a0;
                q.a2 = (1.0 - kk / quality + kk * kk) / a0;
            }
            else
            {
                const auto a0 = 1.0 + kk / quality + kk * kk;
                q.b0 = 1.0; q.b1 = -2.0; q.b2 = 1.0;   // as tabulated in BS.1770
                q.a1 = 2.0 * (kk * kk - 1.0) / a0;
                q.a2 = (1.0 - kk / quality + kk * kk) / a0;
            }
        };
        design(kWeight[0], 1681.974450955533, 3.999843853973347, 0.7071752369554196, true);
        design(kWeight[1], 38.13547087602444, 0.0, 0.5003270373238773, false);
        kWeight[2] = kWeight[0];
        kWeight[3] = kWeight[1];
    }
    reset();
}

void Engine::reset() noexcept
{
    stage1 = {};
    stage2 = {};
    voice = {};
    tail = {};
    tailGain = 0.0f;
    heldCount = 0;
    soundingNote = -1;
    pendingEvent = releaseAfterPending = false;
    dcX = dcY = 0.0f;
    filter = lowSplit1 = lowSplit2 = harmHp1 = harmHp2 = harmLp = widthHp = widthHp2 = coneLp = {};
    phoneA1 = phoneA2 = phoneB1 = phoneB2 = phoneC = {};
    splitLp1 = splitLp2 = splitHp1 = splitHp2 = {};
    driveAdaa = {};
    driveAdaa.f1 = shapeIntegral(0.0, adaaMode);
    clipL = {};
    clipR = {};
    dryPrev = 0.0f;
    cachedVelocity = -1.0f;
    compEnv = 0.0f;
    compGainDb = 0.0f;
    transEnv = 0.0f;
    widthDelay.fill(0.0f);
    kickLp = {};
    kickPrev = kickFast = kickSlow = 0.0f;
    lastUpCross = lastOnset = -1000000;
    waitingForCross = false;
    lockCount = 0;
    duckGain = 1.0f;
    duckFalling = false;
    sampleCounter = 0;
    window = {};
    windowSquares = 0.0;
    windowCount = 0;
    for (auto& q : kWeight) q.z1 = q.z2 = 0.0;
}

void Engine::startNote(int note, float velocity, bool fromSilence) noexcept
{
    // A note that starts while the previous one still sounds hands the old voice to a 5 ms
    // crossfade tail, so the new hit can restart its phase (consistent punch) without a click.
    if (voice.stage != 0 && voice.env > 0.0005f)
    {
        tail = voice;
        tailGain = 1.0f;
        tailStep = static_cast<float>(1.0 / (0.005 * srOs));
        tailNote = currentNote;
    }
    voice.phase = 0.0;
    voice.subPhase = 0.0;
    voice.env = 0.0f;
    voice.stage = 1;
    voice.velocity = velocity;
    voice.punchTime = 0.0f;
    voice.clickTime = 0.0f;
    voice.clickNoiseLp = voice.clickNoiseHp = voice.clickPrev = 0.0f;
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    voice.drift = static_cast<float>(static_cast<int>(noiseSeed >> 9) - (1 << 22)) / static_cast<float>(1 << 22);   // -1..1, scaled by Analog
    if (fromSilence)
    {
        currentNote = glideFrom = targetNote = static_cast<float>(note);
        glideTotal = 0.0f;
    }
}

int Engine::chooseNote() const noexcept
{
    if (heldCount == 0)
        return -1;
    if (priority == Priority::last)
        return held[static_cast<std::size_t>(heldCount - 1)];
    int best = held[0];
    for (int i = 1; i < heldCount; ++i)
    {
        const auto n = held[static_cast<std::size_t>(i)];
        best = priority == Priority::low ? std::min(best, n) : std::max(best, n);
    }
    return best;
}

void Engine::queueNote(int note, float velocity, bool legato, bool isReturn) noexcept
{
    pendingNote = note;
    pendingVelocity = velocity;
    pendingLegato = legato;
    pendingReturn = isReturn;
    pendingEvent = true;
    soundingNote = note;
}

void Engine::noteOn(int note, float velocity) noexcept
{
    velocity = std::clamp(velocity, 0.0f, 1.0f);
    const auto wasHeld = heldCount > 0;
    // keep the stack ordered by press time; drop a repeat of the same key
    int write = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[static_cast<std::size_t>(i)] != note)
            held[static_cast<std::size_t>(write++)] = held[static_cast<std::size_t>(i)];
    heldCount = write;
    if (heldCount < static_cast<int>(held.size()))
        held[static_cast<std::size_t>(heldCount++)] = note;

    const auto target = chooseNote();
    if (!wasHeld)
        queueNote(target, velocity, false, false);
    else if (target != soundingNote)
        queueNote(target, velocity, true, false);
    // otherwise the new key is hidden by Low / High priority and only waits in the stack
}

void Engine::noteOff(int note) noexcept
{
    int write = 0;
    for (int i = 0; i < heldCount; ++i)
        if (held[static_cast<std::size_t>(i)] != note)
            held[static_cast<std::size_t>(write++)] = held[static_cast<std::size_t>(i)];
    const auto removed = write != heldCount;
    heldCount = write;
    if (!removed)
        return;
    if (heldCount == 0)
    {
        if (pendingEvent)
            releaseAfterPending = true;   // a note that starts and ends before the next sample
        else if (voice.stage != 0)
            voice.stage = 3;
        return;
    }
    const auto target = chooseNote();
    if (target != soundingNote)
        queueNote(target, voice.velocity, true, true);
}

void Engine::allNotesOff() noexcept
{
    heldCount = 0;
    soundingNote = -1;
    pendingEvent = false;
    if (voice.stage != 0)
        voice.stage = 3;
}

float Engine::shape(float x, DriveMode mode) const noexcept
{
    switch (mode)
    {
        case DriveMode::tape: return std::tanh(x);
        case DriveMode::tube: { const auto t = std::tanh(x); return t + 0.2f * t * t; }   // bounded even harmonics
        case DriveMode::hard: return static_cast<float>(hardShape(x));
        case DriveMode::fold: return std::sin(x * pi * 0.5f);
    }
    return x;
}

double Engine::shapeIntegral(double x, DriveMode mode) noexcept
{
    switch (mode)
    {
        case DriveMode::tape: return logCosh(x);
        case DriveMode::tube: return logCosh(x) + 0.2 * (x - std::tanh(x));
        case DriveMode::hard: return hardIntegral(x);
        case DriveMode::fold: return -2.0 / 3.141592653589793 * std::cos(x * 3.141592653589793 * 0.5);
    }
    return 0.5 * x * x;
}

// One drive sample at 4x, anti-aliased with the shaper's integral (first-order ADAA), then the
// optional Split: the sub below splitHz is taken from the clean signal, the rest from the driven one.
float Engine::driveSample(float x, const Settings& s, float gain, float norm) noexcept
{
    const double u = static_cast<double>(x) * gain;
    if (s.driveMode != adaaMode)
    {
        // the integral's constant differs per shape: re-base the previous point so the next difference is valid
        adaaMode = s.driveMode;
        driveAdaa.f1 = shapeIntegral(driveAdaa.x1, adaaMode);
    }
    const double f = shapeIntegral(u, s.driveMode);
    const double d = u - driveAdaa.x1;
    const double shaped = std::abs(d) > 1.0e-6 ? (f - driveAdaa.f1) / d
                                                : shape(static_cast<float>(0.5 * (u + driveAdaa.x1)), s.driveMode);
    driveAdaa.x1 = u;
    driveAdaa.f1 = f;
    const auto dry = 0.5f * (x + dryPrev);   // ADAA delays the wet path by half a sample; match it
    dryPrev = x;
    const auto wet = static_cast<float>(shaped) * norm;
    const auto mixed = dry + std::clamp(s.driveMix * 0.01f, 0.0f, 1.0f) * (wet - dry);
    if (s.splitHz <= 0.0f)
        return mixed;
    // Linkwitz-Riley 4th order: the two halves sum flat, so with no drive nothing changes.
    const auto g = std::tan(pi * s.splitHz / static_cast<float>(srOs));
    const auto low = svf(splitLp2, svf(splitLp1, dry, g, butterworthK).low, g, butterworthK).low;
    const auto high = svf(splitHp2, svf(splitHp1, mixed, g, butterworthK).high, g, butterworthK).high;
    return low + high;
}

float Engine::clipAdaa(float x, Adaa& st) noexcept
{
    const double f = softClipIntegral(x);
    const double d = static_cast<double>(x) - st.x1;
    const double y = std::abs(d) > 1.0e-6 ? (f - st.f1) / d : softClip(0.5 * (x + st.x1));
    st.x1 = x;
    st.f1 = f;
    return static_cast<float>(y);
}

// Up 2x, clip with ADAA at the doubled rate, back down. Runs even with Clip off so the delay never changes.
float Engine::ClipStage::process(float x, bool clipOn, const std::array<float, halfbandTaps>& h) noexcept
{
    up.push(2.0f * x);
    auto a = up.output(h);
    up.push(0.0f);
    auto b = up.output(h);
    if (clipOn)
    {
        a = clipAdaa(a, adaa);
        b = clipAdaa(b, adaa);
    }
    else
    {
        adaa.x1 = b;
        adaa.f1 = softClipIntegral(b);
    }
    down.push(a);
    down.push(b);
    const auto y = down.output(h);
    if (!clipOn)
    {
        guard.x1 = y;
        guard.f1 = guardIntegral(y);
        return y;
    }
    // The down-sampling filter can overshoot by a few percent; a second, gentle anti-aliased knee
    // (linear to 0.95, asymptote 1.0) keeps true sample peaks at or under 0 dBFS.
    const double g = guardIntegral(y);
    const double d = static_cast<double>(y) - guard.x1;
    const double out = std::abs(d) > 1.0e-6 ? (g - guard.f1) / d : guardShape(0.5 * (y + guard.x1));
    guard.x1 = y;
    guard.f1 = g;
    return static_cast<float>(out);
}

float Engine::renderVoiceSample(Voice& v, const Settings& s, double hz, float dtOs) noexcept
{
    if (v.stage == 0)
        return 0.0f;

    // amp envelope (Analog rounds the attack a little)
    const auto attack = std::max(0.0005f + 0.004f * s.analog, s.attackMs * 0.001f);
    switch (v.stage)
    {
        case 1:
            v.env += dtOs / attack;
            if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 2; }
            break;
        case 2:
        {
            const auto coef = std::exp(-6.9078f * dtOs / std::max(0.02f, s.lengthMs * 0.001f));   // -60 dB at Length
            v.env = s.sustain + (v.env - s.sustain) * coef;
            break;
        }
        case 3:
        {
            const auto coef = std::exp(-6.9078f * dtOs / std::max(0.003f, s.releaseMs * 0.001f));
            v.env *= coef;
            if (v.env < 1.0e-5f) { v.env = 0.0f; v.stage = 0; }
            break;
        }
        default: break;
    }

    // oscillator: sine body, Tone adds 2nd and 3rd harmonics, Sub adds an octave below
    v.phase += hz / srOs;
    v.phase -= std::floor(v.phase);
    v.subPhase += 0.5 * hz / srOs;
    v.subPhase -= std::floor(v.subPhase);
    const auto p = static_cast<float>(twoPi * v.phase);
    auto body = std::sin(p) + s.tone * (0.35f * std::sin(2.0f * p) + 0.2f * std::sin(3.0f * p))
              + s.sub * 0.8f * static_cast<float>(std::sin(twoPi * v.subPhase));
    body /= 1.0f + 0.55f * s.tone + 0.8f * s.sub;

    const auto velocityGain = 0.35f + 0.65f * v.velocity;
    auto out = body * v.env * velocityGain;

    // click layer: a few ms of bright noise and a 1.6 kHz blip, for presence on small speakers
    if (v.clickTime < 0.03f && s.click > 0.0f)
    {
        noiseSeed = noiseSeed * 1664525u + 1013904223u;
        const auto noise = static_cast<float>(static_cast<int>(noiseSeed >> 9) - (1 << 22)) / static_cast<float>(1 << 22);
        v.clickNoiseLp += 0.35f * (noise - v.clickNoiseLp);
        const auto bright = v.clickNoiseLp - v.clickPrev;
        v.clickPrev = v.clickNoiseLp;
        const auto t = v.clickTime;
        const auto blip = std::sin(static_cast<float>(twoPi) * 1600.0f * t) * std::exp(-t / 0.005f);
        out += s.click * velocityGain * 0.7f * (bright * 1.6f * std::exp(-t / 0.003f) + 0.6f * blip);
    }
    v.clickTime += dtOs;
    v.punchTime += dtOs;
    return out;
}

void Engine::processSidechain(float x) noexcept
{
    const auto lp = svf(kickLp, x, std::tan(pi * 250.0f / static_cast<float>(sr)), butterworthK).low;
    const auto crossedUp = kickPrev < 0.0f && lp >= 0.0f;
    kickPrev = lp;
    if (crossedUp)
        lastUpCross = sampleCounter;

    const auto a = std::abs(lp);
    const auto dt = static_cast<float>(1.0 / sr);
    kickFast += (a > kickFast ? 1.0f - std::exp(-dt / 0.0005f) : 1.0f - std::exp(-dt / 0.010f)) * (a - kickFast);
    kickSlow += (a > kickSlow ? 1.0f - std::exp(-dt / 0.020f) : 1.0f - std::exp(-dt / 0.150f)) * (a - kickSlow);

    const auto schedule = [this](std::int64_t at)
    {
        auto when = at + lookahead - kickGroupDelay;
        when = std::max(when, sampleCounter);
        if (lockCount < static_cast<int>(lockQueue.size()))
            lockQueue[static_cast<std::size_t>(lockCount++)] = when;
    };

    if (kickFast > 0.02f && kickFast > 2.2f * kickSlow + 0.003f && sampleCounter - lastOnset > static_cast<std::int64_t>(0.08 * sr))
    {
        lastOnset = sampleCounter;
        // the kick's waveform starts at (or just before) its last upward zero crossing
        if (sampleCounter - lastUpCross < static_cast<std::int64_t>(0.006 * sr))
            schedule(lastUpCross);
        else
        {
            waitingForCross = true;
            waitStarted = sampleCounter;
        }
    }
    else if (waitingForCross && (crossedUp || sampleCounter - waitStarted > static_cast<std::int64_t>(0.03 * sr)))
    {
        waitingForCross = false;
        schedule(sampleCounter);
    }
}

void Engine::lockPhase() noexcept
{
    kickInFrame = true;
    duckFalling = true;
    if (voice.stage == 0 || voice.env < 0.001f)
        return;
    // Start the 808's cycle where the kick's starts, so the two add instead of cancelling.
    const auto error = std::min(voice.phase, 1.0 - voice.phase);
    if (error < 30.0 / 360.0)
        return;
    tail = voice;
    tailGain = 1.0f;
    tailStep = static_cast<float>(1.0 / (0.004 * srOs));
    tailNote = currentNote;
    voice.phase = 0.0;
    voice.subPhase = 0.0;
}

void Engine::render(float* left, float* right, int samples, const Settings& s, const float* sidechain) noexcept
{
    const auto dtOs = static_cast<float>(1.0 / srOs);
    const auto dt = static_cast<float>(1.0 / sr);
    const auto outGain = dbToGain(s.outputDb);
    const auto translate = std::clamp(s.translate * 0.01f, 0.0f, 1.0f);
    const auto glue = std::clamp(s.glue * 0.01f, 0.0f, 1.0f);
    const auto width = std::clamp(s.width * 0.01f, 0.0f, 1.0f);
    const auto tw = [this](float hz) { return std::tan(pi * std::min(hz, static_cast<float>(sr) * 0.45f) / static_cast<float>(sr)); };
    const auto gSplit = tw(140.0f), gHarmHp = tw(180.0f), gHarmLp = tw(3000.0f), gWidth = tw(160.0f), gCone = tw(120.0f);
    const auto filterOn = s.cutoffHz < 19500.0f || s.resonance > 0.001f;
    const auto k = 2.0f - 1.9f * std::clamp(s.resonance, 0.0f, 1.0f);
    const auto compAttack = 1.0f - std::exp(-dt / 0.003f), compRelease = 1.0f - std::exp(-dt / 0.12f);
    const auto transAttack = 1.0f - std::exp(-dt / 0.004f), transRelease = 1.0f - std::exp(-dt / 0.060f);
    const auto grAttack = 1.0f - std::exp(-dt / 0.002f), grRelease = 1.0f - std::exp(-dt / 0.10f);
    const auto threshold = -4.0f - 20.0f * glue, ratio = 1.0f + 4.0f * glue, makeup = 6.0f * glue;
    const auto widthTaps = std::min(static_cast<int>(widthDelay.size()) - 1, static_cast<int>(0.007 * sr));
    const auto metersOn = metering.load(std::memory_order_relaxed);
    lookahead = lookaheadFor(s, sr);
    const auto kickOn = s.kickLock && sidechain != nullptr;
    const auto duckFloor = 0.18f;   // -15 dB
    const auto duckFall = (1.0f - duckFloor) / static_cast<float>(0.001 * sr);
    const auto duckRise = s.duckMs > 0.5f ? 1.0f - std::exp(-dt / (s.duckMs * 0.001f / 3.0f)) : 1.0f;
    const auto driftLfoStep = 0.23 / sr;

    // phone model filters
    float gA = 0.0f, gC = 0.0f;
    switch (s.phoneModel)
    {
        case PhoneModel::phone:   gA = tw(320.0f); gC = tw(6000.0f); break;
        case PhoneModel::laptop:  gA = tw(170.0f); gC = tw(12000.0f); break;
        case PhoneModel::earbuds: gA = tw(45.0f);  gC = tw(14000.0f); break;
        case PhoneModel::car:     gA = tw(70.0f);  gC = tw(10000.0f); break;
    }

    for (int i = 0; i < samples; ++i)
    {
        // note events arrive between samples (the processor splits blocks at MIDI timestamps)
        if (pendingEvent)
        {
            pendingEvent = false;
            const auto note = static_cast<float>(pendingNote);
            if (!pendingLegato)
            {
                const auto glideFromSounding = s.glideMode == GlideMode::always && voice.stage != 0 && s.glideMs > 0.5f;
                startNote(pendingNote, pendingVelocity, !glideFromSounding);
                if (glideFromSounding)
                {
                    glideFrom = currentNote;
                    targetNote = note;
                    glideElapsed = 0.0f;
                    glideTotal = s.glideMs * 0.001f;
                }
            }
            else if (s.glideMode == GlideMode::off || s.glideMs <= 0.5f)
            {
                if (pendingReturn) { currentNote = glideFrom = targetNote = note; glideTotal = 0.0f; }
                else startNote(pendingNote, pendingVelocity, true);
            }
            else
            {
                glideFrom = currentNote;
                targetNote = note;
                glideElapsed = 0.0f;
                glideTotal = s.glideMs * 0.001f;
                if (s.retrigger && !pendingReturn && voice.stage != 0)
                {
                    // fresh punch and click on the slide, the pitch keeps gliding
                    voice.punchTime = 0.0f;
                    voice.clickTime = 0.0f;
                    voice.clickNoiseLp = voice.clickPrev = 0.0f;
                    voice.velocity = pendingVelocity;
                    voice.stage = 1;
                }
            }
            pendingReturn = false;
            if (releaseAfterPending)
            {
                releaseAfterPending = false;
                if (heldCount == 0 && voice.stage != 0)
                    voice.stage = 3;
            }
        }

        // Kick Lock: listen to the sidechain, act on scheduled kicks
        if (kickOn)
            processSidechain(sidechain[i]);
        if (lockCount > 0 && lockQueue[0] <= sampleCounter)
        {
            for (int q = 1; q < lockCount; ++q)
                lockQueue[static_cast<std::size_t>(q - 1)] = lockQueue[static_cast<std::size_t>(q)];
            --lockCount;
            if (s.kickLock)
                lockPhase();
        }

        // glide: constant time from the current pitch to the new note, easing out
        if (glideTotal > 0.0f && glideElapsed < glideTotal)
        {
            glideElapsed += dt;
            const auto u = std::min(1.0f, glideElapsed / glideTotal);
            const auto eased = 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u);
            currentNote = glideFrom + (targetNote - glideFrom) * eased;
        }
        else
        {
            currentNote = targetNote;
        }

        const auto punchScale = 1.0f - s.velPunch + s.velPunch * voice.velocity;
        const auto punchTau = std::max(0.002f, s.punchMs * 0.001f / 3.0f);
        const auto punch = s.punchSemitones * punchScale * std::exp(-voice.punchTime / punchTau);
        // Analog: up to 30 cents of droop as the tail decays, +-4 cents per note, a slow 1.5 cent wander
        driftPhase += driftLfoStep;
        driftPhase -= std::floor(driftPhase);
        const auto analog = s.analog * (0.04f * voice.drift + 0.015f * static_cast<float>(std::sin(twoPi * driftPhase))
                                        - (voice.stage >= 2 ? 0.3f * (1.0f - voice.env) : 0.0f));
        const auto semis = currentNote + s.tuneSemitones + s.fineCents * 0.01f + bend * s.bendRange + punch + analog;
        const auto hz = std::min(440.0 * std::pow(2.0, (semis - 69.0) / 12.0), sr * 0.45);
        const auto tailPunch = s.punchSemitones * punchScale * std::exp(-tail.punchTime / punchTau);
        const auto tailHz = std::min(440.0 * std::pow(2.0, (tailNote + s.tuneSemitones + s.fineCents * 0.01f + tailPunch - 69.0) / 12.0), sr * 0.45);

        // drive gain follows velocity when asked; recomputed only when something changes
        const auto driveDb = s.driveDb * (1.0f - s.velDrive + s.velDrive * voice.velocity);
        if (driveDb != cachedDriveDb || s.driveMode != cachedMode)
        {
            cachedDriveDb = driveDb;
            cachedMode = s.driveMode;
            // Fold multiplies harmonics much faster than the other modes, so its drive scale is gentler.
            cachedGain = dbToGain(s.driveMode == DriveMode::fold ? 0.6f * driveDb : driveDb);
            cachedNorm = s.driveMode == DriveMode::fold ? 1.0f
                       : 1.0f / std::max(0.05f, std::abs(shape(cachedGain, s.driveMode)));
        }

        // four oversampled voice samples -> drive -> two halfband stages
        float os[oversample];
        for (int o = 0; o < oversample; ++o)
        {
            auto x = renderVoiceSample(voice, s, hz, dtOs);
            if (tailGain > 0.0f)
            {
                x += tailGain * renderVoiceSample(tail, s, tailHz, dtOs);
                tailGain = std::max(0.0f, tailGain - tailStep);
            }
            os[o] = driveSample(x, s, cachedGain, cachedNorm);
        }
        stage1.push(os[0]); stage1.push(os[1]);
        const auto half0 = stage1.output(kernel);
        stage1.push(os[2]); stage1.push(os[3]);
        const auto half1 = stage1.output(kernel);
        stage2.push(half0); stage2.push(half1);
        auto y = stage2.output(kernel);

        // DC block (Tube and Fold are asymmetric), then the internal headroom trim: a full-scale voice
        // sits at -4.4 dBFS here, so a default 808 peaks around -3 dBFS and Clip only works when pushed.
        const auto dcOut = y - dcX + 0.9995f * dcY;
        dcX = y;
        dcY = dcOut;
        y = dcOut * headroom;

        if (filterOn)
        {
            smoothCutoff += 0.002f * (s.cutoffHz - smoothCutoff);
            y = svf(filter, y, tw(smoothCutoff), k).low;
        }

        // Translate: a harmonic series of the low end, band-limited to 180 Hz - 3 kHz, so the 808 is heard
        // on phone and laptop speakers that cannot reproduce its fundamental (the ear fills it in).
        // The low band is level-normalised first, so the harmonics are equally rich on quiet tails and loud
        // hits, then shaped (odd harmonics from the soft square, even ones from the rectifier) and put back
        // at the low band's own level.
        float harm = 0.0f;
        if (translate > 0.0f)
        {
            const auto low = svf(lowSplit2, svf(lowSplit1, y, gSplit, 1.4f).low, gSplit, 1.4f).low;
            const auto level = std::abs(low);
            transEnv += (level > transEnv ? transAttack : transRelease) * (level - transEnv);
            const auto xn = low / (transEnv + 1.0e-4f);
            const auto sq = std::tanh(3.0f * xn);
            const auto gen = (sq + 0.6f * (std::abs(sq) - 0.64f)) * transEnv;
            harm = svf(harmHp2, svf(harmHp1, gen, gHarmHp, 1.4f).high, gHarmHp, 1.4f).high;
            harm = svf(harmLp, harm, gHarmLp, 1.4f).low;
            y += translate * 2.4f * harm;
        }

        // Glue: single-knob compressor for an even level from note to note
        if (glue > 0.0f)
        {
            const auto level = std::abs(y);
            compEnv += (level > compEnv ? compAttack : compRelease) * (level - compEnv);
            const auto over = gainToDb(compEnv) - threshold;
            const auto target = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            compGainDb += (target > compGainDb ? grAttack : grRelease) * (target - compGainDb);
            y *= dbToGain(makeup - compGainDb);
        }
        else
        {
            compGainDb = 0.0f;
        }

        // Width: only content above 160 Hz is spread, so the sub stays mono and the mono sum is exact
        auto l = y, r = y;
        if (width > 0.0f)
        {
            const auto high = svf(widthHp2, svf(widthHp, y, gWidth, 1.4f).high, gWidth, 1.4f).high;
            widthDelay[static_cast<std::size_t>(widthPos)] = high;
            const auto delayed = widthDelay[static_cast<std::size_t>((widthPos - widthTaps + static_cast<int>(widthDelay.size())) % static_cast<int>(widthDelay.size()))];
            widthPos = (widthPos + 1) % static_cast<int>(widthDelay.size());
            l += width * 0.5f * delayed;
            r -= width * 0.5f * delayed;
        }

        // Kick Lock duck: 1 ms down to -15 dB, back up over the Duck time
        if (duckFalling && s.duckMs > 0.5f)
        {
            duckGain -= duckFall;
            if (duckGain <= duckFloor) { duckGain = duckFloor; duckFalling = false; }
        }
        else
        {
            duckFalling = false;
            duckGain += duckRise * (1.0f - duckGain);
        }
        l *= duckGain * outGain;
        r *= duckGain * outGain;

        l = clipL.process(l, s.clip, kernel);
        r = clipR.process(r, s.clip, kernel);

        if (metersOn)
        {
            const auto lowForCone = svf(coneLp, 0.5f * (l + r), gCone, 1.4f).low;
            window.peakL = std::max(window.peakL, std::abs(l));
            window.peakR = std::max(window.peakR, std::abs(r));
            window.harmonics = std::max(window.harmonics, std::abs(translate * 1.6f * harm));
            window.lowPeak = std::max(window.lowPeak, std::abs(lowForCone));
            window.gainReductionDb = std::max(window.gainReductionDb, compGainDb);
            window.duck = std::max(window.duck, 1.0f - duckGain);
            const auto kl = kWeight[1].run(kWeight[0].run(l)), kr = kWeight[3].run(kWeight[2].run(r));
            windowSquares += kl * kl + kr * kr;
            const auto slot = scopeWrite.load(std::memory_order_relaxed);
            scope[slot & (scopeSize - 1)] = 0.5f * (l + r);
            scopeWrite.store(slot + 1, std::memory_order_release);
            if (++windowCount >= framePeriod)
            {
                window.envelope = voice.env;
                window.frequencyHz = voice.stage != 0 ? static_cast<float>(hz) : 0.0f;
                window.note = voice.stage != 0 ? semis - punch : -1.0f;
                window.meanSquare = static_cast<float>(windowSquares / windowCount);
                window.kick = kickInFrame;
                publish();
                window = {};
                windowSquares = 0.0;
                windowCount = 0;
                kickInFrame = false;
            }
        }

        // Phone Check: hear the 808 through an approximate device curve (monitoring only; never bounce with it)
        if (s.phoneCheck)
        {
            switch (s.phoneModel)
            {
                case PhoneModel::phone:
                case PhoneModel::laptop:
                {
                    const auto mono = 0.5f * (l + r);
                    auto p = svf(phoneA2, svf(phoneA1, mono, gA, butterworthK).high, gA, butterworthK).high;
                    p = svf(phoneC, p, gC, butterworthK).low * (s.phoneModel == PhoneModel::phone ? 1.4f : 1.2f);
                    l = r = p;
                    break;
                }
                case PhoneModel::earbuds:
                    l = svf(phoneA1, l, gA, butterworthK).high;
                    r = svf(phoneB1, r, gA, butterworthK).high;
                    break;
                case PhoneModel::car:
                    // cabin gain: about +6 dB below 70 Hz
                    l += svf(phoneA1, l, gA, butterworthK).low;
                    r += svf(phoneB1, r, gA, butterworthK).low;
                    l = svf(phoneA2, l, gC, butterworthK).low;
                    r = svf(phoneB2, r, gC, butterworthK).low;
                    break;
            }
        }

        left[i] = l;
        right[i] = r;
        ++sampleCounter;
    }
}

void Engine::publish() noexcept
{
    slots[writeIndex] = window;
    writeIndex = middle.exchange(writeIndex | 4u, std::memory_order_acq_rel) & 3u;
}

bool Engine::readMeterFrame(MeterFrame& frame) noexcept
{
    if ((middle.load(std::memory_order_acquire) & 4u) == 0)
        return false;
    readIndex = middle.exchange(readIndex, std::memory_order_acq_rel) & 3u;
    frame = slots[readIndex];
    return true;
}

int Engine::copyScope(float* destination, int count) const noexcept
{
    count = std::clamp(count, 0, static_cast<int>(scopeSize) / 2);
    const auto end = scopeWrite.load(std::memory_order_acquire);
    const auto start = end - static_cast<std::uint32_t>(count);
    for (int i = 0; i < count; ++i)
        destination[i] = end >= static_cast<std::uint32_t>(count - i)
                         ? scope[(start + static_cast<std::uint32_t>(i)) & (scopeSize - 1)] : 0.0f;
    return count;
}
}
