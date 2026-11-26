#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String percentText (float value, int) { return juce::String (juce::roundToInt (value)) + " %"; }

    juce::String decibelText (float value, int)
    {
        const auto rounded = std::round (value * 10.0f) / 10.0f;
        return (rounded > 0.0f ? "+" : "") + juce::String (rounded, 1) + " dB";
    }
}

SilinaAudioProcessor::SilinaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SILINA", createParameterLayout())
{
    inputParam    = parameters.getRawParameterValue (silina::ids::input);
    biasParam     = parameters.getRawParameterValue (silina::ids::bias);
    speedParam    = parameters.getRawParameterValue (silina::ids::speed);
    wowParam      = parameters.getRawParameterValue (silina::ids::wow);
    flutterParam  = parameters.getRawParameterValue (silina::ids::flutter);
    emphasisParam = parameters.getRawParameterValue (silina::ids::emphasis);
    mixParam      = parameters.getRawParameterValue (silina::ids::mix);
    outputParam   = parameters.getRawParameterValue (silina::ids::output);
    autoGainParam = parameters.getRawParameterValue (silina::ids::autoGain);
}

juce::AudioProcessorValueTreeState::ParameterLayout SilinaAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
            AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText));
    };

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { silina::ids::input, 1 }, "Input", NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (decibelText)));

    layout.add (percent (silina::ids::bias, "Bias", 50.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { silina::ids::speed, 1 }, "Tape Speed", StringArray { "7.5 IPS", "15 IPS", "30 IPS" }, 1));

    layout.add (percent (silina::ids::wow, "Wow", 0.0f));
    layout.add (percent (silina::ids::flutter, "Flutter", 0.0f));
    layout.add (percent (silina::ids::emphasis, "Emphasis", 50.0f));
    layout.add (percent (silina::ids::mix, "Mix", 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { silina::ids::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (decibelText)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { silina::ids::autoGain, 1 }, "Auto Gain", true));

    return layout;
}

bool SilinaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

silina::TapeSettings SilinaAudioProcessor::readSettings() const noexcept
{
    silina::TapeSettings s;
    s.inputDb  = inputParam->load();
    s.bias     = biasParam->load() * 0.01f;
    s.speed    = (int) speedParam->load();
    s.wow      = wowParam->load() * 0.01f;
    s.flutter  = flutterParam->load() * 0.01f;
    s.emphasis = emphasisParam->load() * 0.01f;
    s.mix      = mixParam->load() * 0.01f;
    s.outputDb = outputParam->load();
    s.autoGain = autoGainParam->load() > 0.5f;
    return s;
}

void SilinaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock, channels);
    setLatencySamples (engine.getLatencySamples());
}

void SilinaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());
    engine.process (buffer);
}

juce::AudioProcessorEditor* SilinaAudioProcessor::createEditor()
{
    return new SilinaAudioProcessorEditor (*this);
}

void SilinaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SilinaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SilinaAudioProcessor();
}
