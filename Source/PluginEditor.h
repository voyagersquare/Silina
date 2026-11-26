#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace sila
{

// Hand-written marker font used for every piece of text on the plugin.
juce::Font handFont (float height);

// Black bakelite dial knobs with a numbered skirt, hand-written labels.
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

    static void drawDialKnob (juce::Graphics&, juce::Point<float> centre, float radius, float position, int wearSeed = 0);
};

// The cassette lying on a wooden desk, drawn at a fixed logical size and scaled by the editor.
class CassetteDeck : public juce::Component, private juce::Timer
{
public:
    static constexpr int logicalWidth = 1400;
    static constexpr int logicalHeight = 820;

    explicit CassetteDeck (SilaAudioProcessor&);
    ~CassetteDeck() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // Exposed for tests.
    juce::Slider* findKnob (const juce::String& paramId) noexcept;
    juce::TextButton& getSpeedButton (int index) noexcept { return speedButtons[(size_t) index]; }
    juce::ToggleButton& getAutoGainButton() noexcept { return autoGain; }
    juce::Rectangle<float> getCassetteBounds() const noexcept { return cassette; }
    float getReelAngle() const noexcept { return reelAngleLeft; }
    float getTapePosition() const noexcept { return tapePosition; }
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

    void paintDesk (juce::Graphics&);
    void paintPapers (juce::Graphics&);
    void paintShell (juce::Graphics&);
    void paintLabel (juce::Graphics&);
    void paintWindow (juce::Graphics&);
    void paintTapePack (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
    void paintHub (juce::Graphics&, juce::Point<float> centre, float radius, float angle);
    void paintBottom (juce::Graphics&);
    void paintScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle);

    float packRadius (bool left) const noexcept;

    SilaAudioProcessor& processor;
    CassetteLookAndFeel lookAndFeel;

    std::array<Knob, 7> knobs;
    std::array<juce::TextButton, 3> speedButtons;
    juce::ToggleButton autoGain { "AUTO GAIN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;

    juce::Rectangle<float> cassette;
    juce::Image desk;
    juce::Image wear; // scratches, grime and stains over the cassette

    float reelAngleLeft = 0.0f, reelAngleRight = 0.7f;
    float tapePosition = 0.35f; // 0 = all tape on the left reel, 1 = all on the right

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CassetteDeck)
};

} // namespace sila

class SilaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SilaAudioProcessorEditor (SilaAudioProcessor&);
    ~SilaAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    sila::CassetteDeck& getDeck() noexcept { return deck; }

private:
    sila::CassetteDeck deck;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilaAudioProcessorEditor)
};
