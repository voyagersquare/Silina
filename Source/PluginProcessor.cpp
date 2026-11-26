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

SilaAudioProcessor::SilaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SILA", createParameterLayout())
{
    inputParam    = parameters.getRawParameterValue (sila::ids::input);
    biasParam     = parameters.getRawParameterValue (sila::ids::bias);
    speedParam    = parameters.getRawParameterValue (sila::ids::speed);
    wowParam      = parameters.getRawParameterValue (sila::ids::wow);
    flutterParam  = parameters.getRawParameterValue (sila::ids::flutter);
    emphasisParam = parameters.getRawParameterValue (sila::ids::emphasis);
    hissParam     = parameters.getRawParameterValue (sila::ids::hiss);
    dropoutsParam = parameters.getRawParameterValue (sila::ids::dropouts);
    mixParam      = parameters.getRawParameterValue (sila::ids::mix);
    outputParam   = parameters.getRawParameterValue (sila::ids::output);
    autoGainParam = parameters.getRawParameterValue (sila::ids::autoGain);
}

juce::AudioProcessorValueTreeState::ParameterLayout SilaAudioProcessor::createParameterLayout()
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
        ParameterID { sila::ids::input, 1 }, "Input", NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (decibelText)));

    layout.add (percent (sila::ids::bias, "Bias", 50.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { sila::ids::speed, 1 }, "Tape Speed", StringArray { "7.5 IPS", "15 IPS", "30 IPS" }, 1));

    layout.add (percent (sila::ids::wow, "Wow", 0.0f));
    layout.add (percent (sila::ids::flutter, "Flutter", 0.0f));
    layout.add (percent (sila::ids::emphasis, "Emphasis", 50.0f));
    layout.add (percent (sila::ids::hiss, "Hiss", 0.0f));
    layout.add (percent (sila::ids::dropouts, "Dropouts", 0.0f));
    layout.add (percent (sila::ids::mix, "Mix", 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { sila::ids::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (decibelText)));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { sila::ids::autoGain, 1 }, "Auto Gain", true));

    return layout;
}

bool SilaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

sila::TapeSettings SilaAudioProcessor::readSettings() const noexcept
{
    sila::TapeSettings s;
    s.inputDb  = inputParam->load();
    s.bias     = biasParam->load() * 0.01f;
    s.speed    = (int) speedParam->load();
    s.wow      = wowParam->load() * 0.01f;
    s.flutter  = flutterParam->load() * 0.01f;
    s.emphasis = emphasisParam->load() * 0.01f;
    s.hiss     = hissParam->load() * 0.01f;
    s.dropouts = dropoutsParam->load() * 0.01f;
    s.mix      = mixParam->load() * 0.01f;
    s.outputDb = outputParam->load();
    s.autoGain = autoGainParam->load() > 0.5f;
    return s;
}

void SilaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jmax (1, getTotalNumOutputChannels());
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock, channels);
    setLatencySamples (engine.getLatencySamples());
}

void SilaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());
    engine.process (buffer);
}

juce::AudioProcessorEditor* SilaAudioProcessor::createEditor()
{
    return new SilaAudioProcessorEditor (*this);
}

void SilaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SilaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SilaAudioProcessor();
}
