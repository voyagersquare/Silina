// Silina processor, DSP and editor checks.
// Usage: silina_processor_tests [directory-for-ui-snapshot]

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

namespace
{
    int failures = 0;

    void require (bool condition, const char* what)
    {
        std::printf ("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what);
        if (! condition)
            ++failures;
    }

    void setParam (SilinaAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.getState().getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void prepare (SilinaAudioProcessor& p, double rate, int block, int channels = 2)
    {
        const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.setRateAndBufferSizeDetails (rate, block);
        p.prepareToPlay (rate, block);
    }

    // Render a mono generator through the processor in blocks; returns channel 0.
    std::vector<float> render (SilinaAudioProcessor& p, int numSamples, int block,
                               const std::function<float (int)>& generator)
    {
        std::vector<float> out ((size_t) numSamples);
        juce::MidiBuffer midi;
        const int channels = p.getTotalNumOutputChannels();
        for (int start = 0; start < numSamples; start += block)
        {
            const int n = juce::jmin (block, numSamples - start);
            juce::AudioBuffer<float> buffer (channels, n);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < n; ++i)
                    buffer.setSample (ch, i, generator (start + i));
            p.processBlock (buffer, midi);
            for (int i = 0; i < n; ++i)
                out[(size_t) (start + i)] = buffer.getSample (0, i);
        }
        return out;
    }

    // Amplitude of a frequency component (Goertzel with Hann window).
    double amplitudeAt (const std::vector<float>& x, size_t from, size_t count, double freq, double rate)
    {
        const double w = juce::MathConstants<double>::twoPi * freq / rate;
        double re = 0, im = 0, windowSum = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (count - 1));
            const double v = x[from + i] * win;
            re += v * std::cos (w * (double) i);
            im -= v * std::sin (w * (double) i);
            windowSum += win;
        }
        return 2.0 * std::sqrt (re * re + im * im) / windowSum;
    }

    double rms (const std::vector<float>& x, size_t from, size_t count)
    {
        double sum = 0;
        for (size_t i = from; i < from + count; ++i)
            sum += (double) x[i] * x[i];
        return std::sqrt (sum / (double) count);
    }

    bool allFinite (const std::vector<float>& x)
    {
        for (auto v : x)
            if (! std::isfinite (v) || std::abs (v) > 64.0f)
                return false;
        return true;
    }

    std::function<float (int)> sine (double freq, double amp, double rate)
    {
        return [=] (int i) { return (float) (amp * std::sin (juce::MathConstants<double>::twoPi * freq * i / rate)); };
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double rate = 48000.0;

    {
        SilinaAudioProcessor p;
        prepare (p, rate, 512);
        require (p.getLatencySamples() > 0 && p.getLatencySamples() == p.getEngine().getLatencySamples(),
                 "reports the engine latency to the host");
        require (p.getLatencySamples() < (int) (rate * 0.01), "latency stays below 10 ms");
    }

    {
        // Mix 0 must be a bit-exact, latency-aligned dry path.
        SilinaAudioProcessor p;
        setParam (p, silina::ids::mix, 0.0f);
        setParam (p, silina::ids::input, 18.0f);
        setParam (p, silina::ids::wow, 100.0f);
        prepare (p, rate, 256);
        const int latency = p.getLatencySamples();
        auto out = render (p, 4096, 256, [] (int i) { return i == 100 ? 1.0f : 0.0f; });
        bool exact = true;
        for (int i = 0; i < 4096; ++i)
            exact &= std::abs (out[(size_t) i] - (i == 100 + latency ? 1.0f : 0.0f)) < 1.0e-6f;
        require (exact, "mix 0 equals the input delayed by exactly the reported latency");
    }

    {
        SilinaAudioProcessor p;
        prepare (p, rate, 512);
        auto out = render (p, 48000, 512, [] (int) { return 0.0f; });
        require (rms (out, 0, out.size()) < 1.0e-7, "silence in gives silence out");
    }

    {
        // Default settings are transparent for quiet signals (unity small-signal gain).
        SilinaAudioProcessor p;
        prepare (p, rate, 512);
        auto out = render (p, 48000, 512, sine (1000.0, 0.01, rate));
        const double gainDb = 20.0 * std::log10 (amplitudeAt (out, 24000, 16384, 1000.0, rate) / 0.01);
        std::printf ("       small-signal 1 kHz gain: %.2f dB\n", gainDb);
        require (std::abs (gainDb) < 1.0, "quiet 1 kHz passes within 1 dB at default settings");
    }

    auto harmonics = [rate] (float inputDb, float biasPercent, double& second, double& third, double& fundamentalDb)
    {
        SilinaAudioProcessor p;
        setParam (p, silina::ids::input, inputDb);
        setParam (p, silina::ids::bias, biasPercent);
        prepare (p, rate, 512);
        auto out = render (p, 65536, 512, sine (375.0, 0.25, rate)); // exact bin-friendly frequency
        const double f = amplitudeAt (out, 32768, 32768, 375.0, rate);
        second = amplitudeAt (out, 32768, 32768, 750.0, rate) / f;
        third = amplitudeAt (out, 32768, 32768, 1125.0, rate) / f;
        fundamentalDb = 20.0 * std::log10 (f / 0.25);
    };

    {
        double h2a, h3a, fa, h2b, h3b, fb;
        harmonics (0.0f, 50.0f, h2a, h3a, fa);
        harmonics (18.0f, 50.0f, h2b, h3b, fb);
        std::printf ("       THD-ish (h2+h3): input 0 dB %.4f, input +18 dB %.4f\n", h2a + h3a, h2b + h3b);
        require (h3b > 0.02 && h2b + h3b > 4.0 * (h2a + h3a), "driving the input adds harmonics");
        std::printf ("       auto-gain fundamental: %.2f dB (0 dB in) / %.2f dB (+18 dB in)\n", fa, fb);
        require (fb < fa && fb > fa - 12.0, "auto gain keeps hard drive level-matched (soft compression only)");
    }

    {
        double h2low, h3low, flow, h2high, h3high, fhigh;
        harmonics (12.0f, 0.0f, h2low, h3low, flow);
        harmonics (12.0f, 100.0f, h2high, h3high, fhigh);
        std::printf ("       2nd harmonic: bias 0 %% %.4f, bias 100 %% %.4f\n", h2low, h2high);
        require (h2low > 3.0 * h2high + 1.0e-4, "under-bias adds more even harmonics than over-bias");
    }

    {
        auto highs = [rate] (int speed)
        {
            SilinaAudioProcessor p;
            setParam (p, silina::ids::speed, (float) speed);
            prepare (p, rate, 512);
            auto out = render (p, 48000, 512, sine (15000.0, 0.01, rate));
            return 20.0 * std::log10 (amplitudeAt (out, 24000, 16384, 15000.0, rate) / 0.01);
        };
        const double slow = highs (0), fast = highs (2);
        std::printf ("       15 kHz response: 7.5 IPS %.2f dB, 30 IPS %.2f dB\n", slow, fast);
        require (slow < fast - 3.0, "7.5 IPS rolls off highs more than 30 IPS");
    }

    {
        auto renderWow = [rate] (float wow, float flutter)
        {
            SilinaAudioProcessor p;
            setParam (p, silina::ids::wow, wow);
            setParam (p, silina::ids::flutter, flutter);
            prepare (p, rate, 512);
            return render (p, 96000, 512, sine (1000.0, 0.1, rate));
        };
        auto still = renderWow (0, 0), wobbly = renderWow (100, 0), flutter = renderWow (0, 100);
        double diffW = 0, diffF = 0;
        for (size_t i = 48000; i < 96000; ++i)
        {
            diffW = std::max (diffW, (double) std::abs (still[i] - wobbly[i]));
            diffF = std::max (diffF, (double) std::abs (still[i] - flutter[i]));
        }
        require (diffW > 0.01 && allFinite (wobbly), "wow modulates the transport");
        require (diffF > 0.001 && allFinite (flutter), "flutter modulates the transport");
    }

    {
        bool ok = true;
        for (double r : { 44100.0, 96000.0, 192000.0 })
            for (int channels : { 1, 2 })
            {
                SilinaAudioProcessor p;
                setParam (p, silina::ids::input, 24.0f);
                setParam (p, silina::ids::bias, 0.0f);
                setParam (p, silina::ids::wow, 100.0f);
                setParam (p, silina::ids::flutter, 100.0f);
                setParam (p, silina::ids::emphasis, 100.0f);
                setParam (p, silina::ids::output, 12.0f);
                setParam (p, silina::ids::autoGain, 0.0f);
                setParam (p, silina::ids::speed, 0.0f);
                prepare (p, r, 64, channels);
                juce::Random rnd (7);
                // Blocks larger than announced must still work.
                auto out = render (p, (int) r, 300, [&rnd] (int) { return rnd.nextFloat() * 2.0f - 1.0f; });
                ok &= allFinite (out);
            }
        require (ok, "extreme settings stay finite at 44.1/96/192 kHz, mono and stereo, oversized blocks");
    }

    {
        SilinaAudioProcessor a;
        setParam (a, silina::ids::input, 7.5f);
        setParam (a, silina::ids::bias, 23.0f);
        setParam (a, silina::ids::speed, 2.0f);
        setParam (a, silina::ids::wow, 41.0f);
        setParam (a, silina::ids::autoGain, 0.0f);
        juce::MemoryBlock state;
        a.getStateInformation (state);

        SilinaAudioProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());
        auto value = [&b] (const char* id) { return b.getState().getRawParameterValue (id)->load(); };
        require (std::abs (value (silina::ids::input) - 7.5f) < 0.01f && std::abs (value (silina::ids::bias) - 23.0f) < 0.01f
                     && (int) value (silina::ids::speed) == 2 && std::abs (value (silina::ids::wow) - 41.0f) < 0.01f
                     && value (silina::ids::autoGain) < 0.5f,
                 "state round-trips all parameters");

        const char junk[] = "not a state";
        b.setStateInformation (junk, (int) sizeof (junk));
        require (std::abs (value (silina::ids::input) - 7.5f) < 0.01f, "invalid state is ignored");
    }

    {
        SilinaAudioProcessor p;
        prepare (p, rate, 512);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        auto* silinaEditor = dynamic_cast<SilinaAudioProcessorEditor*> (editor.get());
        require (silinaEditor != nullptr, "editor opens");

        if (silinaEditor != nullptr)
        {
            auto& deck = silinaEditor->getDeck();
            bool allKnobs = true;
            for (auto* id : { silina::ids::input, silina::ids::bias, silina::ids::wow, silina::ids::flutter,
                              silina::ids::emphasis, silina::ids::mix, silina::ids::output })
            {
                auto* knob = deck.findKnob (id);
                allKnobs &= knob != nullptr && knob->isVisible() && knob->getBottom() < 18 + 256;
            }
            require (allKnobs, "all seven knobs sit in the upper label area");

            deck.getSpeedButton (0).onClick();
            require (p.getSpeedIndex() == 0 && deck.getSpeedButton (0).getToggleState(), "speed button selects 7.5 IPS");
            deck.getSpeedButton (1).onClick();

            auto* inputKnob = deck.findKnob (silina::ids::input);
            inputKnob->setValue (12.0, juce::sendNotificationSync);
            require (std::abs (p.getState().getRawParameterValue (silina::ids::input)->load() - 12.0f) < 0.01f,
                     "turning INPUT updates the parameter");
            inputKnob->setValue (6.0, juce::sendNotificationSync);

            editor->setSize (1040 * 3 / 2, 680 * 3 / 2);
            require (std::abs (deck.getTransform().mat00 - 1.5f) < 0.01f, "editor scales the cassette when resized");
            editor->setSize (1040, 680);

            // Run some audio so the reels turn and the arrow glows, then snapshot.
            setParam (p, silina::ids::input, 14.0f);
            render (p, 9600, 480, sine (220.0, 0.4, rate));
            const float before = deck.getReelAngle();
            deck.advanceAnimation (0.5f);
            require (std::abs (deck.getReelAngle() - before) > 1.0e-4f, "reels turn while audio plays");

            if (argc > 1)
            {
                const juce::File dir { juce::String (argv[1]) };
                dir.createDirectory();
                auto file = dir.getChildFile ("preview.png");
                file.deleteFile();
                auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::FileOutputStream stream (file);
                require (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream), "UI snapshot written");
            }
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
