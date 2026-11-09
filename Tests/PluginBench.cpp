// Epsilon 808 plugin bench: loads every factory preset into the real processor, plays a 4-bar 140 BPM trap
// phrase (with an overlapping slide) and measures gated loudness (BS.1770), peak and Phone Check loudness.
// Also: hybrid layers decode and play, a user sample loads, and state round-trips.
//   PluginBench [--fix]   (--fix prints the Output value each preset needs for -13.5 LUFS)
#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace
{
int fails = 0;
void check(bool ok, const juce::String& what) { std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (!ok) ++fails; }

struct Note { int start, length, note; float velocity; };
// The same 4-bar 140 BPM C minor line the v1.0 audit and the browser lab use (overlaps glide in Legato).
const std::vector<Note> phrase {
    { 0, 7, 24, 1.0f }, { 6, 3, 31, 1.0f }, { 10, 2, 24, 1.0f }, { 12, 4, 27, 1.0f },
    { 16, 6, 32, 1.0f }, { 22, 2, 32, 1.0f }, { 24, 4, 24, 1.0f }, { 28, 4, 27, 1.0f },
    { 32, 7, 29, 1.0f }, { 38, 3, 36, 1.0f }, { 42, 2, 29, 1.0f }, { 44, 4, 32, 1.0f },
    { 48, 6, 31, 1.0f }, { 54, 2, 31, 1.0f }, { 56, 4, 35, 1.0f }, { 60, 4, 26, 1.0f } };

// gated integrated loudness (BS.1770-4): K-weighting, 400 ms blocks, 75% overlap, -70 LUFS and -10 LU gates
double integratedLufs(const juce::AudioBuffer<float>& b, double sr)
{
    struct Bq { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0; double run(double x) { const auto y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    const auto design = [sr](double f0, double g, double q, bool shelf)
    {
        const auto k = std::tan(juce::MathConstants<double>::pi * f0 / sr);
        Bq z {};
        if (shelf) { const auto vh = std::pow(10.0, g / 20.0), vb = std::pow(vh, 0.4996667741545416), a0 = 1 + k / q + k * k;
                     z.b0 = (vh + vb * k / q + k * k) / a0; z.b1 = 2 * (k * k - vh) / a0; z.b2 = (vh - vb * k / q + k * k) / a0; z.a1 = 2 * (k * k - 1) / a0; z.a2 = (1 - k / q + k * k) / a0; }
        else { const auto a0 = 1 + k / q + k * k; z.b0 = 1; z.b1 = -2; z.b2 = 1; z.a1 = 2 * (k * k - 1) / a0; z.a2 = (1 - k / q + k * k) / a0; }
        return z;
    };
    std::vector<double> sq(static_cast<std::size_t>(b.getNumSamples()), 0.0);
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        auto s1 = design(1681.974450955533, 3.999843853973347, 0.7071752369554196, true), s2 = design(38.13547087602444, 0, 0.5003270373238773, false);
        for (int i = 0; i < b.getNumSamples(); ++i) { const auto y = s2.run(s1.run(b.getSample(c, i))); sq[static_cast<std::size_t>(i)] += y * y; }
    }
    const int block = static_cast<int>(0.4 * sr), hop = block / 4;
    std::vector<double> blocks;
    for (int a = 0; a + block <= b.getNumSamples(); a += hop) { double s = 0; for (int i = a; i < a + block; ++i) s += sq[static_cast<std::size_t>(i)]; blocks.push_back(s / block); }
    const auto lufs = [](double ms) { return -0.691 + 10 * std::log10(std::max(ms, 1e-20)); };
    double s = 0; int n = 0;
    for (auto v : blocks) if (lufs(v) > -70) { s += v; ++n; }
    if (n == 0) return -70;
    const auto rel = lufs(s / n) - 10; s = 0; n = 0;
    for (auto v : blocks) if (lufs(v) > rel) { s += v; ++n; }
    return n ? lufs(s / n) : -70;
}

juce::AudioBuffer<float> play(Epsilon808AudioProcessor& p, double sr, double seconds, const std::vector<Note>& notes, double bpm = 140)
{
    const auto step = 60.0 / bpm / 4.0;
    juce::AudioBuffer<float> out(2, static_cast<int>(seconds * sr));
    out.clear();
    const int blockSize = 256;
    for (int pos = 0; pos < out.getNumSamples(); pos += blockSize)
    {
        const auto n = std::min(blockSize, out.getNumSamples() - pos);
        juce::AudioBuffer<float> buf(2, n);
        buf.clear();
        juce::MidiBuffer midi;
        for (const auto& note : notes)
        {
            const auto on = static_cast<int>(note.start * step * sr), off = static_cast<int>((note.start + note.length) * step * sr);
            if (on >= pos && on < pos + n) midi.addEvent(juce::MidiMessage::noteOn(1, note.note, note.velocity), on - pos);
            if (off >= pos && off < pos + n) midi.addEvent(juce::MidiMessage::noteOff(1, note.note), off - pos);
        }
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom(c, pos, buf, c, 0, n);
    }
    return out;
}

void setParam(Epsilon808AudioProcessor& p, const char* id, float plain)
{
    auto* param = p.params.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(plain));
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto fix = argc > 1 && juce::String(argv[1]) == "--fix";
    if (argc > 2 && juce::String(argv[1]) == "--snap")
    {
        // editor snapshots: --snap <folder>  (Simple, Advanced, Hybrid with a preset loaded)
        Epsilon808AudioProcessor sp;
        sp.setPlayConfigDetails(0, 2, 48000, 256);
        sp.prepareToPlay(48000, 256);
        const juce::File folder(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));
        folder.createDirectory();
        const auto shoot = [&](const char* name, bool advanced, bool hybrid, int program)
        {
            if (program >= 0) sp.setCurrentProgram(program);
            sp.params.state.setProperty("advancedView", advanced, nullptr);
            sp.params.state.setProperty("hybridView", hybrid, nullptr);
            std::unique_ptr<juce::AudioProcessorEditor> ed(sp.createEditor());
            ed->setSize(980, 760);
            for (int k = 0; k < 20; ++k) { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); }
            auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto file = folder.getChildFile(juce::String(name) + ".png");
            file.deleteFile();
            juce::FileOutputStream out(file);
            png.writeImageToStream(img, out);
        };
        int layerCake = 0;
        for (int i = 0; i < sp.getNumPrograms(); ++i) if (sp.getProgramName(i) == "Layer Cake") layerCake = i;
        shoot("hybrid", false, true, layerCake);
        shoot("simple", false, false, -1);
        shoot("advanced", true, false, -1);
        return 0;
    }
    const double sr = 48000;
    Epsilon808AudioProcessor p;
    p.setPlayConfigDetails(0, 2, sr, 256);
    p.prepareToPlay(sr, 256);

    // layers decoded
    const auto& bank = LayerBank::get();
    int decoded = 0;
    for (int i = 0; i < bank.size(); ++i) if (bank.at(i) != nullptr && bank.at(i)->sample.length > 1000) ++decoded;
    check(decoded == layerInfoCount, juce::String(decoded) + " of " + juce::String(layerInfoCount) + " built-in layers decode");

    std::printf("\n%-26s %-14s %7s %7s %7s %7s\n", "preset", "category", "LUFS", "peak", "phone", "output");
    const auto& presets = Epsilon808AudioProcessor::factoryPresets();
    juce::StringArray fixes;
    std::vector<double> levels;
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        p.setCurrentProgram(i);
        p.prepareToPlay(sr, 256);
        auto out = play(p, sr, 8.0, phrase);
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int s = 0; s < out.getNumSamples(); ++s) finite = finite && std::isfinite(out.getSample(c, s));
        const auto lufs = integratedLufs(out, sr);
        const auto peak = juce::Decibels::gainToDecibels(out.getMagnitude(0, out.getNumSamples()));
        setParam(p, "phone", 1);
        p.prepareToPlay(sr, 256);
        const auto phone = integratedLufs(play(p, sr, 8.0, phrase), sr);
        setParam(p, "phone", 0);
        const auto output = p.params.getRawParameterValue("output")->load();
        std::printf("%-26s %-14s %7.1f %7.1f %7.1f %7.1f\n", presets[static_cast<std::size_t>(i)].name, presets[static_cast<std::size_t>(i)].category, lufs, peak, phone, output);
        check(finite && peak <= 0.0f, juce::String("preset plays clean: ") + presets[static_cast<std::size_t>(i)].name);
        levels.push_back(lufs);
        fixes.add(juce::String(presets[static_cast<std::size_t>(i)].name) + "|" + juce::String(output, 2) + "|" + juce::String(lufs, 2));
    }
    // v1.1 presets are matched to the v1.0 presets (the first 41) as measured here, so switching compares sounds
    std::vector<double> v10(levels.begin(), levels.begin() + std::min<std::size_t>(41, levels.size()));
    std::nth_element(v10.begin(), v10.begin() + static_cast<long>(v10.size() / 2), v10.end());
    const auto reference = v10[v10.size() / 2];
    double worst = 0;
    for (auto l : levels) worst = std::max(worst, std::abs(l - reference));
    check(worst <= 0.45 || fix, "every factory preset within 0.45 LU of the v1.0 presets' level, " + juce::String(reference, 1) + " LUFS on this phrase (worst " + juce::String(worst, 2) + ")");
    if (fix)
        for (std::size_t i = 0; i < levels.size(); ++i)
            if (std::abs(levels[i] - reference) > 0.3)
            {
                const auto parts = juce::StringArray::fromTokens(fixes[static_cast<int>(i)], "|", "");
                std::printf("FIX %s=%.1f\n", parts[0].toRawUTF8(), juce::jlimit(-24.0, 12.0, parts[1].getDoubleValue() + reference - levels[i]));
            }

    // a user sample: write a decaying 41 Hz tone, load it into Layer B, hear it
    {
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("e808-bench-sample.wav");
        juce::AudioBuffer<float> tone(2, 44100);
        for (int i = 0; i < 44100; ++i) { const auto v = 0.8f * std::exp(-i / 30000.0f) * std::sin(juce::MathConstants<float>::twoPi * 41.2f * i / 44100.0f); tone.setSample(0, i, v); tone.setSample(1, i, v); }
        file.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
            auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions().withSampleRate(44100).withNumChannels(2).withBitsPerSample(24));
            if (writer != nullptr) writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples());
        }
        p.setCurrentProgram(0);
        setParam(p, "engine", -48);
        juce::String error;
        const auto loaded = p.loadUserSample(1, file, error);
        check(loaded, "user sample loads (" + (loaded ? p.getUserSampleName(1) : error) + ")");
        setParam(p, "layerBLowCut", 20);   // Layer B cuts below 150 Hz by default (it is the texture layer)
        p.prepareToPlay(sr, 256);
        auto out = play(p, sr, 1.0, { { 0, 6, 28, 1.0f } });
        // E1 = 41.2 Hz: the sample was detected at its own pitch and plays there
        int crossings = 0; float last = 0;
        for (int s = static_cast<int>(0.2 * sr); s < static_cast<int>(0.6 * sr); ++s) { const auto v = out.getSample(0, s); if (last < 0 && v >= 0) ++crossings; last = v; }
        check(out.getRMSLevel(0, 0, out.getNumSamples()) > 0.01f && std::abs(crossings / 0.4 - 41.2) < 3.0,
              "user sample plays on note E1 at about " + juce::String(crossings / 0.4, 1) + " Hz (engine off)");

        // state round trip keeps the layer choice and reloads the user sample
        juce::MemoryBlock state;
        p.getStateInformation(state);
        Epsilon808AudioProcessor q;
        q.setPlayConfigDetails(0, 2, sr, 256);
        q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        check(q.getUserSampleName(1) == p.getUserSampleName(1) && q.params.getRawParameterValue("layerB")->load() == p.params.getRawParameterValue("layerB")->load(),
              "session state restores Layer B and its user sample");
        file.deleteFile();
    }
    std::printf("\n%d failure(s)\n", fails);
    return fails;
}
