#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace silina
{

// Knobs styled as cassette reel hubs, printed-label buttons.
class CassetteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CassetteLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// The whole cassette, drawn at a fixed logical size and scaled by the editor.
class CassetteDeck : public juce::Component, private juce::Timer
{
public:
    static constexpr int logicalWidth = 1040;
    static constexpr int logicalHeight = 680;

    explicit CassetteDeck (SilinaAudioProcessor&);
    ~CassetteDeck() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Exposed for tests.
    juce::Slider* findKnob (const juce::String& paramId) noexcept;
    juce::TextButton& getSpeedButton (int index) noexcept { return speedButtons[(size_t) index]; }
    juce::ToggleButton& getAutoGainButton() noexcept { return autoGain; }
    float getReelAngle() const noexcept { return reelAngle; }
    void advanceAnimation (float seconds);

private:
    struct Knob
    {
        juce::String paramId;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void setSpeed (int index);
    void refreshSpeedButtons();

    void paintShell (juce::Graphics&);
    void paintLabel (juce::Graphics&);
    void paintWindow (juce::Graphics&);
    void paintHub (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
    void paintBottom (juce::Graphics&);
    void paintScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
    void paintArrow (juce::Graphics&);

    SilinaAudioProcessor& processor;
    CassetteLookAndFeel lookAndFeel;

    std::array<Knob, 7> knobs;
    std::array<juce::TextButton, 3> speedButtons;
    juce::ToggleButton autoGain { "AUTO GAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;

    juce::Rectangle<float> cassette;
    float reelAngle = 0.0f;
    float activity = 0.0f;
    float saturation = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CassetteDeck)
};

} // namespace silina

class SilinaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SilinaAudioProcessorEditor (SilinaAudioProcessor&);
    ~SilinaAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    silina::CassetteDeck& getDeck() noexcept { return deck; }

private:
    silina::CassetteDeck deck;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilinaAudioProcessorEditor)
};
