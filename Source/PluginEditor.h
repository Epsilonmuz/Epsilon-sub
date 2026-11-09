#pragma once

#include "PluginProcessor.h"

namespace boomui { class BoomLook; }

// Laid out at a fixed design size and scaled as one piece, so it reads the same at every size.
class Epsilon808AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Epsilon808AudioProcessorEditor(Epsilon808AudioProcessor&);
    ~Epsilon808AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 980;
    static constexpr int designHeight = 760;

private:
    class Panel;
    void timerCallback() override;

    std::unique_ptr<boomui::BoomLook> look;
    std::unique_ptr<Panel> panel;
    bool focusGrabbed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Epsilon808AudioProcessorEditor)
};
