#pragma once

// Decodes the built-in layer sounds once per process (shared by every instance) and loads the
// user's own samples. Everything decoded here stays alive until the process ends, so the engine can
// read a sample without locks while the message thread swaps what each slot points at.

#include <JuceHeader.h>

#include "Boom808Engine.h"
#include "LayerLibrary.h"

#include <deque>
#include <mutex>

struct DecodedLayer
{
    juce::String name, group;
    std::vector<float> mid, side;
    boom::LayerSample sample;
    bool hasSide = false;

    void finish(double rate, float rootNote, float rootHz)
    {
        sample.mid = mid.data();
        sample.side = hasSide ? side.data() : nullptr;
        sample.length = static_cast<int>(mid.size());
        sample.rate = rate;
        sample.rootNote = rootNote;
        sample.rootHz = rootHz;
    }
};

class LayerBank
{
public:
    static LayerBank& get()
    {
        static LayerBank bank;
        return bank;
    }

    int size() const noexcept { return static_cast<int>(builtIn.size()); }
    const DecodedLayer* at(int index) const noexcept
    {
        return juce::isPositiveAndBelow(index, size()) ? builtIn[static_cast<std::size_t>(index)].get() : nullptr;
    }

    // Decodes any audio file JUCE can read (WAV, AIFF, FLAC, Ogg; MP3 where the OS allows), up to 20 s.
    // Finds the fundamental (25 - 140 Hz) so the sample is tuned like the built-in ones. Message thread only.
    const DecodedLayer* loadUserFile(const juce::File& file, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr)
        {
            error = "Can't read " + file.getFileName() + ". Use a WAV, AIFF or FLAC file.";
            return nullptr;
        }
        auto layer = std::make_unique<DecodedLayer>();
        layer->name = file.getFileNameWithoutExtension();
        layer->group = "Yours";
        if (!decode(*reader, *layer, 20.0))
        {
            error = file.getFileName() + " is empty.";
            return nullptr;
        }
        float rootNote = 24.0f, rootHz = 32.7032f;
        detectRoot(*layer, reader->sampleRate, rootNote, rootHz);
        layer->finish(reader->sampleRate, rootNote, rootHz);
        const std::lock_guard<std::mutex> lock(userMutex);
        user.push_back(std::move(layer));
        return user.back().get();
    }

private:
    LayerBank()
    {
        juce::FlacAudioFormat flac;
        for (int i = 0; i < layerInfoCount; ++i)
        {
            const auto& info = layerInfos[i];
            auto layer = std::make_unique<DecodedLayer>();
            layer->name = info.name;
            layer->group = info.group;
            const char* data = nullptr;
            int bytes = 0;
            for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
                if (juce::String(BinaryData::originalFilenames[r]) == info.file)
                    data = BinaryData::getNamedResource(BinaryData::namedResourceList[r], bytes);
            if (data != nullptr)
            {
                std::unique_ptr<juce::AudioFormatReader> reader(
                    flac.createReaderFor(new juce::MemoryInputStream(data, static_cast<std::size_t>(bytes), false), true));
                if (reader != nullptr && decode(*reader, *layer, 30.0))
                    layer->finish(reader->sampleRate, static_cast<float>(info.rootNote), info.rootHz);
            }
            builtIn.push_back(std::move(layer));
        }
    }

    static bool decode(juce::AudioFormatReader& reader, DecodedLayer& layer, double maxSeconds)
    {
        const auto frames = static_cast<int>(std::min<juce::int64>(reader.lengthInSamples, static_cast<juce::int64>(maxSeconds * reader.sampleRate)));
        if (frames < 64)
            return false;
        juce::AudioBuffer<float> buffer(static_cast<int>(juce::jmax(1u, reader.numChannels)), frames);
        reader.read(&buffer, 0, frames, 0, true, true);
        layer.mid.assign(static_cast<std::size_t>(frames), 0.0f);
        layer.side.assign(static_cast<std::size_t>(frames), 0.0f);
        const auto stereo = buffer.getNumChannels() > 1;
        float sidePeak = 0.0f;
        for (int i = 0; i < frames; ++i)
        {
            const auto l = buffer.getSample(0, i), r = stereo ? buffer.getSample(1, i) : l;
            layer.mid[static_cast<std::size_t>(i)] = 0.5f * (l + r);
            layer.side[static_cast<std::size_t>(i)] = 0.5f * (l - r);
            sidePeak = std::max(sidePeak, std::abs(0.5f * (l - r)));
        }
        layer.hasSide = sidePeak > 1.0e-4f;
        // a user file may start late: skip leading silence so the hit lands on the note
        std::size_t start = 0;
        float peak = 0.0f;
        for (auto v : layer.mid) peak = std::max(peak, std::abs(v));
        while (start + 64 < layer.mid.size() && std::abs(layer.mid[start]) < 0.02f * peak) ++start;
        start = start > 8 ? start - 8 : 0;
        layer.mid.erase(layer.mid.begin(), layer.mid.begin() + static_cast<std::ptrdiff_t>(start));
        layer.side.erase(layer.side.begin(), layer.side.begin() + static_cast<std::ptrdiff_t>(start));
        // normalise to -1 dBFS like the built-in sounds
        if (peak > 0.0f)
            for (auto* v : { &layer.mid, &layer.side })
                for (auto& x : *v) x *= 0.89f / peak;
        return true;
    }

    // Strongest fundamental between 25 and 140 Hz in 150 - 600 ms (after the punch), by a DFT scan.
    static void detectRoot(const DecodedLayer& layer, double rate, float& rootNote, float& rootHz)
    {
        const auto a = static_cast<int>(0.15 * rate);
        const auto b = std::min(static_cast<int>(layer.mid.size()), static_cast<int>(0.6 * rate));
        if (b - a < static_cast<int>(0.1 * rate))
            return;
        double bestHz = 0.0, bestPower = 0.0;
        for (double hz = 25.0; hz <= 140.0; hz *= std::pow(2.0, 2.0 / 1200.0))   // 2-cent steps
        {
            double re = 0.0, im = 0.0;
            const auto w = juce::MathConstants<double>::twoPi * hz / rate;
            for (int i = a; i < b; i += 8)
            {
                const auto win = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * (i - a) / (b - a));
                re += win * layer.mid[static_cast<std::size_t>(i)] * std::cos(w * i);
                im += win * layer.mid[static_cast<std::size_t>(i)] * std::sin(w * i);
            }
            if (re * re + im * im > bestPower) { bestPower = re * re + im * im; bestHz = hz; }
        }
        if (bestHz <= 0.0)
            return;
        rootHz = static_cast<float>(bestHz);
        rootNote = static_cast<float>(std::round(69.0 + 12.0 * std::log2(bestHz / 440.0)));
    }

    std::vector<std::unique_ptr<DecodedLayer>> builtIn;
    std::deque<std::unique_ptr<DecodedLayer>> user;
    std::mutex userMutex;
};
