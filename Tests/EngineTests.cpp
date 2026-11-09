// Epsilon 808 engine tests: tuning, punch, glide, clicks, aliasing, clipper, Split, priority, retrigger,
// Analog, velocity, Kick Lock, Translate, width, Phone Check models, idle, CPU.
// v1.1: hybrid layers (tuning, tracking, glide, Phase Lock, low cut, envelope, mono-safe stereo) and 808 character
// (Snap, Sigh, Vary, Vel > Length, Tone Decay).
#include "Boom808Engine.h"
#include "WaveAnalysis.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <complex>
#include <chrono>
#include <algorithm>
using namespace boom;
static int fails = 0;
#define CHECK(c, ...) do { bool ok_ = (c); std::printf("[%s] ", ok_ ? "PASS" : "FAIL"); std::printf(__VA_ARGS__); std::printf("\n"); if (!ok_) ++fails; } while (0)
static const double PI = 3.141592653589793;

struct Out { std::vector<float> l, r; };
template <typename F>
Out run(double sr, double seconds, Settings s, F events) {
  Engine e; e.prepare(sr); Out o; int n = (int)(sr * seconds); o.l.resize(n); o.r.resize(n);
  for (int p = 0; p < n; p += 64) { int m = std::min(64, n - p); events(e, p); e.render(o.l.data() + p, o.r.data() + p, m, s); }
  return o;
}
// frequency by zero crossings with linear interpolation
double freqOf(const std::vector<float>& x, double sr, int a, int b) {
  double first = -1, last = -1; int count = 0;
  for (int i = a + 1; i < b; ++i) if (x[i - 1] < 0 && x[i] >= 0) { double t = i - 1 + (-x[i - 1]) / (x[i] - x[i - 1]); if (first < 0) first = t; last = t; ++count; }
  return count > 1 ? (count - 1) * sr / (last - first) : 0;
}
double bandEnergy(const std::vector<float>& x, double sr, int a, int n, double lo, double hi) {
  double e = 0; for (int k = (int)(lo * n / sr); k <= (int)(hi * n / sr); ++k) { std::complex<double> acc = 0; for (int i = 0; i < n; ++i) { double w = 0.5 - 0.5 * std::cos(2 * PI * i / n); acc += w * x[a + i] * std::polar(1.0, -2 * PI * k * i / n); } e += std::norm(acc); } return e;
}
// worst inharmonic peak below maxHz relative to the strongest bin, Blackman-Harris, 32768 points from 0.5 s
double inharmonicPeak(const std::vector<float>& x, double f0, double maxHz) {
  const int n = 32768, a = 24000; std::vector<std::complex<double>> b(n);
  for (int i = 0; i < n; ++i) { double t = 2 * PI * i / (n - 1); double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t); b[i] = w * x[a + i]; }
  for (int i = 1, j = 0; i < n; ++i) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(b[i], b[j]); }
  for (int len = 2; len <= n; len <<= 1) { auto wl = std::polar(1.0, -2 * PI / len); for (int i = 0; i < n; i += len) { std::complex<double> w = 1; for (int j = 0; j < len / 2; ++j) { auto u = b[i + j], v = b[i + j + len / 2] * w; b[i + j] = u + v; b[i + j + len / 2] = u - v; w *= wl; } } }
  double pk = 0, worst = 1e-30;
  for (int k = 1; k <= n / 2; ++k) { double f = k * 48000.0 / n, p = std::norm(b[k]); pk = std::max(pk, p); double d = std::abs(f - std::round(f / f0) * f0); if (f > 15 && f < maxHz && d > 4 * 48000.0 / n + 1.5) worst = std::max(worst, p); }
  return 10 * std::log10(worst / pk); }
Settings clean() { Settings s; s.punchSemitones = 0; s.tone = 0; s.sub = 0; s.click = 0; s.driveDb = 0; s.driveMix = 0; s.glue = 0; s.clip = false; s.sustain = 1; s.lengthMs = 8000; s.glideMs = 0; return s; }


// ---- v1.1 hybrid helpers: a synthetic "808 sample" (decaying sine with a pitch drop, optional stereo side)
struct TestSample { std::vector<float> mid, side; LayerSample ls; };
TestSample makeSample(double hz, double phase, double seconds = 3.0, bool stereo = false, double rate = 44100) {
  TestSample t; const int n = (int)(seconds * rate); t.mid.resize(n); t.side.resize(n);
  double ph = phase;
  for (int i = 0; i < n; ++i) { double tt = i / rate; double f = hz * std::pow(2.0, 7.0 * std::exp(-tt / 0.02) / 12.0);
    ph += f / rate; t.mid[i] = (float)(0.9 * std::exp(-tt / 0.9) * std::sin(2 * PI * ph));
    t.side[i] = stereo ? (float)(0.3 * std::exp(-tt / 0.9) * std::sin(2 * PI * ph * 9.0)) : 0.0f; }
  t.ls.mid = t.mid.data(); t.ls.side = stereo ? t.side.data() : nullptr; t.ls.length = n; t.ls.rate = rate; t.ls.rootNote = 24; t.ls.rootHz = (float)hz;
  return t; }
Settings layered(bool engineOn) { Settings s = clean(); s.layers[0].on = true; s.layers[0].levelDb = 0; s.engineDb = engineOn ? 0.0f : -60.0f; return s; }
template <typename F>
Out runL(double sr, double seconds, Settings s, const LayerSample* a, F events, const LayerSample* b = nullptr) {
  Engine e; e.prepare(sr); e.setLayerSample(0, a); e.setLayerSample(1, b); Out o; int n = (int)(sr * seconds); o.l.resize(n); o.r.resize(n);
  for (int p = 0; p < n; p += 64) { int m = std::min(64, n - p); events(e, p); e.render(o.l.data() + p, o.r.data() + p, m, s); }
  return o; }
double rms(const std::vector<float>& x, int a, int b) { double e = 0; for (int i = a; i < b; ++i) e += x[i] * x[i]; return std::sqrt(e / std::max(1, b - a)); }
std::vector<float> lowpassed(const std::vector<float>& x, double sr, double fc) {   // 4 one-pole passes
  std::vector<float> y = x; double a = std::exp(-2 * PI * fc / sr);
  for (int pass = 0; pass < 4; ++pass) { double z = 0; for (auto& v : y) { z = (1 - a) * v + a * z; v = (float)z; } }
  return y; }

void hybridTests() {
  const double sr = 48000;
  // 1. tuning: a sample 31 cents sharp of C1 is auto-tuned; it follows the keys
  { auto smp = makeSample(33.29, 0.0);
    for (int note : { 24, 31, 36 }) {
      auto o = runL(sr, 1.2, layered(false), &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(note, 1.0f); });
      const double want = 440 * std::pow(2.0, (note - 69) / 12.0), got = freqOf(o.l, sr, (int)(0.3 * sr), (int)(1.1 * sr));
      CHECK(std::abs(1200 * std::log2(got / want)) < 2.0, "layer tuning: a +31 cent sample plays note %d at %.3f Hz (%+.2f cents)", note, got, 1200 * std::log2(got / want)); } }
  // 2. the layer follows a glide
  { auto smp = makeSample(32.7032, 0.0); Settings s = layered(false); s.glideMs = 100;
    auto o = runL(sr, 1.6, s, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 24000) e.noteOn(43, 1.0f); });
    double end = freqOf(o.l, sr, 24000 + 9600, 24000 + 30000);
    CHECK(std::abs(1200 * std::log2(end / 97.999)) < 3.0, "layer follows the glide C2 -> G2: %.2f Hz after the slide", end); }
  // 3. Phase Lock: whatever phase the sample's fundamental has, engine + layer add up
  { double worstOn = 1e9, worstOff = 1e9;
    for (int k = 0; k < 12; ++k) {
      auto smp = makeSample(32.7032, k / 12.0);
      double lvl[2];
      for (int lock = 0; lock < 2; ++lock) {
        Settings s = layered(true); s.phaseLock = lock == 1; s.punchSemitones = 10; s.punchMs = 40; s.lengthMs = 2600;
        auto both = runL(sr, 0.5, s, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
        Settings se = s; se.layers[0].on = false;                                  // engine alone (same phase it would start on)
        Settings sl = s; sl.engineDb = -60;                                        // layer alone
        auto eng = runL(sr, 0.5, se, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
        auto lay = runL(sr, 0.5, sl, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
        const int a = (int)(0.06 * sr), b = (int)(0.4 * sr);
        auto lb = lowpassed(both.l, sr, 80), le = lowpassed(eng.l, sr, 80), ll = lowpassed(lay.l, sr, 80);
        lvl[lock] = 20 * std::log10(rms(lb, a, b) / (rms(le, a, b) + rms(ll, a, b)));   // 0 dB = perfectly in phase
      }
      worstOff = std::min(worstOff, lvl[0]); worstOn = std::min(worstOn, lvl[1]); }
    CHECK(worstOn > -1.0 && worstOff < -6.0, "Phase Lock: engine + layer low end within %.2f dB of a perfect sum at every sample phase (without it: down to %.1f dB)", worstOn, worstOff); }
  // 4. low cut: at 120 Hz the layer leaves the sub to the engine
  { auto smp = makeSample(32.7032, 0.0); Settings s = layered(false); s.layers[0].lowCutHz = 120;
    Settings full = layered(false);
    auto cut = runL(sr, 0.8, s, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
    auto all = runL(sr, 0.8, full, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
    const int a = (int)(0.15 * sr), b = (int)(0.7 * sr);
    double d = 20 * std::log10(rms(cut.l, a, b) / rms(all.l, a, b));
    CHECK(d < -30, "layer Low Cut at 120 Hz takes a 33 Hz layer down %.1f dB", d); }
  // 5. layer envelope: attack, hold, decay to sustain, release
  { auto smp = makeSample(32.7032, 0.0, 6.0); for (auto& v : smp.mid) v = (float)(0.8 * std::sin(2 * PI * 32.7032 * (&v - smp.mid.data()) / 44100.0));
    Settings s = layered(false); auto& L = s.layers[0]; L.attackMs = 100; L.holdMs = 100; L.decayMs = 400; L.sustain = 0.25f; L.releaseMs = 300;
    s.sustain = 1; s.lengthMs = 8000;
    auto o = runL(sr, 3.0, s, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 96000) e.noteOff(36); });
    auto win = [&](double t0) { return rms(o.l, (int)(t0 * sr), (int)((t0 + 0.03) * sr)); };
    const double peak = win(0.15), early = win(0.02), sus = win(1.5), rel = win(2.5);
    CHECK(early < 0.45 * peak && std::abs(sus / peak - 0.25) < 0.04 && rel < 0.01 * peak,
          "layer envelope: %.0f%% at 20 ms (attack), sustain %.0f%% of peak, %.1f dB 500 ms after release", 100 * early / peak, 100 * sus / peak, 20 * std::log10(rel / peak)); }
  // 6. stereo layers stay mono-safe: L + R is exactly the mono signal
  { auto smp = makeSample(32.7032, 0.0, 3.0, true); Settings s = layered(true); s.layers[0].width = 1.0f;
    auto o = runL(sr, 1.0, s, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
    Settings m = s; m.layers[0].width = 0.0f;
    auto mono = runL(sr, 1.0, m, &smp.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); });
    double err = 0, sideE = 0, sideLow = 0; auto sd = std::vector<float>(o.l.size());
    for (size_t i = 0; i < o.l.size(); ++i) { err = std::max(err, (double)std::abs(0.5f * (o.l[i] + o.r[i]) - mono.l[i])); sd[i] = 0.5f * (o.l[i] - o.r[i]); sideE += sd[i] * sd[i]; }
    auto sdl = lowpassed(sd, sr, 60); for (auto v : sdl) sideLow += v * v;
    CHECK(err < 1e-5 && sideE > 1e-3 && 10 * std::log10(sideLow / sideE) < -25, "layer stereo: mono sum off by %.1e, side below 60 Hz %.1f dB under the side", err, 10 * std::log10(sideLow / sideE)); }
  // 7. two layers + engine, retrigger with layers: smooth and finite
  { auto a = makeSample(32.7032, 0.3, 3.0, true), b = makeSample(65.406, 0.0, 3.0, true); Settings s = layered(true);
    s.layers[1].on = true; s.layers[1].lowCutHz = 150; s.layers[1].attackMs = 200; s.retrigger = true; s.clip = true; s.glideMs = 80; s.glideMode = GlideMode::always;
    auto o = runL(sr, 2.0, s, &a.ls, [&](Engine& e, int p) { if (p == 0) e.noteOn(24, 1.0f); if (p == 30016) e.noteOn(27, 0.8f); if (p == 60032) e.noteOff(24); if (p == 60032) e.noteOff(27); }, &b.ls);
    bool finite = true; float pk = 0; for (auto v : o.l) { finite = finite && std::isfinite(v); pk = std::max(pk, std::abs(v)); }
    double tailLevel = rms(o.l, (int)(1.8 * sr), (int)(2.0 * sr));
    CHECK(finite && pk <= 0.93f && tailLevel < 1e-4, "engine + two layers with slides and retrigger (Clip on): peak %.3f, silent %.1e after release", pk, tailLevel); }
  // 8. Snap: the first 5 ms sit well above the note (the circuit's attack shift), then it settles
  // A cycle at the snapped pitch is longer than the snap itself, so measure what it does to the phase:
  // the extra cycles it adds must match the circuit shape (+19 st held 5 ms, then a 1.5 ms fall).
  { auto phaseAt = [&](float snap) { Settings s = clean(); s.snap = snap;
      auto o = run(sr, 0.5, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
      std::complex<double> acc = 0; const int a = (int)(0.2 * sr), n = (int)(0.25 * sr);
      for (int i = 0; i < n; ++i) acc += (double)o.l[a + i] * std::polar(1.0, -2 * PI * 65.406 * (a + i) / sr);
      return std::arg(acc) / (2 * PI); };
    double want = 0; for (int k = 0; k < 20000; ++k) { double t = (k + 0.5) * 0.04 / 20000; double sh = t < 0.005 ? 1 : std::exp(-(t - 0.005) / 0.0015);
      want += 65.406 * (std::pow(2.0, 19 * sh / 12) - 1) * 0.04 / 20000; }
    double got = phaseAt(1.0f) - phaseAt(0.0f); got -= std::floor(got); double wf = want - std::floor(want);
    double err = std::min(std::abs(got - wf), 1 - std::abs(got - wf));
    auto o = run(sr, 0.5, [] { Settings s = clean(); s.snap = 1.0f; return s; }(), [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double late = freqOf(o.l, sr, 6000, 20000);
    CHECK(err < 0.03 && std::abs(1200 * std::log2(late / 65.406)) < 1.0, "Snap: adds %.2f cycles in the attack (circuit shape: %.2f), then sits at %.2f Hz", want - wf + got, want, late); }
  // 9. Sigh: a slow settle of up to 3 semitones over ~300 ms
  { Settings s = clean(); s.sigh = 1.0f;
    auto o = run(sr, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double f1 = freqOf(o.l, sr, (int)(0.03 * sr), (int)(0.08 * sr)), f2 = freqOf(o.l, sr, (int)(0.15 * sr), (int)(0.25 * sr)), f3 = freqOf(o.l, sr, (int)(0.6 * sr), (int)(0.95 * sr));
    CHECK(f1 > f2 && f2 > f3 && 1200 * std::log2(f1 / f3) > 150 && std::abs(1200 * std::log2(f3 / 65.406)) < 2, "Sigh: %.1f -> %.1f -> %.2f Hz over a second", f1, f2, f3); }
  // 10. Vary: no two hits identical, but within +-1.5 dB; Vary 0: identical
  { auto hits = [&](float vary) { Settings s = clean(); s.vary = vary; s.sustain = 0; s.lengthMs = 300;
      auto o = run(sr, 4.0, s, [&](Engine& e, int p) { for (int k = 0; k < 8; ++k) { if (p == k * 24000) e.noteOn(36, 1.0f); if (p == k * 24000 + 12032) e.noteOff(36); } });
      std::vector<double> lv; for (int k = 0; k < 8; ++k) lv.push_back(20 * std::log10(rms(o.l, k * 24000 + 200, k * 24000 + 4000))); return lv; };
    auto v0 = hits(0), v1 = hits(1);
    double spread0 = *std::max_element(v0.begin(), v0.end()) - *std::min_element(v0.begin(), v0.end());
    double spread1 = *std::max_element(v1.begin(), v1.end()) - *std::min_element(v1.begin(), v1.end());
    CHECK(spread0 < 0.01 && spread1 > 0.3 && spread1 < 4.5, "Vary: 8 hits spread %.2f dB at 100%% (%.3f dB at 0%%)", spread1, spread0); }
  // 11. Vel > Length: soft hits ring shorter, like the 808's accent
  { auto len = [&](float vel) { Settings s = clean(); s.sustain = 0; s.lengthMs = 1500; s.velLength = 1.0f;
      auto o = run(sr, 2.5, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, vel); });
      double pk = rms(o.l, 500, 2000); for (int i = 2000; i < (int)o.l.size() - 1000; i += 480) if (rms(o.l, i, i + 960) < pk * 0.01) return i / sr; return 2.5; };
    double hard = len(1.0f), soft = len(0.3f);
    CHECK(soft < 0.6 * hard, "Vel > Length: -40 dB after %.2f s at full velocity, %.2f s at 30%%", hard, soft); }
  // 12. Tone Decay: the harmonics fade before the body
  { auto ratio = [&](float td) { Settings s = clean(); s.tone = 0.8f; s.toneDecay = td; s.sustain = 0; s.lengthMs = 3000;
      auto o = run(sr, 2.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
      const int n = 4800; auto at = [&](double t) { int a = (int)(t * sr); return 10 * std::log10(bandEnergy(o.l, sr, a, n, 120, 210) / bandEnergy(o.l, sr, a, n, 55, 75)); };
      return at(1.2) - at(0.05); };
    double off = ratio(0), on = ratio(1);
    CHECK(on < off - 4, "Tone Decay: 2nd harmonic vs body changes %.1f dB over the tail (%.1f dB with Tone Decay off)", on, off); }
}

// ---- v1.1 Blend: a sample's waves merged into the oscillator
std::vector<float> harmonicSample(double hz, double seconds, double rate) {   // bright at the start, darker later
  std::vector<float> x((size_t)(seconds * rate)); double ph = 0;
  for (size_t i = 0; i < x.size(); ++i) { double t = i / rate; ph += hz / rate; double b = std::exp(-t / 0.5);
    x[i] = (float)(0.8 * std::exp(-t / 1.5) * (std::sin(2 * PI * ph) + 0.5 * b * std::sin(4 * PI * ph + 0.7) + 0.3 * b * std::sin(6 * PI * ph + 1.9))); }
  return x; }
double harmonicLevel(const std::vector<float>& x, double sr, int a, int n, double f) {
  std::complex<double> acc = 0; for (int i = 0; i < n; ++i) { double w = 0.5 - 0.5 * std::cos(2 * PI * i / n); acc += w * (double)x[a + i] * std::polar(1.0, -2 * PI * f * (a + i) / sr); }
  return std::abs(acc); }
double harmonicPhase(const std::vector<float>& x, double sr, int a, int n, double f) {
  std::complex<double> acc = 0; for (int i = 0; i < n; ++i) acc += (double)x[a + i] * std::polar(1.0, -2 * PI * f * (a + i) / sr); return std::arg(acc); }
void blendTests() {
  const double sr = 48000, rate = 44100;
  auto smp = harmonicSample(32.7032, 3.0, rate);
  auto tables = wave::analyse(smp.data(), (int)smp.size(), rate, 32.7032);
  MorphSource ms; ms.tables = tables.data(); ms.frames = (int)tables.size() / wave::tableSize; ms.size = wave::tableSize; ms.hopSeconds = wave::hopSeconds;
  // 1. analysis: the first wave holds the sample's 2nd/3rd harmonic levels; later waves are darker
  { std::vector<float> one(tables.begin(), tables.begin() + wave::tableSize), late(tables.begin() + 60 * wave::tableSize, tables.begin() + 61 * wave::tableSize);
    auto h = [&](const std::vector<float>& t, int k) { std::complex<double> a = 0; for (int i = 0; i < wave::tableSize; ++i) a += (double)t[i] * std::polar(1.0, -2 * PI * k * i / wave::tableSize); return std::abs(a); };
    double r2 = 20 * std::log10(h(one, 2) / h(one, 1)), r3 = 20 * std::log10(h(one, 3) / h(one, 1)), r2late = 20 * std::log10(h(late, 2) / h(late, 1));
    CHECK(std::abs(r2 - 20 * std::log10(0.5 * std::exp(-0.046))) < 1.5 && std::abs(r3 - 20 * std::log10(0.3 * std::exp(-0.046))) < 1.5 && r2late < r2 - 15,
          "wave analysis: %d waves; 2nd/3rd harmonic %.1f / %.1f dB at the start (sample: -6.4 / -10.9), 2nd %.1f dB at 1.2 s", ms.frames, r2, r3, r2late); }
  auto run = [&](float morph, int note) { Settings s = clean(); s.morph = morph; s.lengthMs = 8000; s.sustain = 1;
    Engine e; e.prepare(sr); e.setMorphSource(&ms); Out o; int n = (int)(sr * 1.4); o.l.resize(n); o.r.resize(n);
    for (int p = 0; p < n; p += 64) { if (p == 0) e.noteOn(note, 1.0f); e.render(o.l.data() + p, o.r.data() + p, std::min(64, n - p), s); } return o; };
  // 2. Blend 100%: plays the sample's wave at the engine's pitch, in tune, with its harmonic recipe
  { auto o = run(1.0f, 36); const int a = (int)(0.03 * sr), n = 8192; double f = freqOf(o.l, sr, (int)(0.3 * sr), (int)(1.3 * sr));
    double r2 = 20 * std::log10(harmonicLevel(o.l, sr, a, n, 2 * 65.406) / harmonicLevel(o.l, sr, a, n, 65.406));
    CHECK(std::abs(1200 * std::log2(f / 65.406)) < 1.0 && std::abs(r2 + 7.0) < 2.5, "Blend 100%%: plays at %.3f Hz with the sample's 2nd harmonic at %.1f dB", f, r2); }
  // 3. the sample's wave and the sine share the fundamental's phase: blending never cancels
  { auto a = run(0.0f, 24), b = run(1.0f, 24), m = run(0.5f, 24); const int s0 = (int)(0.4 * sr), n = 24000;
    double dph = std::remainder(harmonicPhase(a.l, sr, s0, n, 32.7032) - harmonicPhase(b.l, sr, s0, n, 32.7032), 2 * PI) * 180 / PI;
    double la = harmonicLevel(a.l, sr, s0, n, 32.7032), lm = harmonicLevel(m.l, sr, s0, n, 32.7032), lb = harmonicLevel(b.l, sr, s0, n, 32.7032);
    CHECK(std::abs(dph) < 10 && lm > 0.97 * std::min(la, lb), "Blend 50%%: fundamental %.1f deg from the sine's, level %.2f of the smaller source (no cancellation)", dph, lm / std::min(la, lb)); }
  // 4. Blend 0% is exactly the plain engine
  { auto a = run(0.0f, 36); Settings s = clean(); s.lengthMs = 8000; s.sustain = 1;
    auto b = runL(sr, 1.4, s, nullptr, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double d = 0; for (size_t i = 0; i < a.l.size(); ++i) d = std::max(d, (double)std::abs(a.l[i] - b.l[i]));
    CHECK(d < 1e-6, "Blend 0%%: identical to the engine alone (largest difference %.1e)", d); }
}

int main() {
  for (double sr : { 44100.0, 48000.0, 96000.0 }) {
    for (int note : { 24, 36, 48 }) {
      auto o = run(sr, 1.0, clean(), [&](Engine& e, int p) { if (p == 0) e.noteOn(note, 1.0f); });
      const double want = 440 * std::pow(2.0, (note - 69) / 12.0), got = freqOf(o.l, sr, (int)(0.3 * sr), (int)(0.95 * sr));
      const double cents = 1200 * std::log2(got / want);
      CHECK(std::abs(cents) < 0.5, "tuning %.0f Hz note %d: %.3f Hz, %+.3f cents", sr, note, got, cents);
    }
  }
  { Settings s = clean(); s.punchSemitones = 12; s.punchMs = 60;
    auto o = run(48000, 0.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double early = freqOf(o.l, 48000, 50, 1500), late = freqOf(o.l, 48000, 12000, 28000);
    CHECK(early > late * 1.25 && std::abs(1200 * std::log2(late / 65.406)) < 1.0, "punch: starts at %.1f Hz, settles at %.2f Hz", early, late); }
  { Settings s = clean(); s.glideMs = 100;
    auto o = run(48000, 1.2, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 24000) e.noteOn(48, 1.0f); });
    double mid = freqOf(o.l, 48000, 24000 + 1000, 24000 + 2600), end = freqOf(o.l, 48000, 24000 + 9600, 24000 + 24000);
    CHECK(mid > 70 && mid < 128 && std::abs(1200 * std::log2(end / 130.81)) < 1.0, "legato glide C2->C3 in 100 ms: %.1f Hz midway, %.2f Hz after", mid, end); }
  { Settings s = clean(); s.glideMs = 100;
    auto o = run(48000, 1.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 24000) e.noteOn(48, 1.0f); if (p == 48000) e.noteOff(48); });
    double back = freqOf(o.l, 48000, 48000 + 9600, 48000 + 24000);
    CHECK(std::abs(1200 * std::log2(back / 65.406)) < 1.0, "releasing the top note glides back to the held one: %.2f Hz", back); }
  // clicks: retrigger mid-note and release with the shortest settings
  { Settings s = clean(); s.sustain = 0.8f; s.releaseMs = 5; s.attackMs = 0.5f;
    auto o = run(48000, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 12000) { e.noteOff(36); e.noteOn(41, 1.0f); } if (p == 30016) e.noteOff(41); });
    float maxStep = 0, steady = 0;
    for (int i = 1; i < (int)o.l.size(); ++i) { float d = std::abs(o.l[i] - o.l[i - 1]); if ((i > 11990 && i < 12400) || (i > 30000 && i < 30600)) maxStep = std::max(maxStep, d); if (i > 6000 && i < 11000) steady = std::max(steady, d); }
    CHECK(maxStep < 2.5f * steady, "retrigger and 5 ms release stay smooth: largest step %.4f vs steady-state %.4f", maxStep, steady); }
  // aliasing: every drive mode at full drive across the 808 range (C1-C3), and C4 at +24 dB.
  // Measured with a Blackman-Harris window; an ideal band-limited square reads about -89 dB here.
  {
    const char* names[] = { "Tape", "Tube", "Hard", "Fold" };
    double worstLow = -200, worstC4 = -200;
    for (int mode = 0; mode < 4; ++mode)
      for (int note : { 24, 36, 48, 60 }) {
        Settings s = clean(); s.driveDb = note == 60 ? 24 : 36; s.driveMix = 100; s.driveMode = (DriveMode) mode;
        auto o = run(48000, 1.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(note, 1.0f); });
        const int a = 24000, n = 32768; const double f0 = 440 * std::pow(2.0, (note - 69) / 12.0);
        std::vector<std::complex<double>> X(n / 2 + 1);
        // direct DFT is slow at this size; use a simple radix-2 FFT
        std::vector<std::complex<double>> buf(n);
        for (int i = 0; i < n; ++i) { double t = 2 * PI * i / (n - 1); double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t); buf[i] = w * o.l[a + i]; }
        for (int i = 1, j = 0; i < n; ++i) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(buf[i], buf[j]); }
        for (int len = 2; len <= n; len <<= 1) { auto wl = std::polar(1.0, -2 * PI / len); for (int i = 0; i < n; i += len) { std::complex<double> w = 1; for (int j = 0; j < len / 2; ++j) { auto u = buf[i + j], v = buf[i + j + len / 2] * w; buf[i + j] = u + v; buf[i + j + len / 2] = u - v; w *= wl; } } }
        double total = 0, inharmonic = 0;
        for (int k = 1; k <= n / 2; ++k) { double f = k * 48000.0 / n, p = std::norm(buf[k]); total += p; double nearest = std::round(f / f0) * f0; if (f > 15 && std::abs(f - nearest) > 4 * 48000.0 / n * 1.0 + 1.5) inharmonic += p; }
        double db = 10 * std::log10(std::max(1e-30, inharmonic) / total);
        if (note == 60) worstC4 = std::max(worstC4, db); else worstLow = std::max(worstLow, db);
        if (db > -60) std::printf("   %s note %d: %.1f dB\n", names[mode], note, db);
      }
    CHECK(worstLow < -65 && worstC4 < -50, "aliasing, all drive modes: worst %.1f dB in C1-C3 at +36 dB, %.1f dB at C4 +24 dB", worstLow, worstC4);
  }
  // Translate adds audible harmonics for small speakers
  { Settings s = clean(); auto dry = run(48000, 0.8, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    s.translate = 100; auto wet = run(48000, 0.8, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    double d = bandEnergy(dry.l, 48000, 9600, 8192, 200, 2000), w = bandEnergy(wet.l, 48000, 9600, 8192, 200, 2000);
    CHECK(10 * std::log10(w / d) > 20, "Translate on a 44 Hz 808 adds %.1f dB in 200 Hz - 2 kHz", 10 * std::log10(w / d)); }
  // width: sum of L+R equals the unwidened signal; the sub stays mono
  { Settings s = clean(); s.tone = 1; s.driveDb = 12; s.driveMix = 100;
    auto mono = run(48000, 0.5, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    s.width = 100; auto wide = run(48000, 0.5, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    float worst = 0; for (size_t i = 0; i < mono.l.size(); ++i) worst = std::max(worst, std::abs(0.5f * (wide.l[i] + wide.r[i]) - mono.l[i]));
    std::vector<float> side(wide.l.size()); for (size_t i = 0; i < side.size(); ++i) side[i] = wide.l[i] - wide.r[i];
    double sideLow = bandEnergy(side, 48000, 9600, 8192, 20, 90), monoLow = bandEnergy(mono.l, 48000, 9600, 8192, 20, 90);
    CHECK(worst < 1e-5f && 10 * std::log10(sideLow / monoLow) < -30, "Width at 100%%: mono sum off by %.1e, sub in the sides %.1f dB below the sub", worst, 10 * std::log10(sideLow / monoLow)); }
  // silence after release, and nothing at all before the first note
  { Settings s; auto o = run(48000, 4.0, s, [&](Engine& e, int p) { if (p == 4800) e.noteOn(36, 1.0f); if (p == 9600) e.noteOff(36); });
    bool before = std::all_of(o.l.begin(), o.l.begin() + 4800, [](float v) { return v == 0.0f; });
    float lateMax = 0; for (size_t i = (size_t)(3.5 * 48000); i < o.l.size(); ++i) lateMax = std::max(lateMax, std::abs(o.l[i]));
    bool finite = std::all_of(o.l.begin(), o.l.end(), [](float v) { return std::isfinite(v); });
    CHECK(before && lateMax < 1e-4f && finite, "silent before the first note, decays to %.1e after release, all finite", lateMax); }
  // Phone Check removes the sub
  { Settings s = clean(); auto a = run(48000, 0.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    s.phoneCheck = true; auto b = run(48000, 0.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    double drop = 10 * std::log10(bandEnergy(b.l, 48000, 9600, 8192, 30, 60) / bandEnergy(a.l, 48000, 9600, 8192, 30, 60));
    CHECK(drop < -25, "Phone Check takes the 44 Hz fundamental down %.1f dB", drop); }
  // Clip keeps peaks at or below 0 dBFS even when pushed
  { Settings s; s.outputDb = 12; s.driveDb = 20; s.clip = true;
    auto o = run(48000, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    float peak = 0; for (float v : o.l) peak = std::max(peak, std::abs(v));
    CHECK(peak <= 1.0f, "Clip holds peaks at %.3f with +12 dB output and +20 dB drive", peak); }

  // Output clipper, pushed +6 dB by a fully driven 808: worst inharmonic peak below 16 kHz vs the strongest harmonic
  {
    double worstLow = -200, worstC4 = -200;
    for (int mode = 0; mode < 4; ++mode)
      for (int note : { 24, 36, 48, 60 }) {
        Settings s = clean(); s.driveDb = 36; s.driveMix = 100; s.driveMode = (DriveMode) mode; s.clip = true; s.outputDb = 6;
        auto o = run(48000, 1.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(note, 1.0f); });
        const double db = inharmonicPeak(o.l, 440 * std::pow(2.0, (note - 69) / 12.0), 16000);
        (note == 60 ? worstC4 : worstLow) = std::max(note == 60 ? worstC4 : worstLow, db);
      }
    CHECK(worstLow < -80 && worstC4 < -70, "clipper aliasing (2x + ADAA): worst %.1f dB in C1-C3, %.1f dB at C4, +36 dB drive and +6 dB into Clip", worstLow, worstC4);
  }
  // Split: with no drive it changes nothing; with heavy drive the sub below the split matches the clean 808
  { Settings s = clean(); s.tone = 0.5f;
    auto plain = run(48000, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    s.splitHz = 120; auto split = run(48000, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    double flat = 10 * std::log10(bandEnergy(split.l, 48000, 24000, 8192, 20, 2000) / bandEnergy(plain.l, 48000, 24000, 8192, 20, 2000));
    Settings d = s; d.driveDb = 30; d.driveMix = 100; d.driveMode = DriveMode::hard; d.splitHz = 0;
    auto driven = run(48000, 1.0, d, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    d.splitHz = 120; auto drivenSplit = run(48000, 1.0, d, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    const double clean44 = bandEnergy(plain.l, 48000, 24000, 8192, 30, 60);
    double noSplit = 10 * std::log10(bandEnergy(driven.l, 48000, 24000, 8192, 30, 60) / clean44);
    double withSplit = 10 * std::log10(bandEnergy(drivenSplit.l, 48000, 24000, 8192, 30, 60) / clean44);
    double hi = 10 * std::log10(bandEnergy(drivenSplit.l, 48000, 24000, 8192, 300, 3000) / bandEnergy(plain.l, 48000, 24000, 8192, 300, 3000));
    CHECK(std::abs(flat) < 0.1 && std::abs(withSplit) < 1.0 && hi > 10,
          "Split at 120 Hz: %.2f dB change with no drive; under +30 dB Hard drive the 44 Hz sub is %+.1f dB vs clean (%+.1f dB without Split), harmonics +%.0f dB",
          flat, withSplit, noSplit, hi); }
  // Note priority: Low keeps the lowest held note, High the highest, Last follows the newest
  { auto pitchAfter = [&](Priority pr, int first, int second) {
      Engine e; e.prepare(48000); e.setPriority(pr); Settings s = clean(); std::vector<float> l(48000), r(48000);
      for (int p = 0; p < 48000; p += 64) { if (p == 0) e.noteOn(first, 1); if (p == 9600) e.noteOn(second, 1); e.render(l.data() + p, r.data() + p, 64, s); }
      return freqOf(l, 48000, 24000, 47000); };
    double low = pitchAfter(Priority::low, 36, 43), high = pitchAfter(Priority::high, 43, 36), last = pitchAfter(Priority::last, 36, 43);
    CHECK(std::abs(low - 65.41) < 0.2 && std::abs(high - 98.0) < 0.2 && std::abs(last - 98.0) < 0.2,
          "note priority: Low stays on %.2f Hz, High stays on %.2f Hz, Last moves to %.2f Hz", low, high, last); }
  // Retrigger: a legato slide gets a fresh punch
  { auto early = [&](bool retrig) { Settings s = clean(); s.punchSemitones = 12; s.punchMs = 60; s.glideMs = 100; s.retrigger = retrig;
      auto o = run(48000, 1.0, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); if (p == 24000) e.noteOn(48, 1.0f); });
      return freqOf(o.l, 48000, 24000, 24000 + 960); };
    double plain = early(false), fresh = early(true);
    CHECK(fresh > 1.3 * plain, "Retrigger: first 20 ms of a slide average %.0f Hz with a fresh punch vs %.0f Hz without", fresh, plain); }
  // Analog: the tail droops (15-40 cents by the end); off means exact
  { Settings s = clean(); s.sustain = 0; s.lengthMs = 2000; s.analog = 1;
    auto o = run(48000, 1.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double cents = 1200 * std::log2(freqOf(o.l, 48000, 48000, 72000) / 65.406);
    CHECK(cents < -15 && cents > -40, "Analog at 100%%: tail droops %.1f cents", cents); }
  // Velocity to Punch
  { Settings s = clean(); s.punchSemitones = 12; s.punchMs = 60; s.velPunch = 1;
    auto soft = run(48000, 0.3, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 0.2f); });
    auto hard = run(48000, 0.3, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(36, 1.0f); });
    double fs = freqOf(soft.l, 48000, 50, 1500), fh = freqOf(hard.l, 48000, 50, 1500);
    CHECK(fh > fs * 1.15, "velocity to Punch: soft hit starts at %.1f Hz, hard hit at %.1f Hz", fs, fh); }
  // Kick Lock: a held 55 Hz 808, half a cycle out of phase with a 55 Hz kick. Locked, they line up and the 808 ducks.
  { const double sr = 48000; const int n = (int)(sr * 1.0), kickAt = 24000;   // 27.5 cycles of 55 Hz: the kick lands half a cycle out of phase
    std::vector<float> kick(n, 0.0f);
    for (int i = kickAt; i < n; ++i) { double t = (i - kickAt) / sr; kick[i] = (float)(0.8 * std::sin(2 * PI * 55 * t) * std::exp(-t / 0.25)); }
    auto runKick = [&](bool lock, KickMode mode) {
      Settings s = clean(); s.kickLock = lock; s.kickMode = mode; s.duckMs = 60;
      Engine e; e.prepare(sr); Out o; o.l.resize(n); o.r.resize(n);
      for (int p = 0; p < n; p += 64) { if (p == 0) e.noteOn(33, 1.0f); e.render(o.l.data() + p, o.r.data() + p, 64, s, kick.data() + p); }
      return o; };
    auto corr = [&](const Out& o, int delay, int from, int to) { double xy = 0, xx = 0, yy = 0;
      for (int i = from; i < to; ++i) { double a = o.l[i + delay], b = kick[i]; xy += a * b; xx += a * a; yy += b * b; } return xy / std::sqrt(xx * yy); };
    const int la = Engine::lookaheadFor([] { Settings t; t.kickLock = true; return t; }(), sr), lat = Engine::clipLatency;
    auto off = runKick(false, KickMode::exact), exact = runKick(true, KickMode::exact), live = runKick(true, KickMode::live);
    double cOff = corr(off, lat, kickAt + 4800, kickAt + 14400), cExact = corr(exact, la + lat, kickAt + 4800, kickAt + 14400), cLive = corr(live, lat, kickAt + 4800, kickAt + 14400);
    float before = 0, dip = 1e9;
    for (int i = kickAt - 2400; i < kickAt - 200; ++i) before = std::max(before, std::abs(exact.l[i + la + lat]));
    for (int i = kickAt + 100; i < kickAt + 1400; i += 1) { float m = 0; for (int j = 0; j < 436; ++j) m = std::max(m, std::abs(exact.l[i + j + la + lat])); dip = std::min(dip, m); }
    double duckDb = 20 * std::log10(dip / before);
    CHECK(cOff < -0.9 && cExact > 0.9 && cLive > 0.85 && duckDb < -8,
          "Kick Lock: 808 vs kick correlation %.2f off, %.2f Exact (%d-sample lookahead), %.2f Live; 808 level in the 10 ms after the kick %.1f dB",
          cOff, cExact, la, cLive, duckDb); }
  // Phone Check models
  { Settings s = clean(); auto ref = run(48000, 0.6, s, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
    auto bandDb = [&](PhoneModel m) { Settings t = s; t.phoneCheck = true; t.phoneModel = m;
      auto o = run(48000, 0.6, t, [&](Engine& e, int p) { if (p == 0) e.noteOn(29, 1.0f); });
      return 10 * std::log10(bandEnergy(o.l, 48000, 9600, 8192, 30, 60) / bandEnergy(ref.l, 48000, 9600, 8192, 30, 60)); };
    double phone = bandDb(PhoneModel::phone), laptop = bandDb(PhoneModel::laptop), buds = bandDb(PhoneModel::earbuds), car = bandDb(PhoneModel::car);
    CHECK(phone < -40 && laptop < -25 && buds < -1 && buds > -12 && car > 3,
          "Phone Check at 44 Hz: Phone %.1f dB, Laptop %.1f dB, Earbuds %.1f dB, Car %+.1f dB", phone, laptop, buds, car); }
  hybridTests();
  blendTests();
  // CPU: a busy 808 line, all effects on
  { Settings s; s.translate = 60; s.glue = 60; s.width = 50; s.cutoffHz = 3000; s.resonance = 0.3f; s.driveDb = 18;
    Engine e; e.prepare(48000); std::vector<float> l(256), r(256); const int blocks = 48000 * 20 / 256;
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) { if (b % 47 == 0) e.noteOn(30 + (b / 47) % 12, 0.9f); if (b % 47 == 30) e.noteOff(30 + (b / 47) % 12); e.render(l.data(), r.data(), 256, s); }
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("   CPU: %.2f%% of one core at 48 kHz, all effects on\n", 100 * sec / 20); }
  std::printf("\n%d failure(s)\n", fails);
  return fails;
}
