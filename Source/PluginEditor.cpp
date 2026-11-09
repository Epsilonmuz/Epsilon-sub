#include "PluginEditor.h"
#include "BoomUI.h"

using namespace boomui;

namespace
{
enum Group { body = 0, envelope = 1, effects = 2, play = 3, kick = 4 };

struct KnobSpec
{
    const char* id;
    const char* name;
    Group group;
    int simpleSlot;      // position in Simple view, -1 = Advanced only
    juce::Colour colour;
    const char* tip;
};

const std::vector<KnobSpec>& knobSpecs()
{
    static const std::vector<KnobSpec> specs {
        { "tune", "TUNE", body, 0, palette::magma, "Transpose in semitones. Set it so the root of your track sits on the note you play." },
        { "fine", "FINE", body, -1, palette::magma, "Fine tuning in cents, for samples or tracks that are not at A440." },
        { "punch", "PUNCH", body, 1, palette::magma, "How far above the note the pitch starts, then drops. This is the knock at the front of an 808." },
        { "punchTime", "PUNCH TIME", body, -1, palette::magma, "How long the pitch takes to fall back to the note. Short = click, long = zap." },
        { "tone", "TONE", body, -1, palette::magma, "Adds 2nd and 3rd harmonics to the body, so the note reads on small speakers." },
        { "sub", "SUB", body, -1, palette::magma, "Blends in a sine one octave below. Use sparingly; it eats headroom." },
        { "click", "CLICK", body, -1, palette::magma, "A short transient at the start of each note, for definition over a busy kick." },
        { "analog", "ANALOG", body, -1, palette::magma, "Behaves like the hardware: the pitch droops a little as the tail fades, each note is a few cents different, the attack is rounder. 0% = exact." },
        { "attack", "ATTACK", envelope, -1, palette::text, "Fade-in time. Raise it a little to soften the front of the note." },
        { "length", "LENGTH", envelope, 2, palette::text, "How long the note rings (time to fall by 60 dB). Long for trap, short for drill and bounce." },
        { "sustain", "SUSTAIN", envelope, -1, palette::text, "Level held while the key is down. 0% = a classic one-shot decay." },
        { "release", "RELEASE", envelope, -1, palette::text, "Fade-out after the key is released." },
        { "glide", "GLIDE", envelope, 4, palette::text, "Time to slide between notes. Overlap notes to slide (Legato), or slide on every note (Always)." },
        { "bendRange", "BEND", envelope, -1, palette::text, "Pitch-bend wheel range in semitones." },
        { "drive", "DRIVE", effects, 3, palette::ember, "Saturation. Pick the flavour above: Tape (warm), Tube (thick), Hard (aggressive), Fold (growl)." },
        { "driveMix", "DRIVE MIX", effects, -1, palette::ember, "Blend clean and driven signal." },
        { "split", "SPLIT", effects, -1, palette::ember, "Keeps the sub clean: below this frequency you hear the undistorted 808, above it the driven one. Off = drive everything." },
        { "cutoff", "CUTOFF", effects, -1, palette::ember, "Low-pass filter after the drive. Pull it down to tame fizz." },
        { "resonance", "RESONANCE", effects, -1, palette::ember, "Peak at the cutoff. Adds a vocal 'wow' when the cutoff is low." },
        { "translate", "TRANSLATE", effects, 5, palette::glass, "Adds harmonics of the sub (180 Hz - 3 kHz) so the 808 is heard on phones and laptops. Check it with Phone Check." },
        { "glue", "GLUE", effects, 6, palette::ember, "One-knob compressor. Evens out the tail so it sits under the mix." },
        { "width", "WIDTH", effects, -1, palette::ember, "Widens the harmonics above 160 Hz only. The sub stays mono, so it is safe in clubs." },
        { "output", "OUTPUT", effects, -1, palette::text, "Output level. With Clip on, peaks stay under 0 dBFS." },
        { "velPunch", "VEL > PUNCH", play, -1, palette::text, "How much velocity scales Punch. Soft notes knock less, accents knock more." },
        { "velDrive", "VEL > DRIVE", play, -1, palette::text, "How much velocity scales Drive. Accents get dirtier." },
        { "duck", "DUCK", kick, -1, palette::glass, "How long Kick Lock ducks the 808 after each kick, so the kick's front comes through. 0 = no duck, only phase lock." },
    };
    return specs;
}

float defaultPlainValue(juce::AudioProcessorValueTreeState& state, const char* id)
{
    auto* p = state.getParameter(id);
    return p->convertFrom0to1(p->getDefaultValue());
}
}

//==============================================================================
class Epsilon808AudioProcessorEditor::Panel final : public juce::Component
{
public:
    Panel(Epsilon808AudioProcessor& p, BoomLook& look)
        : processor(p),
          boomLook(look),
          hub(p),
          cone(hub, p.params),
          tail(hub),
          driveMode(*p.params.getParameter("driveMode"), { "TAPE", "TUBE", "HARD", "FOLD" }, palette::ember),
          glideMode(*p.params.getParameter("glideMode"), { "LEGATO", "ALWAYS", "OFF" }, palette::text),
          priority(*p.params.getParameter("priority"), { "LAST", "LOW", "HIGH" }, palette::text),
          kickMode(*p.params.getParameter("kickMode"), { "EXACT", "LIVE" }, palette::glass),
          clip("CLIP OFF", palette::magma, *p.params.getParameter("clip"), "CLIP ON"),
          retrigger("RETRIGGER", palette::magma, *p.params.getParameter("retrigger"), "RETRIGGER ON"),
          phone(*p.params.getParameter("phone"), *p.params.getParameter("phoneModel")),
          kickButton(p, hub),
          keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
          browser(p)
    {
        setOpaque(true);
        hub.onUpdate = [this]
        {
            cone.repaint();
            tail.repaint();
            if (hub.kickFlash > 0.04f || kickButton.getToggleState())
                kickButton.repaint();
        };
        for (auto* c : std::initializer_list<juce::Component*> { &cone, &tail, &driveMode, &glideMode, &priority, &kickMode, &clip, &retrigger,
                                                                &phone, &kickButton, &advanced, &abSwitch, &keyboard, &keysButton,
                                                                &presetButton, &previous, &next, &undo, &redo })
            addAndMakeVisible(c);
        addChildComponent(browser);
        addChildComponent(kickStatus);

        driveMode.setTooltip("Drive flavour. TAPE: warm and round. TUBE: thick, adds even harmonics. HARD: aggressive clipping. FOLD: wavefolder growl.");
        glideMode.setTooltip("LEGATO: slide only when notes overlap. ALWAYS: slide into every note. OFF: no slides.");
        priority.setTooltip("Which held note sounds. LAST: the newest key. LOW: the lowest key (handy for drill lines). HIGH: the highest key.");
        kickMode.setTooltip("EXACT: Kick Lock looks 5 ms ahead and lines up perfectly (the host compensates the delay).\n"
                            "LIVE: no added delay, for playing live; the lock lands a moment later.");
        clip.setTooltip("Anti-aliased soft clipper on the output. Keeps peaks under 0 dBFS and adds loudness.");
        retrigger.setTooltip("Every note gets a fresh punch, even when notes overlap and the pitch glides.");
        kickButton.setTooltip("Kick Lock: route your kick track into Epsilon 808's sidechain input. On every kick the 808 starts its cycle in phase\n"
                              "with the kick (so they add up instead of cancelling) and ducks for the Duck time.");
        advanced.setTooltip("Show every control: body, envelope, effects, playing and Kick Lock.");
        abSwitch.onSelect = [this](int slot) { processor.selectSlot(slot); syncPreset(); };
        abSwitch.onCopy = [this] { processor.copyActiveSlotToOther(); };
        abSwitch.setTooltip("A/B: two complete settings to compare. Click A or B to switch; the arrow copies the active one to the other.");
        advanced.onClick = [this]
        {
            processor.params.state.setProperty("advancedView", !showAdvanced(), nullptr);
            applyView();
        };

        const auto& specs = knobSpecs();
        knobs.resize(specs.size());
        labels.resize(specs.size());
        attachments.resize(specs.size());
        for (std::size_t index = 0; index < specs.size(); ++index)
        {
            const auto& spec = specs[index];
            auto& knob = knobs[index];
            knob = std::make_unique<juce::Slider>();
            knob->setLookAndFeel(&look);
            knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob->setDoubleClickReturnValue(true, defaultPlainValue(p.params, spec.id));
            knob->setColour(juce::Slider::rotarySliderFillColourId, spec.colour);
            knob->getProperties().set("bipolar", juce::String(spec.id) == "tune" || juce::String(spec.id) == "fine" || juce::String(spec.id) == "output");
            knob->setTitle(spec.name);
            knob->setTooltip(spec.tip);
            addAndMakeVisible(*knob);
            attachments[index] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.params, spec.id, *knob);

            auto& label = labels[index];
            label = std::make_unique<juce::Label>();
            label->setText(spec.name, juce::dontSendNotification);
            label->setJustificationType(juce::Justification::centred);
            label->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*label);
        }

        // preset bar: the name opens the browser, arrows step through every preset
        presetButton.onClick = [this] { showBrowser(!browser.isVisible()); };
        presetButton.setTooltip("Click to browse, search, favourite and save presets.");
        for (auto* button : { &previous, &next, &undo, &redo })
            button->setLookAndFeel(&look);
        previous.onClick = [this] { step(-1); };
        next.onClick = [this] { step(1); };
        undo.onClick = [this] { processor.undoManager.undo(); };
        redo.onClick = [this] { processor.undoManager.redo(); };
        undo.setTooltip("Undo (Cmd/Ctrl+Z)");
        redo.setTooltip("Redo (Cmd/Ctrl+Shift+Z)");
        browser.onClose = [this] { showBrowser(false); };
        browser.onLoaded = [this] { syncPreset(); };

        kickStatus.setFont(font(13.0f, Weight::semibold));
        kickStatus.setJustificationType(juce::Justification::centredLeft);
        kickStatus.setInterceptsMouseClicks(false, false);

        keyboard.setAvailableRange(24, 71);            // C1 - B4: where 808s live
        keyboard.setLowestVisibleKey(24);
        keyboard.setScrollButtonsVisible(false);
        keyboard.setOctaveForMiddleC(4);
        keyboard.setVelocity(0.9f, true);              // softer near the top of a key, harder near the bottom
        keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xff26222a));
        keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff09080a));
        keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, palette::background);
        keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, palette::magma.withAlpha(0.25f));
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, palette::magma.withAlpha(0.85f));
        keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, palette::muted);
        keyboard.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.4f));
        keyboard.setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, palette::plot);
        keysButton.onClick = [this] { setComputerKeys(!computerKeys()); };
        keysButton.setTooltip("Play from your computer keyboard: A W S E D F T G Y H U J K..., Z / X change octave.\n"
                              "Off by default inside a DAW so your shortcuts keep working.");
        setComputerKeys(computerKeys());

        // every click starts a new undo step
        addMouseListener(this, true);
        setWantsKeyboardFocus(true);
        applyView();
    }

    ~Panel() override
    {
        removeMouseListener(this);
        hub.onUpdate = nullptr;
        for (auto& knob : knobs)
            knob->setLookAndFeel(nullptr);
        for (auto* button : { &previous, &next, &undo, &redo })
            button->setLookAndFeel(nullptr);
    }

    void mouseDown(const juce::MouseEvent&) override { processor.undoManager.beginNewTransaction(); }

    void syncPreset()
    {
        // The preset name gets an asterisk once any control has moved since it was loaded.
        const auto wanted = processor.getCurrentPresetName() + (processor.isPresetModified() ? " *" : "");
        if (presetButton.getButtonText() != wanted)
            presetButton.setButtonText(wanted);
        abSwitch.setActive(processor.getActiveSlot());
        undo.setEnabled(processor.undoManager.canUndo());
        redo.setEnabled(processor.undoManager.canRedo());
        kickButton.repaint();
        updateKickStatus();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto mods = key.getModifiers();
        if (mods.isCommandDown() && key.getKeyCode() == 'Z')
        {
            if (mods.isShiftDown()) processor.undoManager.redo(); else processor.undoManager.undo();
            return true;
        }
        if (!computerKeys() || mods.isCommandDown() || mods.isAltDown())
            return false;
        // Z / X shift the computer-keyboard octave, like most DAWs.
        const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
        if (c != 'z' && c != 'x')
            return false;
        keyOctave = juce::jlimit(1, 3, keyOctave + (c == 'x' ? 1 : -1));
        processor.keyboardState.allNotesOff(0);
        keyboard.setKeyPressBaseOctave(keyOctave + 1);
        keysButton.repaint();
        return true;
    }

    void grabKeys()
    {
        if (isShowing() && computerKeys())
            keyboard.grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(palette::background);

        // A low ember glow behind the logo: the 808 signature
        g.setGradientFill(juce::ColourGradient(palette::magma.withAlpha(0.20f), 0.0f, 0.0f,
                                               palette::magma.withAlpha(0.0f), 260.0f, 0.0f, false));
        g.fillRect(0, 0, 260, 64);

        g.setColour(palette::text);
        g.setFont(font(14.0f, Weight::bold));
        g.drawText(juce::CharPointer_UTF8("E\xce\xa3  EPSILON SOUNDS"), 24, 9, 180, 18, juce::Justification::centredLeft);
        g.setFont(font(42.0f, Weight::bold));
        g.drawText("808", 22, 22, 80, 40, juce::Justification::centredLeft);
        g.setColour(palette::magma);
        g.fillEllipse(96.0f, 48.0f, 9.0f, 9.0f);
        g.setColour(palette::muted);
        g.setFont(font(14.0f, Weight::semibold));
        g.drawText("SUB BASS INSTRUMENT", 116, 40, 160, 18, juce::Justification::centredLeft);
        g.setColour(palette::dim);
        g.drawText("v" JucePlugin_VersionString, 116, 22, 60, 18, juce::Justification::centredLeft);

        if (!showAdvanced() && !browser.isVisible())
        {
            g.setColour(palette::dim);
            g.setFont(font(13.5f, Weight::semibold));
            const auto hint = computerKeys()
                ? juce::String(juce::CharPointer_UTF8("PLAY: click the keys, or type A W S E D F G\xe2\x80\xa6  \xc2\xb7  Z / X change octave  \xc2\xb7  hold a note and press another to glide"))
                : juce::String(juce::CharPointer_UTF8("PLAY: click the keys  \xc2\xb7  turn on KEYS (left) to play from your computer keyboard  \xc2\xb7  hold a note and press another to glide"));
            g.drawText(hint, 22, 650, getWidth() - 44, 18, juce::Justification::centred);
        }
        if (showAdvanced() && !browser.isVisible())
        {
            g.setFont(font(13.0f, Weight::bold));
            const auto heading = [&](const char* text, juce::Colour colour, int x0, int x1, int y)
            {
                g.setColour(colour);
                g.drawText(text, x0, y, 120, 14, juce::Justification::centredLeft);
                g.setColour(palette::edge);
                g.drawHorizontalLine(y + 7, static_cast<float>(x0) + juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 8.0f, static_cast<float>(x1));
            };
            heading("BODY", palette::magma, groupX[0], groupX[1] - 14, rowOneY - 18);
            heading("ENVELOPE", palette::text, groupX[1], getWidth() - 22, rowOneY - 18);
            heading("EFFECTS", palette::ember, 22, getWidth() - 22, rowTwoY - 18);
            heading("PLAYING", palette::text, 22, kickX - 14, rowThreeY - 18);
            heading("KICK LOCK", palette::glass, kickX, getWidth() - 22, rowThreeY - 18);
            g.setFont(font(12.0f, Weight::bold));
            g.setColour(palette::muted);
            g.drawText("NOTE PRIORITY", priority.getX(), priority.getY() - 16, priority.getWidth(), 14, juce::Justification::centred);
            g.drawText("MODE", kickMode.getX(), kickMode.getY() - 16, kickMode.getWidth(), 14, juce::Justification::centred);
        }
    }

    void resized() override
    {
        const auto width = getWidth();
        // header
        phone.setBounds(width - 22 - 160, 18, 160, 30);
        abSwitch.setBounds(phone.getX() - 12 - 120, 18, 120, 30);
        next.setBounds(abSwitch.getX() - 12 - 34, 16, 34, 34);
        presetButton.setBounds(next.getX() - 4 - 218, 16, 218, 34);
        previous.setBounds(presetButton.getX() - 4 - 34, 16, 34, 34);
        redo.setBounds(previous.getX() - 10 - 30, 18, 30, 30);
        undo.setBounds(redo.getX() - 4 - 30, 18, 30, 30);

        cone.setBounds(22, 68, 300, 240);
        tail.setBounds(334, 68, width - 334 - 22, 240);

        // toggle row: spread evenly
        const int toggleY = 318;
        std::array<juce::Component*, 5> row { &advanced, &driveMode, &glideMode, &kickButton, &clip };
        std::array<int, 5> widths { 150, 250, 210, 170, 104 };
        int total = 0;
        for (auto w : widths) total += w;
        const auto gap = (width - 44 - total) / 4;
        int x = 22;
        for (std::size_t i = 0; i < row.size(); ++i)
        {
            row[i]->setBounds(x, toggleY, widths[i], 30);
            x += widths[i] + gap;
        }

        browser.setBounds(22, 360, width - 44, 310);
        keysButton.setBounds(22, 680, 40, 70);
        keyboard.setBounds(68, 680, width - 22 - 68, 70);
        keyboard.setKeyWidth(static_cast<float>(keyboard.getWidth()) / 28.0f);   // 4 octaves of white keys
        layoutKnobs();
    }

private:
    class ABSwitch final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        std::function<void(int)> onSelect;
        std::function<void()> onCopy;
        void setActive(int slot) { if (slot != active) { active = slot; repaint(); } }
        void paint(juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(palette::plot);
            g.fillRoundedRectangle(area, 7.0f);
            g.setColour(palette::edge);
            g.drawRoundedRectangle(area, 7.0f, 1.0f);
            for (int index = 0; index < 3; ++index)
            {
                const auto cell = cellAt(index);
                if (index == active)
                {
                    g.setColour(palette::edge);
                    g.fillRoundedRectangle(cell.reduced(3.0f), 5.0f);
                }
                g.setColour(index == active ? palette::text : palette::muted);
                g.setFont(font(index < 2 ? 15.0f : 13.0f, Weight::bold));
                const juce::String words = index == 0 ? "A" : index == 1 ? "B"
                                         : juce::String(juce::CharPointer_UTF8(active == 0 ? "A\xe2\x86\x92" "B" : "B\xe2\x86\x92" "A"));
                g.drawText(words, cell, juce::Justification::centred);
            }
        }
        void mouseDown(const juce::MouseEvent& event) override
        {
            for (int index = 0; index < 3; ++index)
                if (cellAt(index).contains(event.position))
                {
                    if (index < 2 && onSelect) onSelect(index);
                    if (index == 2 && onCopy) onCopy();
                }
        }
    private:
        juce::Rectangle<float> cellAt(int index) const
        {
            const auto w = static_cast<float>(getWidth());
            const auto h = static_cast<float>(getHeight());
            return index == 0 ? juce::Rectangle<float>(0.0f, 0.0f, w * 0.3f, h)
                 : index == 1 ? juce::Rectangle<float>(w * 0.3f, 0.0f, w * 0.3f, h)
                              : juce::Rectangle<float>(w * 0.6f, 0.0f, w * 0.4f, h);
        }
        int active = 0;
    };

    class AdvancedButton final : public juce::Button
    {
    public:
        AdvancedButton() : juce::Button("ADVANCED") {}
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            paintPill(g, getLocalBounds().toFloat(), getToggleState(), over, palette::text,
                      getToggleState() ? "HIDE ADVANCED" : "SHOW ADVANCED");
        }
    };

    // The preset name in the header; opens the browser.
    class PresetButton final : public juce::Button
    {
    public:
        PresetButton() : juce::Button("Init 808") {}
        void paintButton(juce::Graphics& g, bool over, bool down) override
        {
            const auto area = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(down ? palette::edge : palette::plot);
            g.fillRoundedRectangle(area, 6.0f);
            g.setColour(over ? palette::dim : palette::edge);
            g.drawRoundedRectangle(area, 6.0f, 1.0f);
            g.setColour(palette::text);
            g.setFont(font(17.0f, Weight::semibold));
            g.drawFittedText(getButtonText(), getLocalBounds().reduced(28, 0), juce::Justification::centred, 1);
            juce::Path tri;
            const auto c = juce::Point<float>(area.getRight() - 16.0f, area.getCentreY());
            tri.addTriangle(c.x - 4.0f, c.y - 2.0f, c.x + 4.0f, c.y - 2.0f, c.x, c.y + 3.0f);
            g.setColour(palette::muted);
            g.fillPath(tri);
        }
    };

    // Computer-keyboard toggle beside the keys; shows the current octave.
    class KeysButton final : public juce::Button
    {
    public:
        explicit KeysButton(const int& octaveRef) : juce::Button("KEYS"), octave(octaveRef) {}
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            const auto on = getToggleState();
            const auto area = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(on ? palette::magma.withAlpha(0.16f) : palette::plot);
            g.fillRoundedRectangle(area, 6.0f);
            g.setColour(on ? palette::magma : over ? palette::dim : palette::edge);
            g.drawRoundedRectangle(area, 6.0f, 1.0f);
            // a small keyboard glyph
            auto glyph = area.withSizeKeepingCentre(22.0f, 14.0f).translated(0.0f, -12.0f);
            g.setColour(on ? palette::text : palette::muted);
            g.drawRoundedRectangle(glyph, 2.0f, 1.2f);
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 4; ++c)
                    g.fillRect(glyph.getX() + 3.0f + c * 4.4f, glyph.getY() + 3.0f + r * 4.5f, 2.6f, 2.4f);
            g.setFont(font(11.5f, Weight::bold));
            g.drawText(on ? "C" + juce::String(octave) : juce::String("OFF"), area.withTrimmedTop(area.getHeight() * 0.55f), juce::Justification::centredTop);
        }
    private:
        const int& octave;
    };

    bool showAdvanced() const { return static_cast<bool>(processor.params.state.getProperty("advancedView", false)); }
    bool computerKeys() const { return static_cast<bool>(processor.params.state.getProperty("computerKeys", false)); }

    void setComputerKeys(bool on)
    {
        processor.params.state.setProperty("computerKeys", on, nullptr);
        keysButton.setToggleState(on, juce::dontSendNotification);
        keyboard.setWantsKeyboardFocus(on);
        if (on)
        {
            keyboard.setKeyPressBaseOctave(keyOctave + 1);
            grabKeys();
        }
        else
        {
            processor.keyboardState.allNotesOff(0);
            if (keyboard.hasKeyboardFocus(true))
                giveAwayKeyboardFocus();
        }
        keysButton.repaint();
        repaint();
    }

    void showBrowser(bool show)
    {
        if (show)
            browser.refresh();
        browser.setVisible(show);
        if (show)
            browser.grabKeyboardFocus();
        applyView();
    }

    void updateKickStatus()
    {
        juce::String text;
        auto colour = palette::muted;
        if (!processor.isKickInputRouted())
            text = "Route your kick track to the sidechain input.";
        else if (processor.getKickInputLevel() < 0.001f)
            text = "Kick input routed, waiting for signal.";
        else
        {
            text = "Kick input: receiving.";
            colour = palette::glass;
        }
        if (processor.params.getRawParameterValue("kickLock")->load() < 0.5f)
            text = "Off. " + text;
        kickStatus.setText(text, juce::dontSendNotification);
        kickStatus.setColour(juce::Label::textColourId, colour);
    }

    void applyView()
    {
        const auto on = showAdvanced();
        const auto browsing = browser.isVisible();
        advanced.setToggleState(on, juce::dontSendNotification);
        const auto& specs = knobSpecs();
        for (std::size_t index = 0; index < specs.size(); ++index)
        {
            const auto visible = !browsing && (on || specs[index].simpleSlot >= 0);
            knobs[index]->setVisible(visible);
            labels[index]->setVisible(visible);
        }
        for (auto* c : std::initializer_list<juce::Component*> { &priority, &retrigger, &kickMode, &kickStatus })
            c->setVisible(on && !browsing);
        layoutKnobs();
        repaint();
    }

    void styleKnob(std::size_t index, bool hero, int textWidth, int textHeight, float labelSize)
    {
        auto& knob = *knobs[index];
        if (static_cast<bool>(knob.getProperties()["hero"]) != hero || knob.getTextBoxWidth() != textWidth || knob.getTextBoxHeight() != textHeight)
        {
            knob.getProperties().set("hero", hero);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textWidth, textHeight);
            knob.setLookAndFeel(nullptr);   // re-create the text box with the new font
            knob.setLookAndFeel(&boomLook);
        }
        auto& label = *labels[index];
        label.setFont(font(labelSize, Weight::bold));
        label.setColour(juce::Label::textColourId, hero ? palette::text : palette::muted);
    }

    void layoutKnobs()
    {
        const auto width = getWidth();
        const auto& specs = knobSpecs();
        if (!showAdvanced())
        {
            // Seven big controls; Drive is the hero in the middle.
            const auto cell = (width - 44) / 7;
            for (std::size_t index = 0; index < specs.size(); ++index)
            {
                const auto slot = specs[index].simpleSlot;
                if (slot < 0)
                    continue;
                const auto hero = slot == 3;
                const auto size = hero ? 130 : 88;
                const auto textHeight = hero ? 26 : 22;
                styleKnob(index, hero, hero ? 110 : 96, textHeight, hero ? 18.0f : 14.0f);
                const auto centreX = 22 + cell * slot + cell / 2;
                const auto top = hero ? 384 : 410;
                knobs[index]->setBounds(centreX - (size + 20) / 2, top, size + 20, size + textHeight);
                labels[index]->setBounds(centreX - 60, knobs[index]->getBottom(), 120, 20);
            }
            return;
        }

        // Row one: BODY (8) and ENVELOPE (6). Row two: EFFECTS (9). Row three: PLAYING and KICK LOCK.
        const auto rowOneCell = (width - 44 - 14) / 14;
        groupX = { 22, 22 + rowOneCell * 8 + 14 };
        kickX = width / 2 + 40;
        int counts[5] = { 0, 0, 0, 0, 0 };
        const auto fxCell = (width - 44) / 9;
        for (std::size_t index = 0; index < specs.size(); ++index)
        {
            const auto group = specs[index].group;
            const auto slot = counts[group]++;
            auto& knob = *knobs[index];
            auto& label = *labels[index];
            if (group == body || group == envelope)
            {
                const auto centreX = groupX[group == body ? 0 : 1] + rowOneCell * slot + rowOneCell / 2;
                styleKnob(index, false, rowOneCell, 18, 11.5f);
                knob.setBounds(centreX - rowOneCell / 2, rowOneY, rowOneCell, 46 + 18);
                label.setBounds(centreX - rowOneCell / 2 - 8, knob.getBottom(), rowOneCell + 16, 16);
            }
            else if (group == effects)
            {
                const auto hero = juce::String(specs[index].id) == "drive";
                const auto size = hero ? 60 : 50;
                const auto centreX = 22 + fxCell * slot + fxCell / 2;
                styleKnob(index, hero, fxCell, 18, 12.0f);
                knob.setBounds(centreX - fxCell / 2, rowTwoY - (hero ? 6 : 0), fxCell, size + 18);
                label.setBounds(centreX - fxCell / 2, knob.getBottom(), fxCell, 16);
            }
            else
            {
                const auto cell = 84;
                const auto centreX = (group == play ? 22 : kickX) + cell * slot + cell / 2;
                styleKnob(index, false, cell, 18, 11.5f);
                knob.setBounds(centreX - cell / 2, rowThreeY, cell, 44 + 18);
                label.setBounds(centreX - cell / 2 - 8, knob.getBottom(), cell + 16, 16);
            }
        }
        priority.setBounds(22 + 84 * 2 + 16, rowThreeY + 24, 168, 28);
        retrigger.setBounds(priority.getRight() + 12, rowThreeY + 24, 128, 28);
        kickMode.setBounds(kickX + 84 + 12, rowThreeY + 24, 130, 28);
        kickStatus.setBounds(kickMode.getRight() + 12, rowThreeY + 18, width - 22 - kickMode.getRight() - 12, 40);
    }

    void step(int direction)
    {
        const auto list = processor.listPresets();
        if (list.empty())
            return;
        int current = 0;
        for (std::size_t i = 0; i < list.size(); ++i)
            if (list[i].name == processor.getCurrentPresetName()) current = static_cast<int>(i);
        const auto count = static_cast<int>(list.size());
        processor.loadPreset(list[static_cast<std::size_t>((current + direction + count) % count)]);
        syncPreset();
        if (browser.isVisible())
            browser.refresh();
    }

    Epsilon808AudioProcessor& processor;
    BoomLook& boomLook;
    Hub hub;
    ConeView cone;
    TailView tail;
    Segments driveMode, glideMode, priority, kickMode;
    PillToggle clip, retrigger;
    PhoneButton phone;
    KickButton kickButton;
    AdvancedButton advanced;
    ABSwitch abSwitch;
    juce::MidiKeyboardComponent keyboard;
    int keyOctave = 1;   // computer keys start at C1
    KeysButton keysButton { keyOctave };
    PresetBrowser browser;
    PresetButton presetButton;
    juce::Label kickStatus;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::TextButton previous { juce::CharPointer_UTF8("\xe2\x97\x80") }, next { juce::CharPointer_UTF8("\xe2\x96\xb6") };
    juce::TextButton undo { juce::CharPointer_UTF8("\xe2\x86\xb6") }, redo { juce::CharPointer_UTF8("\xe2\x86\xb7") };
    juce::TooltipWindow tooltips { this, 600 };
    std::array<int, 2> groupX { 22, 560 };
    int kickX = 530;
    static constexpr int rowOneY = 378;
    static constexpr int rowTwoY = 484;
    static constexpr int rowThreeY = 594;
};

//==============================================================================
Epsilon808AudioProcessorEditor::Epsilon808AudioProcessorEditor(Epsilon808AudioProcessor& p)
    : AudioProcessorEditor(&p),
      look(std::make_unique<BoomLook>()),
      panel(std::make_unique<Panel>(p, *look))
{
    setOpaque(true);
    setLookAndFeel(look.get());   // text fields, menus and buttons everywhere use the 808 look
    addAndMakeVisible(*panel);
    panel->setBounds(0, 0, designWidth, designHeight);

    setResizable(true, true);
    if (auto* sizeLimits = getConstrainer())
    {
        sizeLimits->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
        sizeLimits->setSizeLimits(designWidth * 75 / 100, designHeight * 75 / 100, designWidth * 2, designHeight * 2);
    }
    setSize(designWidth, designHeight);
    startTimerHz(4);
}

Epsilon808AudioProcessorEditor::~Epsilon808AudioProcessorEditor()
{
    stopTimer();
    panel = nullptr;
    setLookAndFeel(nullptr);
}

void Epsilon808AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void Epsilon808AudioProcessorEditor::resized()
{
    const auto scale = static_cast<float>(getWidth()) / static_cast<float>(designWidth);
    panel->setTransform(juce::AffineTransform::scale(scale));
}

void Epsilon808AudioProcessorEditor::timerCallback()
{
    panel->syncPreset();
    if (!focusGrabbed && isShowing())
    {
        panel->grabKeys();
        focusGrabbed = true;
    }
}
