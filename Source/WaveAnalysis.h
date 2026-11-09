#pragma once

// Turns a recorded 808 (or any pitched bass) into a "morph source": a series of single-cycle waves, one
// every 20 ms of the sample's life, each holding the sample's harmonic recipe and wave shape at that moment
// (its bright, gritty front, how it darkens, its movement). The engine plays these at its own pitch and
// blends its sine into them, so the two become one wave: no layering, no phase cancellation.
// Plain C++17, no JUCE. Runs once per sound when it is loaded, never on the audio thread.

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace boom
{
namespace wave
{
constexpr int tableSize = 2048;       // samples per single-cycle wave
constexpr double hopSeconds = 0.02;   // one wave every 20 ms
constexpr double maxSeconds = 3.0;    // the sample's first 3 s (later notes hold the last wave)
constexpr int maxHarmonics = 48;

inline void fft(std::vector<std::complex<double>>& a, bool inverse)
{
    const auto n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const auto ang = 2.0 * 3.141592653589793 / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// mid: the sample (mono), rate: its sample rate, rootHz: its measured fundamental.
// Returns frames * tableSize floats; each wave's fundamental is in sine phase (so blending with the
// engine's sine adds, never cancels) and every wave has the RMS of a full-scale sine.
inline std::vector<float> analyse(const float* mid, int length, double rate, double rootHz)
{
    std::vector<float> tables;
    if (mid == nullptr || length < 1024 || rootHz <= 1.0)
        return tables;
    const int window = 4096, fftSize = 8192;
    const auto frames = std::max(1, static_cast<int>(std::min(maxSeconds, (length - window * 0.5) / rate) / hopSeconds));
    tables.assign(static_cast<std::size_t>(frames) * tableSize, 0.0f);
    std::vector<std::complex<double>> spec(fftSize), cyc(tableSize);
    std::vector<double> hann(window);
    for (int i = 0; i < window; ++i)
        hann[static_cast<std::size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * 3.141592653589793 * i / (window - 1));
    const auto binHz = rate / fftSize;
    bool havePrevious = false;

    for (int f = 0; f < frames; ++f)
    {
        float* table = tables.data() + static_cast<std::size_t>(f) * tableSize;
        const auto start = static_cast<int>(f * hopSeconds * rate);
        std::fill(spec.begin(), spec.end(), std::complex<double>());
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const auto n = start + i;
            const double x = n < length ? mid[n] : 0.0;
            spec[static_cast<std::size_t>(i)] = x * hann[static_cast<std::size_t>(i)];
            energy += x * x;
        }
        if (energy < 1.0e-9 * window)
        {
            if (havePrevious) std::copy(table - tableSize, table, table);
            continue;
        }
        fft(spec, false);
        const auto mag = [&](int b) { return std::abs(spec[static_cast<std::size_t>(std::clamp(b, 0, fftSize / 2))]); };
        // this moment's fundamental: the strongest peak between 0.8x and 2.3x the root (an 808 starts sharp)
        auto best = static_cast<int>(0.8 * rootHz / binHz);
        for (int b = best; b <= static_cast<int>(2.3 * rootHz / binHz); ++b)
            if (mag(b) > mag(best)) best = b;
        double f0 = best * binHz;
        {
            const auto a = mag(best - 1), b = mag(best), c = mag(best + 1);
            const auto d = a - 2.0 * b + c;
            if (std::abs(d) > 1.0e-12) f0 = (best + 0.5 * (a - c) / d) * binHz;
        }
        if (f0 < 10.0) f0 = rootHz;

        // each harmonic: the peak nearest k * f0, its level and phase
        std::vector<double> amp(maxHarmonics + 1, 0.0), phase(maxHarmonics + 1, 0.0);
        for (int k = 1; k <= maxHarmonics; ++k)
        {
            if (k * f0 > std::min(9000.0, rate * 0.45)) break;
            const auto centre = static_cast<int>(std::round(k * f0 / binHz));
            const auto reach = std::max(1, static_cast<int>(0.3 * f0 / binHz));
            auto pk = centre;
            for (int b = centre - reach; b <= centre + reach; ++b)
                if (mag(b) > mag(pk)) pk = b;
            amp[static_cast<std::size_t>(k)] = mag(pk);
            phase[static_cast<std::size_t>(k)] = std::arg(spec[static_cast<std::size_t>(std::clamp(pk, 0, fftSize / 2))]);
        }
        // one cycle: harmonic k at amp a_k, phase relative to the fundamental's, fundamental in sine phase
        std::fill(cyc.begin(), cyc.end(), std::complex<double>());
        for (int k = 1; k <= maxHarmonics && k < tableSize / 2; ++k)
        {
            if (amp[static_cast<std::size_t>(k)] <= 0.0) continue;
            const auto rel = phase[static_cast<std::size_t>(k)] - k * phase[1] - k * 3.141592653589793 * 0.5;
            cyc[static_cast<std::size_t>(k)] = std::polar(0.5 * amp[static_cast<std::size_t>(k)], rel);
            cyc[static_cast<std::size_t>(tableSize - k)] = std::conj(cyc[static_cast<std::size_t>(k)]);
        }
        fft(cyc, true);
        double rms = 0.0;
        for (int i = 0; i < tableSize; ++i) rms += cyc[static_cast<std::size_t>(i)].real() * cyc[static_cast<std::size_t>(i)].real();
        rms = std::sqrt(rms / tableSize);
        if (rms < 1.0e-12)
        {
            if (havePrevious) std::copy(table - tableSize, table, table);
            continue;
        }
        const auto g = 0.70710678 / rms;
        for (int i = 0; i < tableSize; ++i)
            table[i] = static_cast<float>(cyc[static_cast<std::size_t>(i)].real() * g);
        if (!havePrevious)   // frames before the first sound (none in practice) take this wave
            for (int p = 0; p < f; ++p)
                std::copy(table, table + tableSize, tables.data() + static_cast<std::size_t>(p) * tableSize);
        havePrevious = true;
    }
    return tables;
}
}
}
