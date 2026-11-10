#pragma once

// Epsilon 808 panel parts: palette, typeface, look, meter hub, speaker cone, tail view, small controls.
// Header-only; included by PluginEditor.cpp.

#include "PluginProcessor.h"
#include "../Shared/epsilon_ui/EpsilonHeader.h"
#include "../Shared/epsilon_ui/EpsilonSignature.h"

#include <BinaryData.h>
#include <cmath>

namespace boomui
{
namespace palette
{
// Epsilon Sounds design system v1 (Epsilon tier). Glass stays: it marks harmonics / Phone Check.
inline const epsilon_ui::Palette brand = epsilon_ui::palette (epsilon_ui::Tier::Epsilon);
inline const juce::Colour background = brand.bg;
inline const juce::Colour panel      = brand.surface;
inline const juce::Colour plot       = brand.bg.darker (0.15f);
inline const juce::Colour edge       = brand.line;
inline const juce::Colour text       = brand.ink;   // bone
inline const juce::Colour muted      = brand.inkMuted;
inline const juce::Colour dim        = epsilon_ui::faint (brand);
inline const juce::Colour magma      = brand.live;   // the 808 itself: level, punch, the hero knob
inline const juce::Colour ember      = brand.meterHot;   // drive and saturation
inline const juce::Colour glass      { 0xff6fe3d1 };   // Translate harmonics and Phone Check
}

enum class Weight { medium, semibold, bold };

inline juce::Font font(float height, Weight weight = Weight::medium)
{
    static const auto medium = juce::Typeface::createSystemTypefaceFor(
        BinaryData::EpsilonDialRegular_ttf, BinaryData::EpsilonDialRegular_ttfSize);
    static const auto semibold = juce::Typeface::createSystemTypefaceFor(
        BinaryData::EpsilonDialBold_ttf, BinaryData::EpsilonDialBold_ttfSize);
    static const auto bold = juce::Typeface::createSystemTypefaceFor(
        BinaryData::EpsilonDialBold_ttf, BinaryData::EpsilonDialBold_ttfSize);
    const auto& face = weight == Weight::bold ? bold : weight == Weight::semibold ? semibold : medium;
    return juce::Font(juce::FontOptions(face).withHeight(height));
}

inline juce::String noteName(float note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto n = juce::roundToInt(note);
    return juce::String(names[((n % 12) + 12) % 12]) + juce::String(n / 12 - 1);
}

//==============================================================================
class BoomLook final : public juce::LookAndFeel_V4
{
public:
    BoomLook()
    {
        setColour(juce::ComboBox::backgroundColourId, palette::plot);
        setColour(juce::ComboBox::outlineColourId, palette::edge);
        setColour(juce::ComboBox::textColourId, palette::text);
        setColour(juce::ComboBox::arrowColourId, palette::muted);
        setColour(juce::PopupMenu::backgroundColourId, palette::panel);
        setColour(juce::PopupMenu::textColourId, palette::text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, palette::edge);
        setColour(juce::PopupMenu::highlightedTextColourId, palette::text);
        setColour(juce::Slider::textBoxTextColourId, palette::text);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxHighlightColourId, palette::edge);
        setColour(juce::TextEditor::backgroundColourId, palette::plot);
        setColour(juce::TextEditor::textColourId, palette::text);
        setColour(juce::TextEditor::focusedOutlineColourId, palette::edge);
        setColour(juce::CaretComponent::caretColourId, palette::text);
        setColour(juce::TooltipWindow::backgroundColourId, palette::panel);
        setColour(juce::TooltipWindow::textColourId, palette::text);
        setColour(juce::TooltipWindow::outlineColourId, palette::edge);
        setColour(juce::TextButton::buttonColourId, palette::plot);
        setColour(juce::TextButton::textColourOnId, palette::text);
        setColour(juce::TextEditor::outlineColourId, palette::edge);
        setColour(juce::TextEditor::highlightColourId, palette::magma.withAlpha(0.3f));
        setColour(juce::ScrollBar::thumbColourId, palette::dim);
        setColour(juce::ListBox::backgroundColourId, palette::plot);
        setColour(juce::TextButton::textColourOffId, palette::muted);
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override
    {
        return font(f.getHeight(), f.isBold() ? Weight::bold : Weight::medium).getTypefacePtr();
    }

    // Epsilon Sounds signature coin-edge knob (static layers cached)
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float value, float, float, juce::Slider& slider) override
    {
        epsilon_ui::signature::knob(g, juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(4.0f),
                                    value, slider, epsilon_ui::Tier::Epsilon);
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(font(static_cast<bool>(slider.getProperties()["hero"]) ? 20.0f : 14.0f, Weight::semibold));
        label->setJustificationType(juce::Justification::centred);
        return label;
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        g.setColour(palette::plot);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(box.isMouseOver(true) ? palette::dim : palette::edge);
        g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(box.getLocalBounds().reduced(8, 1));
        label.setFont(font(17.0f, Weight::semibold));
        label.setJustificationType(juce::Justification::centred);
    }

    juce::Font getPopupMenuFont() override { return font(16.0f); }

    // Epsilon Sounds signature keycap; toggles carry the champagne lume bar when on
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
    {
        epsilon_ui::signature::keycap(g, button, over, down, epsilon_ui::Tier::Epsilon);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        epsilon_ui::signature::keycapText(g, b, getTextButtonFont(b, b.getHeight()));
    }
};

//==============================================================================
// Reads the engine once per frame for the cone and the tail view.
class Hub final : private juce::Timer
{
public:
    static constexpr int historySize = 240;   // 4 seconds at 60 frames per second

    explicit Hub(Epsilon808AudioProcessor& p) : processor(p)
    {
        processor.setMeteringEnabled(true);
        amp.fill(0.0f);
        notes.fill(-1.0f);
        duck.fill(0.0f);
        kicks.fill(false);
        startTimerHz(60);
    }
    ~Hub() override
    {
        stopTimer();
        processor.setMeteringEnabled(false);
    }

    std::function<void()> onUpdate;
    boom::MeterFrame frame;
    std::array<float, historySize> amp {}, notes {}, duck {};
    std::array<bool, historySize> kicks {};
    int head = 0;                 // index of the newest history entry
    float excursion = 0.0f, glow = 0.0f, harmonics = 0.0f, peakHold = 0.0f, grShown = 0.0f;
    float loudness = -70.0f;      // short-term loudness (3 s), LUFS
    float kickFlash = 0.0f;
    bool live = false;

private:
    void timerCallback() override
    {
        boom::MeterFrame f;
        bool got = false;
        double squares = 0.0;
        int frames = 0;
        bool kick = false;
        while (processor.readMeterFrame(f))
        {
            got = true;
            frame = f;
            squares += f.meanSquare;
            kick = kick || f.kick;
            ++frames;
        }
        if (!got)
        {
            frame.peakL = frame.peakR = frame.lowPeak = frame.harmonics = frame.gainReductionDb = frame.duck = 0.0f;
            frame.meanSquare = 0.0f;
        }
        live = got;
        head = (head + 1) % historySize;
        const auto h = static_cast<std::size_t>(head);
        amp[h] = juce::jmax(frame.peakL, frame.peakR);
        notes[h] = frame.envelope > 0.002f ? frame.note : -1.0f;
        duck[h] = frame.duck;
        kicks[h] = kick;
        squareHistory[h] = frames > 0 ? static_cast<float>(squares / frames) : 0.0f;
        double sum = 0.0;
        for (int i = 0; i < 180; ++i)   // 3 seconds
            sum += squareHistory[static_cast<std::size_t>((head - i + historySize) % historySize)];
        loudness = sum > 1.0e-12 ? juce::jmax(-70.0f, static_cast<float>(-0.691 + 10.0 * std::log10(sum / 180.0))) : -70.0f;
        excursion += 0.35f * (frame.lowPeak - excursion);
        glow += 0.25f * (frame.envelope - glow);
        harmonics += 0.2f * (frame.harmonics * 3.0f - harmonics);
        peakHold = juce::jmax(peakHold * 0.97f, juce::jmax(frame.peakL, frame.peakR));
        grShown += 0.2f * (frame.gainReductionDb - grShown);
        kickFlash = kick ? 1.0f : kickFlash * 0.85f;
        if (onUpdate)
            onUpdate();
    }
    std::array<float, historySize> squareHistory {};
    Epsilon808AudioProcessor& processor;
};

//==============================================================================
// The hero: a speaker cone that moves with the low end, with the note and its tuning in the middle.
class ConeView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ConeView(Hub& h, juce::AudioProcessorValueTreeState& state) : hub(h), params(state)
    {
        setOpaque(true);
        setTooltip("The cone moves with the low end. The ring lights when Translate adds harmonics.\n"
                   "Underneath: the note playing, its frequency and how far it is from the nearest note.");
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(palette::background);
        auto b = getLocalBounds().toFloat();
        g.setColour(palette::panel);
        g.fillRoundedRectangle(b, 12.0f);
        g.setColour(params.getRawParameterValue("phone")->load() > 0.5f ? palette::glass : palette::edge);
        g.drawRoundedRectangle(b.reduced(0.5f), 12.0f, 1.0f);
        g.setColour(palette::text);
        g.setFont(font(15.0f, Weight::bold));
        g.drawText("SUB", juce::Rectangle<float>(16.0f, 10.0f, 80.0f, 22.0f), juce::Justification::centredLeft);
        if (params.getRawParameterValue("phone")->load() > 0.5f)
        {
            g.setColour(palette::glass);
            g.setFont(font(13.0f, Weight::bold));
            g.drawText("PHONE CHECK ON", juce::Rectangle<float>(b.getWidth() - 150.0f, 10.0f, 134.0f, 22.0f), juce::Justification::centredRight);
        }

        const auto area = b.withTrimmedTop(34.0f).withTrimmedBottom(70.0f);
        const auto c = area.getCentre();
        const auto R = juce::jmin(area.getWidth(), area.getHeight()) * 0.46f;
        const auto e = juce::jlimit(0.0f, 1.0f, hub.excursion * 1.4f);
        const auto glow = juce::jlimit(0.0f, 1.0f, hub.glow);

        // outer glow with the level
        g.setGradientFill(juce::ColourGradient(palette::magma.withAlpha(0.28f * glow), c, palette::magma.withAlpha(0.0f),
                                               c.translated(R * 1.25f, 0.0f), true));
        g.fillEllipse(juce::Rectangle<float>(R * 2.5f, R * 2.5f).withCentre(c));
        // frame with screws
        g.setColour(juce::Colour(0xff1d1a1f));
        g.fillEllipse(juce::Rectangle<float>(R * 2.0f, R * 2.0f).withCentre(c));
        g.setColour(palette::edge.brighter(0.2f));
        g.drawEllipse(juce::Rectangle<float>(R * 2.0f, R * 2.0f).withCentre(c), 1.2f);
        for (int i = 0; i < 8; ++i)
        {
            const auto p = c.getPointOnCircumference(R * 0.94f, juce::MathConstants<float>::twoPi * static_cast<float>(i) / 8.0f + 0.39f);
            g.setColour(juce::Colour(0xff3a333a));
            g.fillEllipse(juce::Rectangle<float>(5.0f, 5.0f).withCentre(p));
        }
        // surround (the rubber roll)
        const auto rs = R * 0.86f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3b343a), c.translated(-rs * 0.4f, -rs * 0.6f),
                                               juce::Colour(0xff0d0c0e), c.translated(rs * 0.5f, rs * 0.7f), false));
        g.fillEllipse(juce::Rectangle<float>(rs * 2.0f, rs * 2.0f).withCentre(c));
        // Translate ring
        if (hub.harmonics > 0.01f)
        {
            g.setColour(palette::glass.withAlpha(juce::jlimit(0.0f, 0.9f, hub.harmonics)));
            g.drawEllipse(juce::Rectangle<float>(R * 2.08f, R * 2.08f).withCentre(c), 2.5f);
        }
        // cone body pushes out with the low end
        const auto rc = R * 0.74f * (1.0f + 0.05f * e);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b262b), c, juce::Colour(0xff17141a), c.translated(rc, 0.0f), true));
        g.fillEllipse(juce::Rectangle<float>(rc * 2.0f, rc * 2.0f).withCentre(c));
        for (int i = 1; i <= 5; ++i)
        {
            g.setColour(juce::Colours::white.withAlpha(0.03f + 0.01f * static_cast<float>(i)));
            const auto rr = rc * (0.42f + 0.11f * static_cast<float>(i));
            g.drawEllipse(juce::Rectangle<float>(rr * 2.0f, rr * 2.0f).withCentre(c), 1.0f);
        }
        // dust cap
        const auto rd = R * 0.3f * (1.0f + 0.12f * e);
        g.setGradientFill(juce::ColourGradient(palette::magma.withAlpha(0.35f + 0.6f * glow).interpolatedWith(juce::Colour(0xff3a3036), 0.3f),
                                               c.translated(-rd * 0.35f, -rd * 0.4f),
                                               juce::Colour(0xff120f12), c.translated(rd * 0.6f, rd * 0.7f), true));
        g.fillEllipse(juce::Rectangle<float>(rd * 2.0f, rd * 2.0f).withCentre(c));
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillEllipse(juce::Rectangle<float>(rd * 0.5f, rd * 0.3f).withCentre(c.translated(-rd * 0.3f, -rd * 0.45f)));

        // note readout
        auto read = b.removeFromBottom(70.0f).reduced(16.0f, 6.0f);
        const auto playing = hub.frame.frequencyHz > 0.0f && hub.frame.note >= 0.0f;
        if (playing)
        {
            const auto n = hub.frame.note;
            const auto cents = juce::roundToInt((n - std::round(n)) * 100.0f);
            g.setColour(palette::text);
            g.setFont(font(34.0f, Weight::bold));
            g.drawText(noteName(n), read.removeFromLeft(read.getWidth() * 0.45f), juce::Justification::centredRight);
            read.removeFromLeft(12.0f);
            g.setFont(font(15.0f, Weight::semibold));
            g.setColour(palette::muted);
            g.drawText(juce::String(hub.frame.frequencyHz, 1) + " Hz", read.removeFromTop(read.getHeight() * 0.5f), juce::Justification::bottomLeft);
            g.setColour(std::abs(cents) <= 5 ? palette::glass : palette::ember);
            g.drawText((cents >= 0 ? "+" : "") + juce::String(cents) + juce::String(juce::CharPointer_UTF8(" cents")), read, juce::Justification::topLeft);
        }
        else
        {
            g.setColour(palette::dim);
            g.setFont(font(15.0f, Weight::semibold));
            g.drawText("Play a note", read, juce::Justification::centred);
        }
    }

private:
    Hub& hub;
    juce::AudioProcessorValueTreeState& params;
};

//==============================================================================
// Four seconds of the output: level as a filled shape, pitch as a line, so punch, tail and glides show.
class TailView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TailView(Hub& h) : hub(h)
    {
        setOpaque(true);
        setTooltip("The last four seconds: level (filled) and pitch (line). Glides show as slopes.\n"
                   "Teal ticks mark kicks that Kick Lock caught; shaded stretches are where it ducked the 808.\n"
                   "LOUD is short-term loudness (3 s, LUFS); PEAK is the highest sample.");
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(palette::background);
        const auto b = getLocalBounds().toFloat();
        g.setColour(palette::panel);
        g.fillRoundedRectangle(b, 12.0f);
        g.setColour(palette::edge);
        g.drawRoundedRectangle(b.reduced(0.5f), 12.0f, 1.0f);

        g.setFont(font(15.0f, Weight::bold));
        g.setColour(palette::text);
        g.drawText("TAIL", juce::Rectangle<float>(16.0f, 10.0f, 60.0f, 22.0f), juce::Justification::centredLeft);
        g.setFont(font(13.0f, Weight::bold));
        auto legendX = 80.0f;
        for (const auto& [colour, label] : { std::pair<juce::Colour, const char*> { palette::magma, "LEVEL" }, { palette::text, "PITCH" } })
        {
            g.setColour(colour);
            g.fillRoundedRectangle(legendX, 19.0f, 14.0f, 4.0f, 2.0f);
            g.setColour(palette::muted);
            g.drawText(label, juce::Rectangle<float>(legendX + 20.0f, 10.0f, 60.0f, 22.0f), juce::Justification::centredLeft);
            legendX += 72.0f;
        }
        // meters: peak, short-term loudness, Glue gain reduction
        const auto peakDb = juce::Decibels::gainToDecibels(hub.peakHold, -60.0f);
        auto meters = juce::Rectangle<float>(b.getWidth() - 16.0f - 334.0f, 10.0f, 334.0f, 22.0f);
        g.setColour(palette::muted);
        g.drawText("GLUE -" + juce::String(hub.grShown, 1) + " dB", meters.removeFromRight(96.0f), juce::Justification::centredRight);
        g.setColour(hub.loudness > -70.0f ? palette::text : palette::muted);
        g.drawText("LOUD " + (hub.loudness > -70.0f ? juce::String(hub.loudness, 1) : juce::String("--")) + " LUFS", meters.removeFromRight(128.0f), juce::Justification::centredRight);
        g.setColour(peakDb > -0.5f ? palette::magma : palette::muted);
        g.drawText("PEAK " + juce::String(peakDb, 1) + " dB", meters.removeFromRight(110.0f), juce::Justification::centredRight);

        const auto plot = b.reduced(16.0f, 0.0f).withTrimmedTop(40.0f).withTrimmedBottom(16.0f).withTrimmedLeft(36.0f);
        g.setColour(palette::plot);
        g.fillRoundedRectangle(plot, 4.0f);
        const auto n = Hub::historySize;
        const auto xAt = [&](int i) { return plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(n - 1); };
        const auto at = [&](const std::array<float, Hub::historySize>& a, int i) { return a[static_cast<std::size_t>((hub.head + 1 + i) % n)]; };

        // pitch range from what is on screen (at least an octave)
        float lo = 1000.0f, hi = -1000.0f;
        for (int i = 0; i < n; ++i)
            if (const auto v = at(hub.notes, i); v >= 0.0f) { lo = juce::jmin(lo, v); hi = juce::jmax(hi, v); }
        if (hi < lo) { lo = 24.0f; hi = 48.0f; }
        const auto mid = 0.5f * (lo + hi), span = juce::jmax(12.0f, hi - lo + 4.0f);
        lo = mid - span * 0.5f; hi = mid + span * 0.5f;
        const auto yNote = [&](float v) { return plot.getBottom() - (v - lo) / (hi - lo) * plot.getHeight(); };
        g.setFont(font(11.5f, Weight::semibold));
        for (int note = static_cast<int>(std::ceil(lo)); note <= static_cast<int>(hi); ++note)
        {
            if (note % 12 != 0 && note % 12 != 7 && span > 14.0f) continue;
            const auto y = yNote(static_cast<float>(note));
            g.setColour(palette::edge.withAlpha(0.6f));
            g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
            g.setColour(palette::muted);
            g.drawText(noteName(static_cast<float>(note)), juce::Rectangle<float>(plot.getX() - 36.0f, y - 7.0f, 32.0f, 14.0f), juce::Justification::centredRight);
        }

        // level, mirrored around the middle
        juce::Path level;
        const auto mid0 = plot.getCentreY(), half = plot.getHeight() * 0.48f;
        level.startNewSubPath(xAt(0), mid0);
        for (int i = 0; i < n; ++i) level.lineTo(xAt(i), mid0 - half * juce::jmin(1.0f, at(hub.amp, i)));
        for (int i = n - 1; i >= 0; --i) level.lineTo(xAt(i), mid0 + half * juce::jmin(1.0f, at(hub.amp, i)));
        level.closeSubPath();
        g.setGradientFill(juce::ColourGradient(palette::magma.withAlpha(0.55f), 0.0f, plot.getY(), palette::magma.withAlpha(0.12f), 0.0f, mid0, false));
        g.fillPath(level);

        // Kick Lock: ducked stretches shaded teal, a tick at every locked kick
        for (int i = 0; i < n; ++i)
        {
            const auto d = at(hub.duck, i);
            if (d > 0.02f)
            {
                g.setColour(palette::glass.withAlpha(0.18f * d));
                g.fillRect(juce::Rectangle<float>(xAt(i), plot.getY(), plot.getWidth() / static_cast<float>(n - 1) + 0.5f, plot.getHeight()));
            }
            if (hub.kicks[static_cast<std::size_t>((hub.head + 1 + i) % n)])
            {
                g.setColour(palette::glass);
                g.fillRect(juce::Rectangle<float>(xAt(i) - 1.0f, plot.getY(), 2.0f, 8.0f));
            }
        }

        // pitch line, only while a note sounds
        juce::Path pitch;
        bool drawing = false;
        for (int i = 0; i < n; ++i)
        {
            const auto v = at(hub.notes, i);
            if (v < 0.0f) { drawing = false; continue; }
            const auto p = juce::Point<float>(xAt(i), yNote(v));
            if (!drawing) { pitch.startNewSubPath(p); drawing = true; }
            else pitch.lineTo(p);
        }
        g.setColour(palette::text.withAlpha(0.25f));
        g.strokePath(pitch, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(palette::text);
        g.strokePath(pitch, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(palette::dim);
        g.setFont(font(11.5f));
        g.drawText("4 s ago", juce::Rectangle<float>(plot.getX() + 4.0f, plot.getBottom() - 16.0f, 60.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText("now", juce::Rectangle<float>(plot.getRight() - 40.0f, plot.getBottom() - 16.0f, 36.0f, 14.0f), juce::Justification::centredRight);
    }

private:
    Hub& hub;
};

//==============================================================================
inline void paintPill(juce::Graphics& g, juce::Rectangle<float> area, bool on, bool over, juce::Colour colour, const juce::String& label)
{
    area = area.reduced(0.5f);
    const auto radius = area.getHeight() * 0.5f;
    g.setColour(on ? colour.withAlpha(0.16f) : palette::plot);
    g.fillRoundedRectangle(area, radius);
    g.setColour(on ? colour : (over ? palette::dim : palette::edge));
    g.drawRoundedRectangle(area, radius, 1.0f);
    g.setColour(on ? colour : palette::muted);
    g.fillEllipse(area.getX() + 11.0f, area.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour(on ? palette::text : palette::muted);
    g.setFont(font(14.0f, Weight::semibold));
    g.drawText(label, area.withTrimmedLeft(22.0f).withTrimmedRight(8.0f), juce::Justification::centred);
}

class PillToggle final : public juce::Button
{
public:
    PillToggle(const juce::String& offText, juce::Colour onColour, juce::RangedAudioParameter& parameter, const juce::String& onText = {})
        : juce::Button(offText), offLabel(offText), onLabel(onText.isEmpty() ? offText : onText), colour(onColour),
          attachment(parameter, [this](float v) { setToggleState(v > 0.5f, juce::dontSendNotification); repaint(); })
    {
        onClick = [this] { attachment.setValueAsCompleteGesture(getToggleState() ? 0.0f : 1.0f); };
        attachment.sendInitialUpdate();
    }
    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        paintPill(g, getLocalBounds().toFloat(), getToggleState(), over, colour, getToggleState() ? onLabel : offLabel);
    }
private:
    juce::String offLabel, onLabel;
    juce::Colour colour;
    juce::ParameterAttachment attachment;
};

// Segmented switch for a choice parameter (Drive mode, Glide mode).
class Segments final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segments(juce::RangedAudioParameter& parameter, juce::StringArray labels, juce::Colour accentColour)
        : names(std::move(labels)), accent(accentColour),
          attachment(parameter, [this](float v) { selected = juce::roundToInt(v); repaint(); })
    {
        attachment.sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(palette::plot);
        g.fillRoundedRectangle(area, 7.0f);
        g.setColour(palette::edge);
        g.drawRoundedRectangle(area, 7.0f, 1.0f);
        for (int i = 0; i < names.size(); ++i)
        {
            const auto cell = cellAt(i);
            if (i == selected)
            {
                g.setColour(accent.withAlpha(0.18f));
                g.fillRoundedRectangle(cell.reduced(3.0f), 5.0f);
                g.setColour(accent);
                g.drawRoundedRectangle(cell.reduced(3.0f), 5.0f, 1.0f);
            }
            g.setColour(i == selected ? palette::text : palette::muted);
            g.setFont(font(13.5f, Weight::bold));
            g.drawText(names[i], cell, juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < names.size(); ++i)
            if (cellAt(i).contains(e.position) && i != selected)
                attachment.setValueAsCompleteGesture(static_cast<float>(i));
    }
private:
    juce::Rectangle<float> cellAt(int i) const
    {
        const auto w = static_cast<float>(getWidth()) / static_cast<float>(names.size());
        return { w * static_cast<float>(i), 0.0f, w, static_cast<float>(getHeight()) };
    }
    juce::StringArray names;
    juce::Colour accent;
    juce::ParameterAttachment attachment;
    int selected = 0;
};

//==============================================================================
// Phone Check: click to listen through a device curve; the arrow picks the device.
class PhoneButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    PhoneButton(juce::RangedAudioParameter& phone, juce::RangedAudioParameter& model)
        : onAttachment(phone, [this](float v) { on = v > 0.5f; repaint(); }),
          modelAttachment(model, [this](float v) { device = juce::roundToInt(v); repaint(); })
    {
        onAttachment.sendInitialUpdate();
        modelAttachment.sendInitialUpdate();
        setTooltip("Phone Check: hear the 808 through an approximate device curve (Phone, Laptop, Earbuds or Car).\n"
                   "Click to switch on and off; the arrow picks the device. Monitoring only: never saved in presets.");
    }
    void paint(juce::Graphics& g) override
    {
        static const char* names[] = { "PHONE", "LAPTOP", "EARBUDS", "CAR" };
        const auto area = getLocalBounds().toFloat();
        paintPill(g, area.withTrimmedRight(0.0f), on, isMouseOver(true), palette::glass,
                  on ? juce::String("HEARING: ") + names[juce::jlimit(0, 3, device)] : juce::String("PHONE CHECK"));
        const auto arrow = arrowArea();
        g.setColour(on ? palette::glass.withAlpha(0.5f) : palette::edge);
        g.drawVerticalLine(juce::roundToInt(arrow.getX()), arrow.getY() + 6.0f, arrow.getBottom() - 6.0f);
        juce::Path tri;
        const auto c = arrow.getCentre();
        tri.addTriangle(c.x - 4.0f, c.y - 2.0f, c.x + 4.0f, c.y - 2.0f, c.x, c.y + 3.0f);
        g.setColour(on ? palette::text : palette::muted);
        g.fillPath(tri);
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (arrowArea().contains(e.position))
        {
            juce::PopupMenu menu;
            const char* names[] = { "Phone speaker", "Laptop speakers", "Earbuds", "Car" };
            for (int i = 0; i < 4; ++i)
                menu.addItem(i + 1, names[i], true, device == i);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               [safe = juce::Component::SafePointer<PhoneButton>(this)](int result)
                               {
                                   if (safe == nullptr || result <= 0) return;
                                   safe->modelAttachment.setValueAsCompleteGesture(static_cast<float>(result - 1));
                                   if (!safe->on) safe->onAttachment.setValueAsCompleteGesture(1.0f);
                               });
            return;
        }
        onAttachment.setValueAsCompleteGesture(on ? 0.0f : 1.0f);
    }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
private:
    juce::Rectangle<float> arrowArea() const { return getLocalBounds().toFloat().removeFromRight(26.0f); }
    juce::ParameterAttachment onAttachment, modelAttachment;
    bool on = false;
    int device = 0;
};

//==============================================================================
// Kick Lock on/off, with its routing status in the label and a flash on every locked kick.
class KickButton final : public juce::Button
{
public:
    KickButton(Epsilon808AudioProcessor& p, Hub& h)
        : juce::Button("KICK LOCK"), processor(p), hub(h),
          attachment(*p.params.getParameter("kickLock"), [this](float v) { setToggleState(v > 0.5f, juce::dontSendNotification); repaint(); })
    {
        onClick = [this] { attachment.setValueAsCompleteGesture(getToggleState() ? 0.0f : 1.0f); };
        attachment.sendInitialUpdate();
    }
    bool hasInput() const { return processor.isKickInputRouted() && processor.getKickInputLevel() > 0.001f; }
    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        const auto on = getToggleState();
        const auto waiting = on && !hasInput();
        paintPill(g, getLocalBounds().toFloat(), on, over, waiting ? palette::ember : palette::glass,
                  !on ? "KICK LOCK" : waiting ? "KICK LOCK: NO KICK" : "KICK LOCK ON");
        if (on && hub.kickFlash > 0.05f)
        {
            g.setColour(palette::glass.withAlpha(0.6f * hub.kickFlash));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), getHeight() * 0.5f, 2.5f);
        }
    }
private:
    Epsilon808AudioProcessor& processor;
    Hub& hub;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Preset browser: categories, search, favourites, user presets (save / delete), click to audition.
class PresetBrowser final : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit PresetBrowser(Epsilon808AudioProcessor& p) : processor(p)
    {
        setWantsKeyboardFocus(true);
        title.setText("PRESETS", juce::dontSendNotification);
        title.setFont(font(18.0f, Weight::bold));
        title.setColour(juce::Label::textColourId, palette::text);
        addAndMakeVisible(title);

        search.setTextToShowWhenEmpty("Search presets", palette::dim);
        search.setFont(font(16.0f));
        search.onTextChange = [this] { refresh(); };
        search.setColour(juce::TextEditor::outlineColourId, palette::edge);
        addAndMakeVisible(search);

        close.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible(close);

        juce::StringArray cats { "All", "Favourites" };
        cats.addArray(Epsilon808AudioProcessor::categories());
        cats.add("User");
        for (const auto& c : cats)
        {
            auto* b = categoryButtons.add(new CategoryButton(c));
            b->onClick = [this, c] { category = c; refresh(); };
            addAndMakeVisible(b);
        }

        list.setModel(this);
        list.setRowHeight(40);
        list.setColour(juce::ListBox::backgroundColourId, palette::plot);
        list.setColour(juce::ListBox::outlineColourId, palette::edge);
        list.setOutlineThickness(1);
        addAndMakeVisible(list);

        audition.setClickingTogglesState(true);
        audition.setToggleState(static_cast<bool>(processor.params.state.getProperty("audition", true)), juce::dontSendNotification);
        audition.onClick = [this] { processor.params.state.setProperty("audition", audition.getToggleState(), nullptr); repaint(); };
        addAndMakeVisible(audition);

        nameField.setTextToShowWhenEmpty("Name your preset", palette::dim);
        nameField.setFont(font(15.0f));
        nameField.setColour(juce::TextEditor::outlineColourId, palette::edge);
        nameField.onReturnKey = [this] { save(); };
        addAndMakeVisible(nameField);
        for (const auto& c : Epsilon808AudioProcessor::categories())
            saveCategory.addItem(c, saveCategory.getNumItems() + 1);
        saveCategory.addItem("User", saveCategory.getNumItems() + 1);
        saveCategory.setSelectedId(saveCategory.getNumItems(), juce::dontSendNotification);
        addAndMakeVisible(saveCategory);
        saveButton.onClick = [this] { save(); };
        addAndMakeVisible(saveButton);
        deleteButton.onClick = [this] { remove(); };
        addAndMakeVisible(deleteButton);
        folderButton.onClick = [] { auto f = Epsilon808AudioProcessor::userPresetFolder(); f.createDirectory(); f.revealToUser(); };
        addAndMakeVisible(folderButton);
        status.setFont(font(13.5f));
        status.setColour(juce::Label::textColourId, palette::muted);
        addAndMakeVisible(status);
        refresh();
    }
    ~PresetBrowser() override { stopTimer(); releaseAudition(); }

    std::function<void()> onClose, onLoaded;

    void refresh()
    {
        all = processor.listPresets();
        shown.clear();
        const auto query = search.getText().trim();
        for (const auto& e : all)
        {
            if (category == "Favourites" && !processor.isFavourite(e)) continue;
            if (category == "User" && !e.isUser()) continue;
            if (category != "All" && category != "Favourites" && category != "User" && e.category != category) continue;
            if (query.isNotEmpty() && !e.name.containsIgnoreCase(query) && !e.category.containsIgnoreCase(query)) continue;
            shown.push_back(e);
        }
        for (auto* b : categoryButtons)
            b->setToggleState(b->getButtonText() == category, juce::dontSendNotification);
        list.updateContent();
        int current = -1;
        for (std::size_t i = 0; i < shown.size(); ++i)
            if (shown[i].name == processor.getCurrentPresetName()) current = static_cast<int>(i);
        if (current >= 0) list.selectRow(current, true, true);
        else list.deselectAllRows();
        updateButtons();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(palette::panel);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
        g.setColour(palette::edge.brighter(0.2f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 12.0f, 1.0f);
        g.setColour(palette::muted);
        g.setFont(font(12.5f, Weight::bold));
        g.drawText("SAVE CURRENT SOUND", saveLabel, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(16, 12);
        auto top = b.removeFromTop(32);
        title.setBounds(top.removeFromLeft(100));
        close.setBounds(top.removeFromRight(32));
        top.removeFromRight(10);
        search.setBounds(top.removeFromLeft(300));
        b.removeFromTop(8);
        // category chips in one row
        auto chips = b.removeFromTop(26);
        int x = chips.getX();
        for (auto* c : categoryButtons)
        {
            const auto w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font(14.0f, Weight::semibold), c->getButtonText())) + 22;
            c->setBounds(x, chips.getY(), w, chips.getHeight());
            x += w + 4;
        }
        b.removeFromTop(8);
        auto bottom = b.removeFromBottom(60);
        b.removeFromBottom(8);
        list.setBounds(b);

        saveLabel = bottom.removeFromTop(18).toFloat();
        auto row = bottom.removeFromTop(32);
        nameField.setBounds(row.removeFromLeft(240));
        row.removeFromLeft(8);
        saveCategory.setBounds(row.removeFromLeft(130));
        row.removeFromLeft(8);
        saveButton.setBounds(row.removeFromLeft(76));
        row.removeFromLeft(16);
        deleteButton.setBounds(row.removeFromLeft(76));
        row.removeFromLeft(8);
        folderButton.setBounds(row.removeFromLeft(104));
        audition.setBounds(row.removeFromRight(140));
        row.removeFromRight(8);
        status.setBounds(row.withTrimmedLeft(10));
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        return false;
    }

private:
    class CategoryButton final : public juce::Button
    {
    public:
        explicit CategoryButton(const juce::String& t) : juce::Button(t) {}
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            const auto on = getToggleState();
            const auto a = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(on ? palette::magma.withAlpha(0.16f) : palette::plot);
            g.fillRoundedRectangle(a, a.getHeight() * 0.5f);
            g.setColour(on ? palette::magma : over ? palette::dim : palette::edge);
            g.drawRoundedRectangle(a, a.getHeight() * 0.5f, 1.0f);
            g.setColour(on ? palette::text : palette::muted);
            g.setFont(font(14.0f, Weight::semibold));
            g.drawText(getButtonText(), a, juce::Justification::centred);
        }
    };

    class AuditionButton final : public juce::Button
    {
    public:
        AuditionButton() : juce::Button("Audition") {}
        void paintButton(juce::Graphics& g, bool over, bool) override
        {
            paintPill(g, getLocalBounds().toFloat(), getToggleState(), over, palette::magma, getToggleState() ? "AUDITION ON" : "AUDITION OFF");
        }
    };

    int getNumRows() override { return static_cast<int>(shown.size()); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (!juce::isPositiveAndBelow(row, static_cast<int>(shown.size()))) return;
        const auto& e = shown[static_cast<std::size_t>(row)];
        if (selected)
        {
            g.setColour(palette::magma.withAlpha(0.14f));
            g.fillRect(0, 0, width, height);
        }
        // favourite star
        const auto fav = processor.isFavourite(e);
        juce::Path star;
        star.addStar({ 18.0f, height * 0.5f }, 5, 4.0f, 8.5f, 0.0f);
        g.setColour(fav ? palette::ember : palette::dim);
        if (fav) g.fillPath(star); else g.strokePath(star, juce::PathStrokeType(1.2f));
        g.setColour(selected ? palette::text : palette::text.withAlpha(0.88f));
        g.setFont(font(16.0f, Weight::semibold));
        g.drawText(e.name, 40, 3, width - 220, height / 2, juce::Justification::bottomLeft);
        g.setColour(palette::muted);
        g.setFont(font(13.0f));
        g.drawText(e.note.isNotEmpty() ? e.note : juce::String(e.isUser() ? "Your preset" : ""), 40, height / 2 + 1, width - 220, height / 2 - 4, juce::Justification::topLeft);
        g.setColour(e.isUser() ? palette::glass : palette::muted);
        g.setFont(font(13.0f, Weight::semibold));
        g.drawText((e.isUser() ? juce::String(juce::CharPointer_UTF8("USER \xc2\xb7 ")) : juce::String()) + e.category.toUpperCase(), width - 170, 0, 160, height, juce::Justification::centredRight);
        g.setColour(palette::edge.withAlpha(0.6f));
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (!juce::isPositiveAndBelow(row, static_cast<int>(shown.size()))) return;
        const auto entry = shown[static_cast<std::size_t>(row)];
        if (e.x < 36)
        {
            processor.toggleFavourite(entry);
            if (category == "Favourites") refresh(); else list.repaintRow(row);
            return;
        }
        processor.loadPreset(entry);
        nameField.setText(entry.isUser() ? entry.name : juce::String(), false);
        if (audition.getToggleState())
            playAudition();
        updateButtons();
        if (onLoaded) onLoaded();
    }

    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { if (onClose) onClose(); }
    void selectedRowsChanged(int) override { updateButtons(); }

    void updateButtons()
    {
        const auto row = list.getSelectedRow();
        deleteButton.setEnabled(juce::isPositiveAndBelow(row, static_cast<int>(shown.size())) && shown[static_cast<std::size_t>(row)].isUser());
    }

    void save()
    {
        const auto name = nameField.getText().trim();
        if (name.isEmpty()) { status.setText("Type a name first.", juce::dontSendNotification); return; }
        if (processor.saveUserPreset(name, saveCategory.getText()))
        {
            status.setText("Saved \"" + name + "\" to your preset folder.", juce::dontSendNotification);
            category = "User";
            refresh();
            if (onLoaded) onLoaded();
        }
        else
            status.setText("Could not write to " + Epsilon808AudioProcessor::userPresetFolder().getFullPathName(), juce::dontSendNotification);
    }

    void remove()
    {
        const auto row = list.getSelectedRow();
        if (!juce::isPositiveAndBelow(row, static_cast<int>(shown.size()))) return;
        const auto entry = shown[static_cast<std::size_t>(row)];
        if (!entry.isUser()) return;
        if (!confirmDelete || confirmName != entry.name)
        {
            confirmDelete = true;
            confirmName = entry.name;
            status.setText("Click Delete again to remove \"" + entry.name + "\".", juce::dontSendNotification);
            return;
        }
        confirmDelete = false;
        if (processor.deleteUserPreset(entry))
            status.setText("Deleted \"" + entry.name + "\".", juce::dontSendNotification);
        refresh();
    }

    void playAudition()
    {
        releaseAudition();
        processor.keyboardState.noteOn(1, auditionNote, 0.9f);
        auditionPlaying = true;
        startTimer(500);
    }
    void releaseAudition()
    {
        if (auditionPlaying)
            processor.keyboardState.noteOff(1, auditionNote, 0.0f);
        auditionPlaying = false;
    }
    void timerCallback() override { stopTimer(); releaseAudition(); }

    Epsilon808AudioProcessor& processor;
    juce::Label title, status;
    juce::TextEditor search, nameField;
    juce::TextButton close { juce::CharPointer_UTF8("\xe2\x9c\x95") };
    juce::OwnedArray<CategoryButton> categoryButtons;
    juce::ListBox list;
    AuditionButton audition;
    juce::ComboBox saveCategory;
    juce::TextButton saveButton { "Save" }, deleteButton { "Delete" }, folderButton { "Open folder" };
    juce::Rectangle<float> saveLabel;
    std::vector<PresetEntry> all, shown;
    juce::String category { "All" };
    bool confirmDelete = false, auditionPlaying = false;
    juce::String confirmName;
    static constexpr int auditionNote = 31;   // G1
};
}
