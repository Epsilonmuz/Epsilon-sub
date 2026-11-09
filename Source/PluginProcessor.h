#pragma once

#include <JuceHeader.h>

#include "Boom808Engine.h"
#include "LayerBank.h"

struct BoomPreset
{
    const char* name;
    const char* category;
    const char* note;    // one line shown in the preset browser
    std::vector<std::pair<const char*, float>> values;   // plain values; anything not listed = default
};

// One entry in the preset browser: a factory preset or a user preset file.
struct PresetEntry
{
    juce::String name, category, note;
    int factoryIndex = -1;   // >= 0 for factory presets
    juce::File file;         // user presets
    bool isUser() const { return factoryIndex < 0; }
    juce::String key() const { return (isUser() ? "U:" : "F:") + name; }
};

class Epsilon808AudioProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    Epsilon808AudioProcessor();
    ~Epsilon808AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Epsilon 808"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Host programs = the factory presets.
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax(0, currentFactory); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int size) override;

    void setMeteringEnabled(bool enabled) noexcept
    {
        const auto clients = (meteringClients += enabled ? 1 : -1);
        engine.setMeteringEnabled(clients > 0);
    }
    bool readMeterFrame(boom::MeterFrame& frame) noexcept { return engine.readMeterFrame(frame); }
    double getCurrentSampleRate() const noexcept { return sampleRateForDisplay.load(); }

    // presets
    static const std::vector<BoomPreset>& factoryPresets();
    static juce::StringArray categories();
    static const std::vector<std::pair<const char*, float>>& characterBaseline();
    static juce::File userPresetFolder();
    std::vector<PresetEntry> listPresets() const;
    void loadPreset(const PresetEntry&);
    bool saveUserPreset(const juce::String& name, const juce::String& category);
    bool deleteUserPreset(const PresetEntry&);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool isPresetModified() const noexcept { return presetModified.load(); }
    bool isFavourite(const PresetEntry&) const;
    void toggleFavourite(const PresetEntry&);

    // A/B
    int getActiveSlot() const noexcept { return activeSlot; }
    void selectSlot(int slot);
    void copyActiveSlotToOther();

    // Kick Lock status for the panel
    bool isKickInputRouted() const noexcept { return kickRouted.load(); }
    float getKickInputLevel() const noexcept { return kickLevel.load(); }

    // Hybrid layers. Choice index 0 = Off, 1..N = built-in sounds, N + 1 = the slot's own sample.
    static juce::StringArray layerChoices(int slot);
    static int layerIndexOf(const char* name);
    static int userLayerIndex() { return LayerBank::get().size() + 1; }
    bool loadUserSample(int slot, const juce::File& file, juce::String& error);
    juce::String getUserSampleName(int slot) const;
    const DecodedLayer* layerFor(int slot) const;   // what the slot plays now (for the waveform), or null

    // Settings that belong to the session, not to the sound: presets and A/B leave them alone.
    static bool isSessionParameter(const juce::String& id);

    // Notes played on the on-screen keyboard arrive here and are merged with the host's MIDI.
    juce::MidiKeyboardState keyboardState;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void parameterChanged(const juce::String&, float) override;
    std::vector<float> captureSettings() const;
    void applySettings(const std::vector<float>& values);
    void resetSoundParameters();
    boom::Settings readSettings() const;
    void handleMidi(const juce::MidiMessage&);
    void loadFavourites();

    boom::Engine engine;
    std::atomic<double> sampleRateForDisplay { 44100.0 };
    std::atomic<int> meteringClients { 0 };

    int currentFactory = 0;
    juce::String currentPresetName { "Init 808" };
    std::atomic<bool> presetModified { false };
    bool suppressModified = false;
    juce::StringArray favourites;

    std::array<std::vector<float>, 2> slots;
    int activeSlot = 0;

    // MIDI delay line for Kick Lock's Exact mode (the engine sees each kick 5 ms before it plays)
    struct DelayedEvent { std::int64_t time = 0; std::uint8_t data[3] {}; int size = 0; };
    std::array<DelayedEvent, 1024> midiQueue {};
    int queueHead = 0, queueCount = 0;
    std::int64_t sampleClock = 0;
    int currentLookahead = -1;

    std::array<std::atomic<const DecodedLayer*>, 2> userLayer {};
    std::array<juce::File, 2> userFile;

    std::vector<float> sidechainMono;
    std::atomic<bool> kickRouted { false };
    std::atomic<float> kickLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Epsilon808AudioProcessor)
};
