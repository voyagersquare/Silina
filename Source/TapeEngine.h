#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

namespace sila
{

// Tape speed positions exposed as the "speed" choice parameter.
enum class TapeSpeed { ips7_5 = 0, ips15 = 1, ips30 = 2 };

struct TapeSettings
{
    float inputDb   = 0.0f;   // -12 .. +24 dB drive into the tape
    float bias      = 0.5f;   // 0 = under-biased (gritty, bright), 1 = over-biased (smooth, dark)
    int   speed     = 1;      // TapeSpeed
    float wow       = 0.0f;   // 0 .. 1
    float flutter   = 0.0f;   // 0 .. 1
    float emphasis  = 0.5f;   // 0 .. 1 pre-/de-emphasis amount
    float mix       = 1.0f;   // 0 .. 1 dry/wet
    float outputDb  = 0.0f;   // -24 .. +12 dB
    bool  autoGain  = true;   // compensate output for the input drive
};

// Transposed direct form II biquad with RBJ cookbook designs.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setHighShelf (double sampleRate, double freq, double q, double gainDb) noexcept;
    void setPeak      (double sampleRate, double freq, double q, double gainDb) noexcept;
    void setLowPass   (double sampleRate, double freq, double q) noexcept;
    void setHighPass  (double sampleRate, double freq, double q) noexcept;

    // Magnitude response in dB, used by the tests.
    double magnitudeDb (double sampleRate, double freq) const noexcept;

private:
    void normalise (double a0) noexcept;
};

// Signal flow per channel:
//   input gain -> pre-emphasis -> 4x oversampled magnetic saturation (bias controls
//   asymmetry and high-frequency loss) -> de-emphasis -> head bump / low cut /
//   speed-dependent high roll-off -> wow & flutter transport delay -> auto gain
//   -> dry/wet (dry delayed by the same latency) -> output gain
class TapeEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int oversamplingOrder = 2; // 2^2 = 4x

    // Maximum transport modulation depths at 15 IPS (scaled by speed).
    static constexpr double maxWowMs = 1.2;
    static constexpr double maxFlutterMs = 0.06;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();
    void setSettings (const TapeSettings& newSettings) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    void processChunk (float* const* data, int numCh, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

    // 0 .. 1, how hard the tape is being saturated (for the UI).
    float getSaturationMeter() const noexcept { return saturationMeter.load (std::memory_order_relaxed); }
    // Output peak level (linear) of the last block.
    float getOutputPeak() const noexcept { return outputPeak.load (std::memory_order_relaxed); }

    static double emphasisFrequency (int speed) noexcept;
    static double headBumpFrequency (int speed) noexcept;
    static double headBumpGainDb (int speed) noexcept;
    static double lowCutFrequency (int speed) noexcept;
    static double highCutFrequency (int speed) noexcept;
    static double transportScale (int speed) noexcept;

private:
    void updateSpeedFilters() noexcept;
    void updateEmphasisFilters (double emphasisAmount) noexcept;
    double readDelay (int channel, double delaySamples) const noexcept;

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;

    TapeSettings target;
    int currentSpeed = -1;
    double currentEmphasis = -1.0;

    juce::SmoothedValue<float> inputGain, outputGain, mixAmount, wowDepth, flutterDepth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> biasSmoothed;
    juce::SmoothedValue<float> autoGainAmount;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int oversamplingLatency = 0;

    struct ChannelState
    {
        Biquad preEmphasis, deEmphasis, headBump, lowCut, highCut;
        double magnetisation = 0.0; // bias-dependent HF loss state at the oversampled rate
        std::vector<float> transport; // wow & flutter delay line
        std::vector<float> dry;       // latency-matched dry path
        int dryWrite = 0;
    };
    std::array<ChannelState, maxChannels> channels;

    int transportWrite = 0;
    int transportMask = 0;
    double transportBaseDelay = 0.0;
    int transportBaseDelayInt = 0;

    double wowPhase = 0.0, flutterPhase1 = 0.0, flutterPhase2 = 0.0;
    double driftState = 0.0, driftTarget = 0.0;
    int driftCounter = 0;
    juce::Random random { 0x51714A };

    int latencySamples = 0;
    int dryLength = 1;

    juce::AudioBuffer<float> dryScratch;
    std::vector<float> gainScratch;

    double meterState = 0.0;
    std::atomic<float> saturationMeter { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

} // namespace sila
