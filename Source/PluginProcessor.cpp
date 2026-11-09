#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String hzText(float hz) { return hz < 1000.0f ? juce::String(juce::roundToInt(hz)) + " Hz" : juce::String(hz / 1000.0f, 1) + " kHz"; }
juce::String msText(float ms) { return ms < 1000.0f ? juce::String(ms, ms < 10.0f ? 1 : 0) + " ms" : juce::String(ms / 1000.0f, 2) + " s"; }
constexpr float splitOff = 50.0f;   // the Split knob's lowest position means Off
}

const std::vector<BoomPreset>& Epsilon808AudioProcessor::factoryPresets()
{
    static const std::vector<BoomPreset> list {
        // Every factory preset is loudness-matched to about -13.5 LUFS (gated, BS.1770) on a 4-bar test phrase,
        // so switching presets compares sounds, not levels.
        // Clean
        { "Init 808", "Clean", "A clean, punchy 808. Start here.", {  } },
        { "Clean Sub", "Clean", "Almost a pure sine. For R&B, ballads and big systems.", { { "punch", 4 }, { "tone", 0 }, { "drive", 0 }, { "click", 0 }, { "glue", 20 }, { "length", 5000 }, { "sustain", 60 }, { "analog", 10 }, { "translate", 25 }, { "output", -2.5 } } },
        { "Pure Sine", "Clean", "Nothing added. Layer it under another bass.", { { "punch", 0 }, { "tone", 0 }, { "click", 0 }, { "drive", 0 }, { "glue", 0 }, { "sustain", 100 }, { "length", 8000 }, { "release", 200 }, { "output", -3 } } },
        { "Soft Boom", "Clean", "Round and warm, no knock. Sits under vocals.", { { "punch", 5 }, { "punchTime", 80 }, { "attack", 6 }, { "tone", 10 }, { "click", 0 }, { "drive", 2 }, { "length", 4000 }, { "analog", 40 }, { "glue", 25 }, { "translate", 25 }, { "output", -1 } } },
        { "R&B Sub", "Clean", "Smooth sustained sub with gentle slides.", { { "punch", 3 }, { "tone", 10 }, { "click", 5 }, { "drive", 3 }, { "glue", 30 }, { "length", 3500 }, { "sustain", 40 }, { "glide", 120 }, { "analog", 25 }, { "translate", 30 }, { "output", -1.5 } } },
        // Trap
        { "Long Trap 808", "Trap", "The classic long trap tail. Overlap notes to slide.", { { "punch", 12 }, { "punchTime", 40 }, { "length", 3500 }, { "glide", 120 }, { "drive", 9 }, { "glue", 35 }, { "translate", 30 }, { "click", 10 }, { "velPunch", 30 }, { "analog", 15 }, { "output", -2 } } },
        { "Trap Classic", "Trap", "Punchy, slightly driven, heard on phones.", { { "punch", 14 }, { "punchTime", 50 }, { "length", 2800 }, { "glide", 100 }, { "drive", 10 }, { "tone", 20 }, { "translate", 35 }, { "glue", 35 }, { "velPunch", 30 }, { "split", 90 }, { "output", 1 } } },
        { "Club Trap", "Trap", "Hard knock and Tube grit with a clean sub under it.", { { "punch", 16 }, { "punchTime", 35 }, { "length", 2400 }, { "click", 25 }, { "drive", 14 }, { "driveMode", 1 }, { "split", 100 }, { "translate", 40 }, { "glue", 45 }, { "velPunch", 40 }, { "output", 2 } } },
        { "Glide Machine", "Trap", "Slides into every note. Sustains while held.", { { "glide", 180 }, { "glideMode", 1 }, { "length", 4000 }, { "sustain", 30 }, { "drive", 12 }, { "driveMode", 1 }, { "translate", 40 }, { "split", 100 }, { "glue", 35 }, { "output", -0.5 } } },
        { "Deep Tail", "Trap", "Seven-second tail with an octave-down sub.", { { "punch", 9 }, { "length", 7000 }, { "sub", 25 }, { "drive", 6 }, { "release", 400 }, { "analog", 20 }, { "glue", 30 }, { "translate", 35 }, { "output", -2 } } },
        { "Southern Boom", "Trap", "Warm Tube boom with analog drift.", { { "punch", 10 }, { "length", 5000 }, { "sub", 15 }, { "drive", 10 }, { "driveMode", 1 }, { "translate", 30 }, { "analog", 35 }, { "glue", 40 }, { "split", 90 }, { "output", 1 } } },
        { "Velvet Trap", "Trap", "Soft-edged trap 808 for melodic beats.", { { "punch", 8 }, { "length", 3000 }, { "tone", 10 }, { "drive", 4 }, { "glue", 25 }, { "analog", 30 }, { "translate", 35 }, { "output", -1 } } },
        // Drill
        { "Drill Slide", "Drill", "Long slides into every note, Hard drive, 12-semitone bend.", { { "glide", 220 }, { "glideMode", 1 }, { "bendRange", 12 }, { "punch", 7 }, { "length", 2600 }, { "drive", 14 }, { "driveMode", 2 }, { "tone", 25 }, { "translate", 45 }, { "split", 110 }, { "glue", 40 }, { "output", 1 } } },
        { "UK Drill", "Drill", "Low-note priority, fresh punch on every slide.", { { "glide", 260 }, { "glideMode", 1 }, { "punch", 6 }, { "length", 2200 }, { "drive", 18 }, { "driveMode", 2 }, { "tone", 30 }, { "translate", 50 }, { "retrigger", 1 }, { "split", 110 }, { "priority", 1 }, { "glue", 40 }, { "output", 0.5 } } },
        { "NY Drill", "Drill", "Dirtier and brighter, knocks on every slide.", { { "glide", 200 }, { "glideMode", 1 }, { "punch", 10 }, { "length", 1800 }, { "drive", 22 }, { "driveMode", 1 }, { "tone", 25 }, { "translate", 55 }, { "click", 30 }, { "split", 100 }, { "retrigger", 1 }, { "glue", 45 }, { "output", 1 } } },
        { "Low Note Drill", "Drill", "Hold a low note and tap higher ones: the low note wins.", { { "priority", 1 }, { "glide", 180 }, { "length", 2500 }, { "drive", 14 }, { "driveMode", 2 }, { "translate", 45 }, { "split", 110 }, { "glue", 35 }, { "output", 0.5 } } },
        { "Slide Grit", "Drill", "Gritty Tape slides with the fizz filtered off.", { { "glide", 240 }, { "glideMode", 1 }, { "drive", 24 }, { "cutoff", 5000 }, { "translate", 40 }, { "split", 120 }, { "glue", 40 }, { "output", 0.5 } } },
        // Distorted
        { "Distorted 808", "Distorted", "Heavy Tube distortion, clean sub under it.", { { "drive", 26 }, { "driveMode", 1 }, { "tone", 30 }, { "translate", 50 }, { "cutoff", 6000 }, { "glue", 40 }, { "split", 100 }, { "output", 0.5 } } },
        { "Hard Clip 808", "Distorted", "Aggressive clipped edge.", { { "drive", 24 }, { "driveMode", 2 }, { "glue", 20 }, { "click", 25 }, { "split", 90 }, { "cutoff", 9000 }, { "output", -1.5 } } },
        { "Fold Growl", "Distorted", "Wavefolder growl through a resonant filter.", { { "drive", 24 }, { "driveMode", 3 }, { "driveMix", 70 }, { "tone", 20 }, { "cutoff", 3500 }, { "resonance", 25 }, { "split", 110 }, { "output", 0.5 } } },
        { "Rage Fuzz", "Distorted", "Maximum fuzz for rage beats. Sub stays solid.", { { "drive", 34 }, { "driveMode", 2 }, { "tone", 40 }, { "translate", 60 }, { "cutoff", 6500 }, { "glue", 45 }, { "split", 90 }, { "width", 30 }, { "output", -1 } } },
        { "Tube Warmth", "Distorted", "Half clean, half Tube. Thick but polite.", { { "drive", 15 }, { "driveMode", 1 }, { "driveMix", 60 }, { "tone", 20 }, { "glue", 30 }, { "analog", 20 }, { "translate", 30 }, { "output", -0.5 } } },
        { "Tape Saturated", "Distorted", "Tape saturation with a clean low end.", { { "drive", 18 }, { "tone", 15 }, { "translate", 30 }, { "glue", 40 }, { "split", 80 }, { "analog", 15 }, { "output", 1 } } },
        { "Screamer", "Distorted", "Folded and filtered, almost vocal.", { { "drive", 30 }, { "driveMode", 3 }, { "driveMix", 55 }, { "tone", 35 }, { "cutoff", 2800 }, { "resonance", 40 }, { "split", 120 }, { "output", 1 } } },
        // Short
        { "Punchy Short", "Short", "Short and tight. Velocity sets the knock.", { { "length", 450 }, { "punch", 18 }, { "punchTime", 30 }, { "click", 35 }, { "drive", 10 }, { "glue", 50 }, { "velPunch", 40 }, { "output", 4 } } },
        { "Knock", "Short", "Maximum punch. Cuts through any kick.", { { "punch", 24 }, { "punchTime", 25 }, { "length", 700 }, { "click", 50 }, { "drive", 16 }, { "glue", 60 }, { "split", 90 }, { "velPunch", 40 }, { "output", 6 } } },
        { "Bounce", "Short", "Bouncy short 808 for up-tempo beats.", { { "length", 600 }, { "punch", 16 }, { "punchTime", 35 }, { "click", 25 }, { "drive", 10 }, { "glue", 45 }, { "velPunch", 50 }, { "translate", 30 }, { "output", 3 } } },
        { "Detroit Tap", "Short", "Tapped, Hard-driven, very short.", { { "length", 350 }, { "punch", 20 }, { "punchTime", 20 }, { "click", 45 }, { "drive", 14 }, { "driveMode", 2 }, { "glue", 55 }, { "translate", 30 }, { "split", 100 }, { "output", 7.5 } } },
        { "Kick 808", "Short", "Doubles as the kick. Accents hit harder and dirtier.", { { "punch", 24 }, { "punchTime", 30 }, { "length", 1200 }, { "click", 60 }, { "drive", 12 }, { "glue", 50 }, { "velPunch", 40 }, { "velDrive", 30 }, { "split", 80 }, { "output", 4 } } },
        // Melodic
        { "Melodic Glide", "Melodic", "Sustained and bright enough to carry a melody.", { { "glide", 140 }, { "length", 3000 }, { "sustain", 50 }, { "tone", 30 }, { "drive", 8 }, { "translate", 45 }, { "analog", 15 }, { "output", -3 } } },
        { "Bell Sub", "Melodic", "Bell-like harmonics over the sub.", { { "tone", 60 }, { "punch", 2 }, { "drive", 5 }, { "translate", 50 }, { "length", 2500 }, { "cutoff", 4000 }, { "click", 5 } } },
        { "Plucked Bass", "Melodic", "Short pluck with a resonant filter. Accents get dirtier.", { { "length", 800 }, { "tone", 45 }, { "punch", 6 }, { "punchTime", 20 }, { "drive", 10 }, { "driveMode", 1 }, { "cutoff", 2500 }, { "resonance", 30 }, { "velDrive", 40 }, { "translate", 30 }, { "output", 2.5 } } },
        // Log Drum
        { "Log Drum", "Log Drum", "Amapiano log drum. Play from C1; it sounds an octave up.", { { "tune", 12 }, { "length", 320 }, { "punch", 9 }, { "punchTime", 14 }, { "click", 55 }, { "tone", 60 }, { "drive", 6 }, { "cutoff", 1500 }, { "resonance", 45 }, { "glide", 60 }, { "glideMode", 1 }, { "translate", 30 }, { "glue", 40 }, { "output", 4 } } },
        { "Log Drum Deep", "Log Drum", "Darker, longer log drum.", { { "tune", 7 }, { "length", 450 }, { "punch", 7 }, { "punchTime", 18 }, { "click", 45 }, { "tone", 50 }, { "drive", 4 }, { "cutoff", 1100 }, { "resonance", 40 }, { "glide", 80 }, { "glideMode", 1 }, { "glue", 35 }, { "translate", 20 }, { "output", 4 } } },
        { "Log Drum Bright", "Log Drum", "Bright log drum that cuts through.", { { "tune", 12 }, { "length", 260 }, { "punch", 12 }, { "punchTime", 10 }, { "click", 70 }, { "tone", 70 }, { "drive", 10 }, { "cutoff", 2400 }, { "resonance", 50 }, { "glide", 50 }, { "glideMode", 1 }, { "translate", 40 }, { "glue", 40 }, { "output", 3 } } },
        { "Amapiano Slide", "Log Drum", "Log drum with long slides and a fresh hit on each.", { { "tune", 12 }, { "length", 380 }, { "punch", 8 }, { "punchTime", 15 }, { "click", 50 }, { "tone", 55 }, { "cutoff", 1400 }, { "resonance", 42 }, { "glide", 140 }, { "glideMode", 1 }, { "retrigger", 1 }, { "glue", 35 }, { "output", 3 } } },
        // Wide
        { "Wide Character", "Wide", "Wide Tube harmonics; the sub stays mono.", { { "width", 60 }, { "tone", 30 }, { "drive", 14 }, { "driveMode", 1 }, { "translate", 35 }, { "split", 100 }, { "output", 2 } } },
        { "Stereo Grit", "Wide", "Wide and gritty for big choruses.", { { "width", 70 }, { "drive", 20 }, { "driveMode", 1 }, { "tone", 35 }, { "translate", 45 }, { "split", 110 }, { "glue", 40 }, { "output", 1.5 } } },
        { "Wide Fold", "Wide", "Wide wavefolder texture.", { { "width", 50 }, { "drive", 22 }, { "driveMode", 3 }, { "driveMix", 65 }, { "cutoff", 3800 }, { "resonance", 20 }, { "split", 110 }, { "output", 1 } } },
        // Mix Ready
        { "Phone Ready", "Mix Ready", "Built to be heard on phone speakers.", { { "translate", 80 }, { "drive", 12 }, { "tone", 35 }, { "click", 30 }, { "split", 100 }, { "glue", 40 }, { "output", 2.5 } } },
        { "Heard Everywhere", "Mix Ready", "Balanced for phones, laptops and clubs.", { { "translate", 65 }, { "split", 100 }, { "drive", 10 }, { "glue", 45 }, { "tone", 25 }, { "analog", 10 }, { "output", 3 } } },
    };
    return list;
}

juce::StringArray Epsilon808AudioProcessor::categories()
{
    return { "Trap", "Drill", "Clean", "Distorted", "Short", "Melodic", "Log Drum", "Wide", "Mix Ready" };
}

juce::File Epsilon808AudioProcessor::userPresetFolder()
{
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Epsilon Sounds/808/Presets");
}

bool Epsilon808AudioProcessor::isSessionParameter(const juce::String& id)
{
    return id == "phone" || id == "phoneModel" || id == "kickLock" || id == "kickMode" || id == "duck";
}

juce::AudioProcessorValueTreeState::ParameterLayout Epsilon808AudioProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Attr = juce::AudioParameterFloatAttributes;
    const auto text = [](std::function<juce::String(float)> f) { return Attr {}.withStringFromValueFunction([f](float v, int) { return f(v); }); };
    const auto skewed = [](float lo, float hi, float centre) { juce::NormalisableRange<float> r { lo, hi }; r.setSkewForCentre(centre); return r; };
    auto add = [&layout](const char* id, const char* name, juce::NormalisableRange<float> range, float initial, Attr attributes)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, range, initial, attributes));
    };
    auto choice = [&layout](const char* id, const char* name, juce::StringArray options, int initial)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, 1 }, name, options, initial));
    };
    const auto percent = text([](float v) { return juce::String(juce::roundToInt(v)) + "%"; });
    const auto ms = text([](float v) { return msText(v); });

    // v0.9 parameters, in their original order (A/B slots and old sessions depend on it)
    add("tune", "Tune", { -24.0f, 24.0f, 1.0f }, 0.0f, text([](float v) { return (v > 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " st"; }));
    add("fine", "Fine", { -100.0f, 100.0f }, 0.0f, text([](float v) { return (v > 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " ct"; }));
    add("punch", "Punch", { 0.0f, 24.0f }, 12.0f, text([](float v) { return "+" + juce::String(v, 1) + " st"; }));
    add("punchTime", "Punch Time", skewed(5.0f, 300.0f, 50.0f), 45.0f, ms);
    add("tone", "Tone", { 0.0f, 100.0f }, 15.0f, percent);
    add("sub", "Sub", { 0.0f, 100.0f }, 0.0f, percent);
    add("click", "Click", { 0.0f, 100.0f }, 15.0f, percent);
    add("attack", "Attack", skewed(0.5f, 50.0f, 5.0f), 1.0f, ms);
    add("length", "Length", skewed(50.0f, 8000.0f, 1200.0f), 1600.0f, ms);
    add("sustain", "Sustain", { 0.0f, 100.0f }, 0.0f, percent);
    add("release", "Release", skewed(5.0f, 2000.0f, 200.0f), 120.0f, ms);
    add("glide", "Glide", skewed(0.0f, 1000.0f, 150.0f), 90.0f, ms);
    choice("glideMode", "Glide Mode", { "Legato", "Always", "Off" }, 0);
    add("bendRange", "Bend Range", { 1.0f, 24.0f, 1.0f }, 2.0f, text([](float v) { return juce::String(juce::roundToInt(v)) + " st"; }));
    add("drive", "Drive", { 0.0f, 36.0f }, 6.0f, text([](float v) { return "+" + juce::String(v, 1) + " dB"; }));
    choice("driveMode", "Drive Mode", { "Tape", "Tube", "Hard", "Fold" }, 0);
    add("driveMix", "Drive Mix", { 0.0f, 100.0f }, 100.0f, percent);
    juce::NormalisableRange<float> cutoff { 40.0f, 20000.0f };
    cutoff.setSkewForCentre(900.0f);
    add("cutoff", "Cutoff", cutoff, 20000.0f, text([](float v) { return v >= 19500.0f ? juce::String("Open") : hzText(v); }));
    add("resonance", "Resonance", { 0.0f, 100.0f }, 0.0f, percent);
    add("translate", "Translate", { 0.0f, 100.0f }, 0.0f, percent);
    add("glue", "Glue", { 0.0f, 100.0f }, 30.0f, percent);
    add("width", "Width", { 0.0f, 100.0f }, 0.0f, percent);
    add("output", "Output", { -24.0f, 12.0f }, 0.0f, text([](float v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "clip", 1 }, "Clip", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "phone", 1 }, "Phone Check", false));

    // v1.0 parameters
    add("analog", "Analog", { 0.0f, 100.0f }, 0.0f, percent);
    add("velPunch", "Velocity to Punch", { 0.0f, 100.0f }, 0.0f, percent);
    add("velDrive", "Velocity to Drive", { 0.0f, 100.0f }, 0.0f, percent);
    choice("priority", "Note Priority", { "Last", "Low", "High" }, 0);
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "retrigger", 1 }, "Retrigger", false));
    add("split", "Split", skewed(splitOff, 250.0f, 110.0f), splitOff, text([](float v) { return v <= splitOff + 0.5f ? juce::String("Off") : hzText(v); }));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "kickLock", 1 }, "Kick Lock", false));
    choice("kickMode", "Kick Lock Mode", { "Exact", "Live" }, 0);
    add("duck", "Duck", { 0.0f, 150.0f }, 60.0f, ms);
    choice("phoneModel", "Phone Check Device", { "Phone", "Laptop", "Earbuds", "Car" }, 0);
    return layout;
}

Epsilon808AudioProcessor::Epsilon808AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Kick", juce::AudioChannelSet::stereo(), false)),
      params(*this, &undoManager, "PARAMETERS", makeLayout())
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            params.addParameterListener(ranged->getParameterID(), this);
    loadFavourites();
}

Epsilon808AudioProcessor::~Epsilon808AudioProcessor()
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            params.removeParameterListener(ranged->getParameterID(), this);
}

bool Epsilon808AudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layout.inputBuses.isEmpty())
        return true;
    const auto kick = layout.getChannelSet(true, 0);
    return kick.isDisabled() || kick == juce::AudioChannelSet::mono() || kick == juce::AudioChannelSet::stereo();
}

void Epsilon808AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate);
    sampleRateForDisplay.store(sampleRate);
    keyboardState.reset();
    sidechainMono.assign(static_cast<std::size_t>(juce::jmax(samplesPerBlock, 4096)), 0.0f);
    queueHead = queueCount = 0;
    sampleClock = 0;
    currentLookahead = boom::Engine::lookaheadFor(readSettings(), sampleRate);
    setLatencySamples(boom::Engine::clipLatency + currentLookahead);
}

boom::Settings Epsilon808AudioProcessor::readSettings() const
{
    const auto v = [this](const char* id) { return params.getRawParameterValue(id)->load(std::memory_order_relaxed); };
    boom::Settings s;
    s.tuneSemitones = v("tune"); s.fineCents = v("fine");
    s.punchSemitones = v("punch"); s.punchMs = v("punchTime");
    s.tone = v("tone") * 0.01f; s.sub = v("sub") * 0.01f; s.click = v("click") * 0.01f;
    s.analog = v("analog") * 0.01f;
    s.attackMs = v("attack"); s.lengthMs = v("length"); s.sustain = v("sustain") * 0.01f; s.releaseMs = v("release");
    s.glideMs = v("glide"); s.glideMode = static_cast<boom::GlideMode>(juce::roundToInt(v("glideMode"))); s.bendRange = v("bendRange");
    s.retrigger = v("retrigger") > 0.5f;
    s.velPunch = v("velPunch") * 0.01f; s.velDrive = v("velDrive") * 0.01f;
    s.driveDb = v("drive"); s.driveMode = static_cast<boom::DriveMode>(juce::roundToInt(v("driveMode"))); s.driveMix = v("driveMix");
    const auto split = v("split");
    s.splitHz = split <= splitOff + 0.5f ? 0.0f : split;
    s.cutoffHz = v("cutoff"); s.resonance = v("resonance") * 0.01f;
    s.translate = v("translate"); s.glue = v("glue"); s.width = v("width"); s.outputDb = v("output");
    s.kickLock = v("kickLock") > 0.5f; s.kickMode = static_cast<boom::KickMode>(juce::roundToInt(v("kickMode"))); s.duckMs = v("duck");
    s.clip = v("clip") > 0.5f; s.phoneCheck = v("phone") > 0.5f;
    s.phoneModel = static_cast<boom::PhoneModel>(juce::roundToInt(v("phoneModel")));
    return s;
}

void Epsilon808AudioProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff(m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.pitchBend((static_cast<float>(m.getPitchWheelValue()) - 8192.0f) / 8192.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void Epsilon808AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto samples = buffer.getNumSamples();
    const auto settings = readSettings();
    engine.setPriority(static_cast<boom::Priority>(juce::roundToInt(params.getRawParameterValue("priority")->load())));

    // Kick Lock's Exact mode delays MIDI by 5 ms; the total delay is reported to the host.
    const auto lookahead = boom::Engine::lookaheadFor(settings, sampleRateForDisplay.load());
    if (lookahead != currentLookahead)
    {
        currentLookahead = lookahead;
        queueHead = queueCount = 0;
        engine.allNotesOff();
        setLatencySamples(boom::Engine::clipLatency + lookahead);
    }

    // Copy the kick (sidechain) to mono before the output overwrites the shared buffer.
    const float* sidechain = nullptr;
    auto* kickBus = getBusCount(true) > 0 ? getBus(true, 0) : nullptr;
    const auto routed = kickBus != nullptr && kickBus->isEnabled() && kickBus->getNumberOfChannels() > 0;
    kickRouted.store(routed);
    if (routed && samples <= static_cast<int>(sidechainMono.size()))
    {
        const auto kick = getBusBuffer(buffer, true, 0);
        const auto channels = kick.getNumChannels();
        float peak = 0.0f;
        for (int i = 0; i < samples; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c)
                sum += kick.getSample(c, i);
            sidechainMono[static_cast<std::size_t>(i)] = sum / static_cast<float>(channels);
            peak = juce::jmax(peak, std::abs(sum / static_cast<float>(channels)));
        }
        kickLevel.store(juce::jmax(peak, kickLevel.load() * 0.9f));
        sidechain = sidechainMono.data();
    }
    else
    {
        kickLevel.store(kickLevel.load() * 0.9f);
    }

    keyboardState.processNextMidiBuffer(midi, 0, samples, true);
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.getRawDataSize() > 3 || queueCount >= static_cast<int>(midiQueue.size()))
            continue;   // sysex and overflow are ignored; an 808 needs notes, bend and all-notes-off
        auto& slot = midiQueue[static_cast<std::size_t>((queueHead + queueCount) % static_cast<int>(midiQueue.size()))];
        slot.time = sampleClock + juce::jlimit(0, samples - 1, metadata.samplePosition) + lookahead;
        slot.size = message.getRawDataSize();
        std::memcpy(slot.data, message.getRawData(), static_cast<std::size_t>(slot.size));
        ++queueCount;
    }
    midi.clear();

    // Render between MIDI events so every note starts on its exact sample.
    std::array<float, 512> left {}, right {};
    int position = 0;
    auto renderTo = [&](int end)
    {
        while (position < end)
        {
            const auto count = std::min(end - position, static_cast<int>(left.size()));
            engine.render(left.data(), right.data(), count, settings, sidechain != nullptr ? sidechain + position : nullptr);
            if (buffer.getNumChannels() > 1 && getTotalNumOutputChannels() > 1)
            {
                buffer.copyFrom(0, position, left.data(), count);
                buffer.copyFrom(1, position, right.data(), count);
            }
            else
            {
                // mono output: the average keeps the sum exact (Width only adds opposite-polarity sides)
                for (int i = 0; i < count; ++i)
                    buffer.setSample(0, position + i, 0.5f * (left[static_cast<std::size_t>(i)] + right[static_cast<std::size_t>(i)]));
            }
            position += count;
        }
    };
    while (queueCount > 0)
    {
        const auto& event = midiQueue[static_cast<std::size_t>(queueHead)];
        const auto at = event.time - sampleClock;
        if (at >= samples)
            break;
        renderTo(static_cast<int>(juce::jmax<std::int64_t>(0, at)));
        handleMidi(juce::MidiMessage(event.data, event.size));
        queueHead = (queueHead + 1) % static_cast<int>(midiQueue.size());
        --queueCount;
    }
    renderTo(samples);
    for (int c = getTotalNumOutputChannels(); c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, samples);
    sampleClock += samples;
}

//==============================================================================
int Epsilon808AudioProcessor::getNumPrograms() { return static_cast<int>(factoryPresets().size()); }

const juce::String Epsilon808AudioProcessor::getProgramName(int index)
{
    return juce::isPositiveAndBelow(index, getNumPrograms()) ? factoryPresets()[static_cast<std::size_t>(index)].name : juce::String();
}

void Epsilon808AudioProcessor::resetSoundParameters()
{
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!isSessionParameter(ranged->getParameterID()))
                ranged->setValueNotifyingHost(ranged->getDefaultValue());
}

void Epsilon808AudioProcessor::setCurrentProgram(int index)
{
    if (!juce::isPositiveAndBelow(index, getNumPrograms()))
        return;
    PresetEntry entry;
    entry.factoryIndex = index;
    entry.name = factoryPresets()[static_cast<std::size_t>(index)].name;
    entry.category = factoryPresets()[static_cast<std::size_t>(index)].category;
    loadPreset(entry);
}

std::vector<PresetEntry> Epsilon808AudioProcessor::listPresets() const
{
    std::vector<PresetEntry> list;
    const auto& factory = factoryPresets();
    for (std::size_t i = 0; i < factory.size(); ++i)
    {
        PresetEntry e;
        e.name = factory[i].name;
        e.category = factory[i].category;
        e.note = factory[i].note;
        e.factoryIndex = static_cast<int>(i);
        list.push_back(e);
    }
    std::vector<PresetEntry> user;
    for (const auto& file : userPresetFolder().findChildFiles(juce::File::findFiles, false, "*.e808preset"))
        if (auto xml = juce::XmlDocument::parse(file); xml != nullptr && xml->hasTagName("E808Preset"))
        {
            PresetEntry e;
            e.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
            e.category = xml->getStringAttribute("category", "User");
            e.note = xml->getStringAttribute("note");
            e.file = file;
            user.push_back(e);
        }
    std::sort(user.begin(), user.end(), [](const auto& a, const auto& b) { return a.name.compareNatural(b.name) < 0; });
    list.insert(list.end(), user.begin(), user.end());
    return list;
}

void Epsilon808AudioProcessor::loadPreset(const PresetEntry& entry)
{
    undoManager.beginNewTransaction("Load preset");
    const juce::ScopedValueSetter<bool> quiet(suppressModified, true);
    if (!entry.isUser())
    {
        if (!juce::isPositiveAndBelow(entry.factoryIndex, getNumPrograms()))
            return;
        // every control goes back to its default, then the preset's own values are applied
        resetSoundParameters();
        for (const auto& [id, value] : factoryPresets()[static_cast<std::size_t>(entry.factoryIndex)].values)
            if (auto* parameter = params.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        currentFactory = entry.factoryIndex;
    }
    else
    {
        auto xml = juce::XmlDocument::parse(entry.file);
        if (xml == nullptr || !xml->hasTagName("E808Preset"))
            return;
        resetSoundParameters();
        for (auto* p : xml->getChildWithTagNameIterator("PARAM"))
            if (auto* parameter = params.getParameter(p->getStringAttribute("id")))
                if (!isSessionParameter(p->getStringAttribute("id")))
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(p->getDoubleAttribute("value"))));
        currentFactory = -1;
    }
    currentPresetName = entry.name;
    presetModified.store(false);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

bool Epsilon808AudioProcessor::saveUserPreset(const juce::String& rawName, const juce::String& category)
{
    const auto name = rawName.trim();
    if (name.isEmpty())
        return false;
    auto folder = userPresetFolder();
    if (!folder.createDirectory())
        return false;
    juce::XmlElement xml("E808Preset");
    xml.setAttribute("name", name);
    xml.setAttribute("category", category.isNotEmpty() ? category : juce::String("User"));
    xml.setAttribute("version", JucePlugin_VersionString);
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            if (!isSessionParameter(ranged->getParameterID()))
            {
                auto* p = xml.createNewChildElement("PARAM");
                p->setAttribute("id", ranged->getParameterID());
                p->setAttribute("value", ranged->convertFrom0to1(ranged->getValue()));
            }
    const auto file = folder.getChildFile(juce::File::createLegalFileName(name) + ".e808preset");
    if (!xml.writeTo(file))
        return false;
    currentPresetName = name;
    currentFactory = -1;
    presetModified.store(false);
    return true;
}

bool Epsilon808AudioProcessor::deleteUserPreset(const PresetEntry& entry)
{
    if (!entry.isUser() || !entry.file.existsAsFile())
        return false;
    if (favourites.contains(entry.key()))
        toggleFavourite(entry);
    return entry.file.deleteFile();
}

void Epsilon808AudioProcessor::loadFavourites()
{
    favourites.clear();
    favourites.addLines(userPresetFolder().getChildFile("favourites.txt").loadFileAsString());
    favourites.removeEmptyStrings();
}

bool Epsilon808AudioProcessor::isFavourite(const PresetEntry& entry) const { return favourites.contains(entry.key()); }

void Epsilon808AudioProcessor::toggleFavourite(const PresetEntry& entry)
{
    if (favourites.contains(entry.key()))
        favourites.removeString(entry.key());
    else
        favourites.add(entry.key());
    if (userPresetFolder().createDirectory())
        userPresetFolder().getChildFile("favourites.txt").replaceWithText(favourites.joinIntoString("\n"));
}

void Epsilon808AudioProcessor::parameterChanged(const juce::String& id, float)
{
    // Phone Check and Kick Lock routing are listening and session aids, not part of the sound
    if (!suppressModified && !isSessionParameter(id))
        presetModified.store(true);
}

//==============================================================================
std::vector<float> Epsilon808AudioProcessor::captureSettings() const
{
    std::vector<float> values;
    for (auto* parameter : getParameters())
        values.push_back(parameter->getValue());
    return values;
}

void Epsilon808AudioProcessor::applySettings(const std::vector<float>& values)
{
    const juce::ScopedValueSetter<bool> quiet(suppressModified, true);
    const auto& list = getParameters();
    for (int index = 0; index < list.size() && index < static_cast<int>(values.size()); ++index)
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(list[index]))
            if (!isSessionParameter(ranged->getParameterID()))
                ranged->setValueNotifyingHost(values[static_cast<std::size_t>(index)]);
}

void Epsilon808AudioProcessor::selectSlot(int slot)
{
    slot = juce::jlimit(0, 1, slot);
    if (slot == activeSlot)
        return;
    undoManager.beginNewTransaction("A/B");
    slots[static_cast<std::size_t>(activeSlot)] = captureSettings();
    auto& target = slots[static_cast<std::size_t>(slot)];
    if (target.empty())
        target = slots[static_cast<std::size_t>(activeSlot)];
    activeSlot = slot;
    applySettings(target);
}

void Epsilon808AudioProcessor::copyActiveSlotToOther()
{
    slots[static_cast<std::size_t>(1 - activeSlot)] = captureSettings();
}

juce::AudioProcessorEditor* Epsilon808AudioProcessor::createEditor() { return new Epsilon808AudioProcessorEditor(*this); }

void Epsilon808AudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = params.copyState();
    state.setProperty("preset", currentFactory, nullptr);
    state.setProperty("presetName", currentPresetName, nullptr);
    state.setProperty("presetModified", presetModified.load(), nullptr);
    for (auto child = state.getChildWithName("AB"); child.isValid(); child = state.getChildWithName("AB"))
        state.removeChild(child, nullptr);
    slots[static_cast<std::size_t>(activeSlot)] = captureSettings();
    juce::ValueTree ab("AB");
    ab.setProperty("active", activeSlot, nullptr);
    for (int slot = 0; slot < 2; ++slot)
    {
        juce::StringArray numbers;
        for (const auto value : slots[static_cast<std::size_t>(slot)])
            numbers.add(juce::String(value, 6));
        ab.setProperty(slot == 0 ? "a" : "b", numbers.joinIntoString(","), nullptr);
    }
    state.appendChild(ab, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void Epsilon808AudioProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(params.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml(*xml);
    currentFactory = juce::jlimit(-1, getNumPrograms() - 1, static_cast<int>(tree.getProperty("preset", 0)));
    currentPresetName = tree.getProperty("presetName", currentFactory >= 0 ? getProgramName(currentFactory) : juce::String("Init 808")).toString();
    slots = {};
    activeSlot = 0;
    if (auto ab = tree.getChildWithName("AB"); ab.isValid())
    {
        activeSlot = juce::jlimit(0, 1, static_cast<int>(ab.getProperty("active", 0)));
        for (int slot = 0; slot < 2; ++slot)
        {
            const auto csv = ab.getProperty(slot == 0 ? "a" : "b").toString();
            if (csv.isNotEmpty())
                for (const auto& number : juce::StringArray::fromTokens(csv, ",", {}))
                    slots[static_cast<std::size_t>(slot)].push_back(number.getFloatValue());
        }
        tree.removeChild(ab, nullptr);
    }
    {
        const juce::ScopedValueSetter<bool> quiet(suppressModified, true);
        params.replaceState(tree);
        for (auto* parameter : getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            {
                const auto stored = ranged->convertTo0to1(ranged->convertFrom0to1(ranged->getValue()));
                if (!juce::exactlyEqual(stored, ranged->getValue()))
                    ranged->setValueNotifyingHost(stored);
            }
    }
    undoManager.clearUndoHistory();
    presetModified.store(static_cast<bool>(tree.getProperty("presetModified", false)));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Epsilon808AudioProcessor(); }
