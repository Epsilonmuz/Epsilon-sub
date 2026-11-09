#pragma once

// Epsilon 808 v1.1 Hybrid page parts: view switch, layer selector, layer waveform, envelope view.
// Header-only; included by PluginEditor.cpp after BoomUI.h.

#include "BoomUI.h"

namespace boomui
{
namespace palette
{
inline const juce::Colour layerA { 0xffb79cff };   // Layer A: the sample's character (violet)
inline const juce::Colour layerB { 0xff7fb2ff };   // Layer B: texture (blue)
}

// SIMPLE / ADVANCED / HYBRID
class ViewSwitch final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void(int)> onSelect;
    void setView(int v) { if (v != view) { view = v; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(palette::plot);
        g.fillRoundedRectangle(area, 7.0f);
        g.setColour(palette::edge);
        g.drawRoundedRectangle(area, 7.0f, 1.0f);
        static const char* names[] = { "SIMPLE", "ADVANCED", "HYBRID" };
        for (int i = 0; i < 3; ++i)
        {
            const auto cell = cellAt(i);
            const auto accent = i == 2 ? palette::layerA : palette::text;
            if (i == view)
            {
                g.setColour(accent.withAlpha(0.16f));
                g.fillRoundedRectangle(cell.reduced(3.0f), 5.0f);
                g.setColour(accent);
                g.drawRoundedRectangle(cell.reduced(3.0f), 5.0f, 1.0f);
            }
            g.setColour(i == view ? palette::text : palette::muted);
            g.setFont(font(13.0f, Weight::bold));
            g.drawText(names[i], cell, juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
            if (cellAt(i).contains(e.position) && onSelect)
                onSelect(i);
    }
private:
    juce::Rectangle<float> cellAt(int i) const
    {
        const auto widths = std::array<float, 3> { 0.3f, 0.38f, 0.32f };
        float x = 0.0f;
        for (int k = 0; k < i; ++k) x += widths[static_cast<std::size_t>(k)];
        return { x * static_cast<float>(getWidth()), 0.0f, widths[static_cast<std::size_t>(i)] * static_cast<float>(getWidth()), static_cast<float>(getHeight()) };
    }
    int view = 0;
};

// The sound a layer plays: click for a grouped menu, arrows step through the library.
class LayerSelector final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LayerSelector(Epsilon808AudioProcessor& p, int slotIndex, juce::Colour accentColour, const char* paramId = nullptr)
        : processor(p), slot(slotIndex), accent(accentColour),
          attachment(*p.params.getParameter(paramId != nullptr ? paramId : (slotIndex == 0 ? "layerA" : "layerB")),
                     [this](float v) { index = juce::roundToInt(v); repaint(); })
    {
        attachment.sendInitialUpdate();
    }
    std::function<void()> onLoadRequest;

    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(0.5f);
        const auto on = index > 0;
        g.setColour(on ? accent.withAlpha(0.12f) : palette::plot);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(on ? accent.withAlpha(0.8f) : (hover ? palette::dim : palette::edge));
        g.drawRoundedRectangle(area, 6.0f, 1.0f);
        g.setColour(on ? palette::text : palette::muted);
        g.setFont(font(15.5f, Weight::semibold));
        g.drawFittedText(displayName(), getLocalBounds().reduced(28, 0), juce::Justification::centred, 1);
        g.setColour(palette::muted);
        g.setFont(font(15.0f, Weight::bold));
        g.drawText(juce::CharPointer_UTF8("\xe2\x97\x80"), arrow(false), juce::Justification::centred);
        g.drawText(juce::CharPointer_UTF8("\xe2\x96\xb6"), arrow(true), juce::Justification::centred);
    }
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto count = Epsilon808AudioProcessor::userLayerIndex() + 1;
        if (arrow(false).contains(e.position)) { set((index - 1 + count) % count); return; }
        if (arrow(true).contains(e.position)) { set((index + 1) % count); return; }
        juce::PopupMenu menu;
        menu.addItem(1, "Off", true, index == 0);
        const auto& bank = LayerBank::get();
        juce::String group;
        for (int i = 0; i < bank.size(); ++i)
        {
            const auto* layer = bank.at(i);
            if (layer == nullptr) continue;
            if (layer->group != group)
            {
                group = layer->group;
                menu.addSectionHeader(group.toUpperCase());
            }
            menu.addItem(i + 2, layer->name, layer->sample.length > 0, index == i + 1);
        }
        menu.addSectionHeader("YOURS");
        const auto user = processor.getUserSampleName(slot);
        if (user.isNotEmpty())
            menu.addItem(Epsilon808AudioProcessor::userLayerIndex() + 1, user, true, index == Epsilon808AudioProcessor::userLayerIndex());
        menu.addItem(10000, juce::CharPointer_UTF8("Load a sample\xe2\x80\xa6"));
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(getWidth()),
                           [safe = juce::Component::SafePointer<LayerSelector>(this)](int result)
                           {
                               if (safe == nullptr || result == 0) return;
                               if (result == 10000) { if (safe->onLoadRequest) safe->onLoadRequest(); return; }
                               safe->set(result - 1);
                           });
    }
private:
    void set(int i) { attachment.setValueAsCompleteGesture(static_cast<float>(i)); }
    juce::String displayName() const
    {
        if (index <= 0) return emptyText;
        if (index == Epsilon808AudioProcessor::userLayerIndex())
        {
            const auto user = processor.getUserSampleName(slot);
            return user.isNotEmpty() ? user : juce::String(juce::CharPointer_UTF8("Load a sample\xe2\x80\xa6"));
        }
        const auto* layer = LayerBank::get().at(index - 1);
        return layer != nullptr ? layer->name : juce::String();
    }
    juce::Rectangle<float> arrow(bool right) const
    {
        const auto h = static_cast<float>(getHeight());
        return right ? juce::Rectangle<float>(static_cast<float>(getWidth()) - 26.0f, 0.0f, 26.0f, h) : juce::Rectangle<float>(0.0f, 0.0f, 26.0f, h);
    }
    Epsilon808AudioProcessor& processor;
    int slot;
    juce::Colour accent;
    juce::ParameterAttachment attachment;
    int index = 0;
    bool hover = false;
public:
    juce::String emptyText { "No layer" };
};

// Reads the controls the views draw, and says when any of them changed.
struct ParamReader
{
    explicit ParamReader(juce::AudioProcessorValueTreeState& s) : state(s) {}
    float operator()(const char* id) const { return state.getRawParameterValue(id)->load(); }
    juce::AudioProcessorValueTreeState& state;
};

// One layer's sample, with its envelope drawn over it (the part of the sample you hear is bright).
class LayerWave final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LayerWave(Epsilon808AudioProcessor& p, int slotIndex, juce::Colour accentColour)
        : processor(p), read(p.params), slot(slotIndex), accent(accentColour)
    {
        startTimerHz(15);
    }
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(palette::plot);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(palette::edge);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);
        const auto* layer = shown;
        const auto inner = area.reduced(6.0f, 5.0f);
        if (layer == nullptr || layer->sample.length == 0)
        {
            g.setColour(palette::dim);
            g.setFont(font(13.0f, Weight::semibold));
            g.drawText(slot == 0 ? "Pick a layer to blend with the engine" : "Optional second layer: a texture or tail", inner, juce::Justification::centred);
            return;
        }
        const auto X = slot == 0 ? "layerA" : "layerB";
        const auto seconds = static_cast<float>(layer->sample.length / layer->sample.rate);
        const auto shownSeconds = juce::jlimit(1.0f, 4.0f, seconds);
        const auto attack = read(juce::String(X + juce::String("Attack")).toRawUTF8()) * 0.001f;
        const auto decay = read(juce::String(X + juce::String("Decay")).toRawUTF8());
        const auto hold = slot == 0 ? read("layerAHold") * 0.001f : 0.0f;
        const auto sustain = slot == 0 ? read("layerASustain") * 0.01f : 0.0f;
        const auto start = slot == 0 ? read("layerAStart") * 0.001f : 0.0f;
        const auto envAt = [&](float t)
        {
            if (t < attack) return t / juce::jmax(0.0005f, attack);
            if (t < attack + hold || decay >= 9999.0f) return 1.0f;
            return sustain + (1.0f - sustain) * std::exp(-6.9078f * (t - attack - hold) / juce::jmax(0.01f, decay * 0.001f));
        };
        const auto columns = static_cast<int>(inner.getWidth());
        const auto mid = inner.getCentreY(), half = inner.getHeight() * 0.5f;
        juce::Path heard, envelope;
        for (int c = 0; c < columns; ++c)
        {
            const auto t0 = shownSeconds * static_cast<float>(c) / static_cast<float>(columns);
            const auto t1 = shownSeconds * static_cast<float>(c + 1) / static_cast<float>(columns);
            const auto f0 = static_cast<int>(t0 * layer->sample.rate), f1 = static_cast<int>(t1 * layer->sample.rate);
            float peak = 0.0f;
            for (int f = f0; f < f1 && f < layer->sample.length; f += 4)
                peak = juce::jmax(peak, std::abs(layer->mid[static_cast<std::size_t>(f)]));
            const auto x = inner.getX() + static_cast<float>(c);
            const auto tHeard = t0 - start;
            g.setColour((tHeard < 0.0f ? palette::dim : accent.withAlpha(0.28f)));
            g.drawVerticalLine(static_cast<int>(x), mid - peak * half, mid + peak * half);
            const auto e = tHeard < 0.0f ? 0.0f : envAt(tHeard);
            const auto h = peak * e * half;
            if (h > 0.25f)
            {
                g.setColour(accent.withAlpha(0.85f));
                g.drawVerticalLine(static_cast<int>(x), mid - h, mid + h);
            }
            const auto ey = inner.getBottom() - e * inner.getHeight();
            if (c == 0) envelope.startNewSubPath(x, ey); else envelope.lineTo(x, ey);
        }
        g.setColour(palette::text.withAlpha(0.7f));
        g.strokePath(envelope, juce::PathStrokeType(1.2f));
        g.setColour(palette::muted);
        g.setFont(font(11.5f, Weight::semibold));
        g.drawText(layer->group.toUpperCase() + juce::String(juce::CharPointer_UTF8("  \xc2\xb7  ")) + juce::String(seconds, 1) + " s", inner.reduced(2.0f, 0.0f), juce::Justification::topRight);
    }
private:
    void timerCallback() override
    {
        // repaint only when the sample or a control the drawing uses has changed
        const auto* layer = processor.layerFor(slot);
        const auto X = slot == 0 ? juce::String("layerA") : juce::String("layerB");
        const auto key = juce::String(read((X + "Attack").toRawUTF8())) + "|" + juce::String(read((X + "Decay").toRawUTF8()))
                       + (slot == 0 ? "|" + juce::String(read("layerAHold")) + "|" + juce::String(read("layerASustain")) + "|" + juce::String(read("layerAStart")) : juce::String());
        if (layer != shown || key != lastKey)
        {
            shown = layer;
            lastKey = key;
            repaint();
        }
    }
    Epsilon808AudioProcessor& processor;
    ParamReader read;
    int slot;
    juce::Colour accent;
    const DecodedLayer* shown = nullptr;
    juce::String lastKey;
};

// Engine and both layers on one timeline: hold the key for 2.5 s, then let go.
class EnvelopeView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit EnvelopeView(Epsilon808AudioProcessor& p) : processor(p), read(p.params) { startTimerHz(15); }
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(palette::plot);
        g.fillRoundedRectangle(area, 6.0f);
        g.setColour(palette::edge);
        g.drawRoundedRectangle(area, 6.0f, 1.0f);
        const auto plot = area.reduced(10.0f, 8.0f).withTrimmedTop(12.0f).withTrimmedBottom(12.0f);
        constexpr float total = 4.0f, keyUp = 2.5f;
        const auto xAt = [&](float t) { return plot.getX() + plot.getWidth() * t / total; };
        const auto yAt = [&](float v) { return plot.getBottom() - plot.getHeight() * juce::jlimit(0.0f, 1.0f, v); };

        g.setColour(palette::edge);
        for (int s = 1; s < 4; ++s) g.drawVerticalLine(static_cast<int>(xAt(static_cast<float>(s))), plot.getY(), plot.getBottom());
        g.setColour(palette::dim);
        g.drawVerticalLine(static_cast<int>(xAt(keyUp)), plot.getY() - 4.0f, plot.getBottom());
        g.setFont(font(11.0f, Weight::semibold));
        g.drawText("KEY UP", juce::Rectangle<float>(xAt(keyUp) + 4.0f, plot.getY() - 14.0f, 60.0f, 12.0f), juce::Justification::centredLeft);
        for (int s = 0; s <= 4; ++s)
            g.drawText(juce::String(s) + " s", juce::Rectangle<float>(xAt(static_cast<float>(s)) - 14.0f, plot.getBottom() + 1.0f, 28.0f, 12.0f), juce::Justification::centred);

        // engine: attack, Length to -60 dB toward Sustain, Release
        const auto engineOn = read("engine") > -47.5f;
        const auto eA = read("attack") * 0.001f, eL = read("length") * 0.001f, eS = read("sustain") * 0.01f, eR = read("release") * 0.001f;
        const auto engineAt = [&](float t)
        {
            const auto held = [&](float u) { return u < eA ? u / juce::jmax(0.0005f, eA) : eS + (1.0f - eS) * std::exp(-6.9078f * (u - eA) / juce::jmax(0.02f, eL)); };
            return t < keyUp ? held(t) : held(keyUp) * std::exp(-6.9078f * (t - keyUp) / juce::jmax(0.003f, eR));
        };
        const auto layerOn = [&](int slot) { return juce::roundToInt(read(slot == 0 ? "layerA" : "layerB")) > 0; };
        const auto rel = read("layerARelease") * 0.001f;
        const auto layerAt = [&](int slot, float t)
        {
            const auto X = juce::String(slot == 0 ? "layerA" : "layerB");
            const auto a = read((X + "Attack").toRawUTF8()) * 0.001f, d = read((X + "Decay").toRawUTF8());
            const auto h = slot == 0 ? read("layerAHold") * 0.001f : 0.0f, s = slot == 0 ? read("layerASustain") * 0.01f : 0.0f;
            const auto held = [&](float u)
            {
                if (u < a) return u / juce::jmax(0.0005f, a);
                if (u < a + h || d >= 9999.0f) return 1.0f;
                return s + (1.0f - s) * std::exp(-6.9078f * (u - a - h) / juce::jmax(0.01f, d * 0.001f));
            };
            return t < keyUp ? held(t) : held(keyUp) * std::exp(-6.9078f * (t - keyUp) / juce::jmax(0.003f, rel));
        };
        const auto curve = [&](auto fn, juce::Colour colour, bool fill)
        {
            juce::Path p;
            const auto steps = static_cast<int>(plot.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = total * static_cast<float>(i) / static_cast<float>(steps);
                const auto v = fn(t);
                if (i == 0) p.startNewSubPath(xAt(t), yAt(v)); else p.lineTo(xAt(t), yAt(v));
            }
            if (fill)
            {
                auto area2 = p;
                area2.lineTo(plot.getRight(), plot.getBottom());
                area2.lineTo(plot.getX(), plot.getBottom());
                area2.closeSubPath();
                g.setColour(colour.withAlpha(0.12f));
                g.fillPath(area2);
            }
            g.setColour(colour);
            g.strokePath(p, juce::PathStrokeType(1.6f));
        };
        if (layerOn(1)) curve([&](float t) { return layerAt(1, t); }, palette::layerB, false);
        if (layerOn(0)) curve([&](float t) { return layerAt(0, t); }, palette::layerA, false);
        if (engineOn) curve(engineAt, palette::magma, true);

        // legend
        g.setFont(font(12.0f, Weight::bold));
        auto x = plot.getX();
        const auto legend = [&](const char* text, juce::Colour c, bool on)
        {
            g.setColour(on ? c : palette::dim);
            g.fillEllipse(x, area.getY() + 7.0f, 7.0f, 7.0f);
            g.drawText(text, juce::Rectangle<float>(x + 11.0f, area.getY() + 3.0f, 90.0f, 14.0f), juce::Justification::centredLeft);
            x += 12.0f + juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 16.0f;
        };
        legend("ENGINE", palette::magma, engineOn);
        legend("LAYER A", palette::layerA, layerOn(0));
        legend("LAYER B", palette::layerB, layerOn(1));
    }
private:
    void timerCallback() override
    {
        juce::String key;
        for (auto* id : { "engine", "attack", "length", "sustain", "release", "layerA", "layerB", "layerAAttack", "layerADecay", "layerAHold",
                          "layerASustain", "layerARelease", "layerBAttack", "layerBDecay" })
            key << read(id) << "|";
        if (key != lastKey) { lastKey = key; repaint(); }
    }
    Epsilon808AudioProcessor& processor;
    ParamReader read;
    juce::String lastKey;
};
}
