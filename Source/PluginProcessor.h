#pragma once

#include <JuceHeader.h>
#include "TapeEngine.h"

namespace sila::ids
{
    inline constexpr const char* input    = "input";
    inline constexpr const char* bias     = "bias";
    inline constexpr const char* speed    = "speed";
    inline constexpr const char* wow      = "wow";
    inline constexpr const char* flutter  = "flutter";
    inline constexpr const char* emphasis = "emphasis";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
    inline constexpr const char* autoGain = "autogain";
}

class SilaAudioProcessor : public juce::AudioProcessor
{
public:
    SilaAudioProcessor();
    ~SilaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState& getState() noexcept { return parameters; }
    const sila::TapeEngine& getEngine() const noexcept { return engine; }

    // Current transport speed position (0, 1, 2) for the reel animation.
    int getSpeedIndex() const noexcept { return (int) speedParam->load(); }

private:
    sila::TapeSettings readSettings() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    sila::TapeEngine engine;

    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* biasParam = nullptr;
    std::atomic<float>* speedParam = nullptr;
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* emphasisParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* autoGainParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SilaAudioProcessor)
};
