#include "TapeEngine.h"

#include <cmath>
#include <complex>

namespace sila
{

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    double clampFrequency (double freq, double sampleRate) noexcept
    {
        return juce::jlimit (5.0, sampleRate * 0.45, freq);
    }
}

//==============================================================================
void Biquad::normalise (double a0) noexcept
{
    const double inv = 1.0 / a0;
    b0 *= inv; b1 *= inv; b2 *= inv; a1 *= inv; a2 *= inv;
}

void Biquad::setHighShelf (double sr, double freq, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = twoPi * clampFrequency (freq, sr) / sr;
    const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
    const double sqA = 2.0 * std::sqrt (A) * alpha;

    b0 =  A * ((A + 1) + (A - 1) * c + sqA);
    b1 = -2 * A * ((A - 1) + (A + 1) * c);
    b2 =  A * ((A + 1) + (A - 1) * c - sqA);
    const double a0 = (A + 1) - (A - 1) * c + sqA;
    a1 =  2 * ((A - 1) - (A + 1) * c);
    a2 =  (A + 1) - (A - 1) * c - sqA;
    normalise (a0);
}

void Biquad::setPeak (double sr, double freq, double q, double gainDb) noexcept
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = twoPi * clampFrequency (freq, sr) / sr;
    const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);

    b0 = 1 + alpha * A;
    b1 = -2 * c;
    b2 = 1 - alpha * A;
    const double a0 = 1 + alpha / A;
    a1 = -2 * c;
    a2 = 1 - alpha / A;
    normalise (a0);
}

void Biquad::setLowPass (double sr, double freq, double q) noexcept
{
    const double w0 = twoPi * clampFrequency (freq, sr) / sr;
    const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);

    b0 = (1 - c) * 0.5;
    b1 = 1 - c;
    b2 = (1 - c) * 0.5;
    const double a0 = 1 + alpha;
    a1 = -2 * c;
    a2 = 1 - alpha;
    normalise (a0);
}

void Biquad::setHighPass (double sr, double freq, double q) noexcept
{
    const double w0 = twoPi * clampFrequency (freq, sr) / sr;
    const double c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);

    b0 = (1 + c) * 0.5;
    b1 = -(1 + c);
    b2 = (1 + c) * 0.5;
    const double a0 = 1 + alpha;
    a1 = -2 * c;
    a2 = 1 - alpha;
    normalise (a0);
}

double Biquad::magnitudeDb (double sr, double freq) const noexcept
{
    const std::complex<double> z = std::polar (1.0, -twoPi * freq / sr);
    const auto num = b0 + b1 * z + b2 * z * z;
    const auto den = 1.0 + a1 * z + a2 * z * z;
    return 20.0 * std::log10 (std::abs (num / den) + 1e-300);
}

//==============================================================================
// Speed-dependent voicing. Slower tape: lower emphasis corner, larger and lower
// head bump, earlier high-frequency loss and more transport instability.
double TapeEngine::emphasisFrequency (int speed) noexcept
{
    switch (speed) { case 0: return 1800.0; case 2: return 6000.0; default: return 3200.0; }
}

double TapeEngine::headBumpFrequency (int speed) noexcept
{
    switch (speed) { case 0: return 60.0; case 2: return 150.0; default: return 95.0; }
}

double TapeEngine::headBumpGainDb (int speed) noexcept
{
    switch (speed) { case 0: return 3.5; case 2: return 1.5; default: return 2.5; }
}

double TapeEngine::lowCutFrequency (int speed) noexcept
{
    switch (speed) { case 0: return 22.0; case 2: return 40.0; default: return 28.0; }
}

double TapeEngine::highCutFrequency (int speed) noexcept
{
    switch (speed) { case 0: return 11000.0; case 2: return 21000.0; default: return 17000.0; }
}

//==============================================================================
namespace
{
    constexpr double couplingA = 0.03; // alpha * Ms / a

    inline double langevin (double x) noexcept
    {
        return std::abs (x) > 1.0e-4 ? 1.0 / std::tanh (x) - 1.0 / x : x / 3.0;
    }

    inline double langevinDeriv (double x) noexcept
    {
        if (std::abs (x) <= 1.0e-4)
            return 1.0 / 3.0;
        const double s = std::sinh (x);
        return 1.0 / (x * x) - 1.0 / (s * s);
    }

    // dm/dt for normalised Jiles-Atherton magnetisation.
    inline double hysteresisRate (double m, double h, double hd, double k, double c) noexcept
    {
        const double q = h + couplingA * m;
        const double diff = langevin (q) - m;
        const double delta = hd >= 0.0 ? 1.0 : -1.0;
        const double deltaM = (delta > 0.0) == (diff > 0.0) ? 1.0 : 0.0;
        const double denom = (1.0 - c) * delta * k - couplingA * diff;
        const double irreversible = std::abs (denom) > 1.0e-9 ? (1.0 - c) * deltaM * diff / denom : 0.0;
        const double lp = langevinDeriv (q);
        return hd * (irreversible + c * lp) / (1.0 - c * couplingA * lp);
    }
}

double TapeEngine::Hysteresis::process (double h, double T, double k, double c) noexcept
{
    // Trapezoidal derivative of the field, then a midpoint (RK2) step.
    double hd = 2.0 * (h - hPrev) / T - hdPrev;
    hd = juce::jlimit (-1.0e7, 1.0e7, hd);

    const double k1 = T * hysteresisRate (m, hPrev, hdPrev, k, c);
    const double k2 = T * hysteresisRate (m + 0.5 * k1, 0.5 * (h + hPrev), 0.5 * (hd + hdPrev), k, c);
    m += k2;

    if (! std::isfinite (m))
    {
        reset();
        return 0.0;
    }

    m = juce::jlimit (-1.2, 1.2, m);
    hPrev = h;
    hdPrev = hd;
    return m;
}

double TapeEngine::hysteresisGainAt (double bias) const noexcept
{
    const double pos = juce::jlimit (0.0, 1.0, bias) * (double) (hysteresisGain.size() - 1);
    const int i = juce::jmin ((int) pos, (int) hysteresisGain.size() - 2);
    const double t = pos - (double) i;
    return hysteresisGain[(size_t) i] + (hysteresisGain[(size_t) i + 1] - hysteresisGain[(size_t) i]) * t;
}

double TapeEngine::hysteresisRestAt (double bias) const noexcept
{
    const double pos = juce::jlimit (0.0, 1.0, bias) * (double) (hysteresisRest.size() - 1);
    const int i = juce::jmin ((int) pos, (int) hysteresisRest.size() - 2);
    const double t = pos - (double) i;
    return hysteresisRest[(size_t) i] + (hysteresisRest[(size_t) i + 1] - hysteresisRest[(size_t) i]) * t;
}

double TapeEngine::transportScale (int speed) noexcept
{
    switch (speed) { case 0: return 1.3; case 2: return 0.7; default: return 1.0; }
}

//==============================================================================
void TapeEngine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate = newSampleRate > 0 ? newSampleRate : 44100.0;
    numChannels = juce::jlimit (1, maxChannels, newNumChannels);
    maxBlock = juce::jmax (1, maxBlockSize);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, (size_t) oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    oversamplingLatency = (int) std::lround (oversampling->getLatencyInSamples());

    // Measure the tape's gain at a moderate level (h = 0.5, about -24 dBFS) for each bias
    // setting, so quiet material passes at roughly unity and loud material is compressed.
    {
        const double osRate = sampleRate * (double) (1 << oversamplingOrder);
        const double T = 1.0 / osRate;
        const int total = (int) (osRate * 0.03), settle = total / 2;
        for (size_t i = 0; i < hysteresisGain.size(); ++i)
        {
            const double bias = (double) i / (double) (hysteresisGain.size() - 1);
            Hysteresis probe;
            double peak = 1.0e-9;
            for (int n = 0; n < total; ++n)
            {
                const double h = 0.5 * std::sin (twoPi * 400.0 * (double) n * T);
                const double m = probe.process (h, T, biasCoercivity (bias), biasReversibility (bias));
                if (n >= settle)
                    peak = juce::jmax (peak, std::abs (m));
            }
            hysteresisGain[i] = peak / 0.5;

            // Where the tape settles with only the asymmetry offset applied (silence).
            Hysteresis rest;
            const int ramp = (int) (osRate * 0.01);
            double m = 0.0;
            for (int n = 0; n <= ramp; ++n)
                m = rest.process (biasAsymmetry (bias) * (double) n / (double) ramp, T, biasCoercivity (bias), biasReversibility (bias));
            hysteresisRest[i] = m;
        }
    }

    const double maxDeviation = (maxWowMs + maxFlutterMs) * transportScale (0) * 0.001 * sampleRate;
    transportBaseDelayInt = (int) std::ceil (maxDeviation) + 3;
    transportBaseDelay = (double) transportBaseDelayInt;

    const int transportSize = juce::nextPowerOfTwo (transportBaseDelayInt * 2 + 8);
    transportMask = transportSize - 1;

    latencySamples = oversamplingLatency + transportBaseDelayInt;
    dryLength = juce::jmax (1, latencySamples);

    for (auto& ch : channels)
    {
        ch.transport.assign ((size_t) transportSize, 0.0f);
        ch.dry.assign ((size_t) dryLength, 0.0f);
    }

    dryScratch.setSize (numChannels, maxBlock, false, false, true);
    gainScratch.assign ((size_t) maxBlock, 1.0f);

    const double smoothing = 0.03;
    inputGain.reset (sampleRate, smoothing);
    outputGain.reset (sampleRate, smoothing);
    mixAmount.reset (sampleRate, smoothing);
    wowDepth.reset (sampleRate, 0.1);
    flutterDepth.reset (sampleRate, 0.1);
    biasSmoothed.reset (sampleRate, 0.05);
    autoGainAmount.reset (sampleRate, smoothing);

    currentSpeed = -1;
    currentEmphasis = -1.0;
    reset();
}

void TapeEngine::reset()
{
    if (oversampling != nullptr)
        oversampling->reset();

    for (auto& ch : channels)
    {
        ch.preEmphasis.reset(); ch.deEmphasis.reset(); ch.headBump.reset();
        ch.lowCut.reset(); ch.highCut.reset();
        ch.tape.reset();
        ch.tape.m = hysteresisRestAt (target.bias);
        ch.tape.hPrev = biasAsymmetry (target.bias);
        ch.hfLoss = 0.0;
        std::fill (ch.transport.begin(), ch.transport.end(), 0.0f);
        std::fill (ch.dry.begin(), ch.dry.end(), 0.0f);
        ch.dryWrite = 0;
    }

    transportWrite = 0;
    wowPhase = flutterPhase1 = flutterPhase2 = 0.0;
    driftState = driftTarget = 0.0;
    driftCounter = 0;
    meterState = 0.0;
    saturationMeter.store (0.0f);
    outputPeak.store (0.0f);

    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (target.inputDb));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (target.outputDb));
    mixAmount.setCurrentAndTargetValue (target.mix);
    wowDepth.setCurrentAndTargetValue (target.wow);
    flutterDepth.setCurrentAndTargetValue (target.flutter);
    biasSmoothed.setCurrentAndTargetValue (target.bias);
    autoGainAmount.setCurrentAndTargetValue (target.autoGain ? 1.0f : 0.0f);

    currentSpeed = -1;
    updateSpeedFilters();
    currentEmphasis = target.emphasis;
    updateEmphasisFilters (currentEmphasis);
}

void TapeEngine::setSettings (const TapeSettings& s) noexcept
{
    target = s;
    target.bias = juce::jlimit (0.0f, 1.0f, s.bias);
    target.wow = juce::jlimit (0.0f, 1.0f, s.wow);
    target.flutter = juce::jlimit (0.0f, 1.0f, s.flutter);
    target.emphasis = juce::jlimit (0.0f, 1.0f, s.emphasis);
    target.mix = juce::jlimit (0.0f, 1.0f, s.mix);
    target.speed = juce::jlimit (0, 2, s.speed);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (target.inputDb));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (target.outputDb));
    mixAmount.setTargetValue (target.mix);
    wowDepth.setTargetValue (target.wow);
    flutterDepth.setTargetValue (target.flutter);
    biasSmoothed.setTargetValue (target.bias);
    autoGainAmount.setTargetValue (target.autoGain ? 1.0f : 0.0f);
}

void TapeEngine::updateSpeedFilters() noexcept
{
    if (currentSpeed == target.speed)
        return;

    currentSpeed = target.speed;
    for (auto& ch : channels)
    {
        ch.headBump.setPeak (sampleRate, headBumpFrequency (currentSpeed), 1.1, headBumpGainDb (currentSpeed));
        ch.lowCut.setHighPass (sampleRate, lowCutFrequency (currentSpeed), 0.707);
        ch.highCut.setLowPass (sampleRate, highCutFrequency (currentSpeed), 0.62);
    }

    // The emphasis corner also depends on speed.
    updateEmphasisFilters (currentEmphasis < 0 ? target.emphasis : currentEmphasis);
}

void TapeEngine::updateEmphasisFilters (double amount) noexcept
{
    const double gainDb = 12.0 * amount;
    const double freq = emphasisFrequency (currentSpeed < 0 ? target.speed : currentSpeed);
    for (auto& ch : channels)
    {
        ch.preEmphasis.setHighShelf (sampleRate, freq, 0.707, gainDb);
        ch.deEmphasis.setHighShelf (sampleRate, freq, 0.707, -gainDb);
    }
}

double TapeEngine::readDelay (int channel, double delaySamples) const noexcept
{
    const auto& buf = channels[(size_t) channel].transport;
    const double readPos = (double) transportWrite - delaySamples;
    const double floorPos = std::floor (readPos);
    const double t = readPos - floorPos;
    const int i1 = (int) floorPos;

    const double y0 = buf[(size_t) ((i1 - 1) & transportMask)];
    const double y1 = buf[(size_t) ( i1      & transportMask)];
    const double y2 = buf[(size_t) ((i1 + 1) & transportMask)];
    const double y3 = buf[(size_t) ((i1 + 2) & transportMask)];

    // 4-point cubic Hermite interpolation; exact at t == 0.
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

//==============================================================================
void TapeEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numCh = juce::jmin (buffer.getNumChannels(), numChannels);
    const int total = buffer.getNumSamples();
    if (numCh <= 0 || total <= 0 || oversampling == nullptr)
        return;

    float peak = 0.0f;
    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - start);
        float* chunk[maxChannels] {};
        for (int ch = 0; ch < numCh; ++ch)
            chunk[ch] = buffer.getWritePointer (ch, start);

        processChunk (chunk, numCh, n);

        for (int ch = 0; ch < numCh; ++ch)
            peak = juce::jmax (peak, juce::FloatVectorOperations::findMaximum (chunk[ch], n),
                               -juce::FloatVectorOperations::findMinimum (chunk[ch], n));
    }

    outputPeak.store (peak, std::memory_order_relaxed);
}

void TapeEngine::processChunk (float* const* data, int numCh, int n) noexcept
{
    updateSpeedFilters();

    // Emphasis amount moves gradually so pre- and de-emphasis stay matched.
    if (std::abs (currentEmphasis - target.emphasis) > 1.0e-5)
    {
        const double step = 5.0 * (double) n / sampleRate; // full sweep in ~200 ms
        currentEmphasis += juce::jlimit (-step, step, (double) target.emphasis - currentEmphasis);
        updateEmphasisFilters (currentEmphasis);
    }

    // Keep an untouched copy for the dry path.
    for (int ch = 0; ch < numCh; ++ch)
        dryScratch.copyFrom (ch, 0, data[ch], n);

    // 1) Input drive and pre-emphasis.
    for (int i = 0; i < n; ++i)
    {
        const float g = inputGain.getNextValue();
        gainScratch[(size_t) i] = g;
        for (int ch = 0; ch < numCh; ++ch)
            data[ch][i] = (float) channels[(size_t) ch].preEmphasis.process ((double) data[ch][i] * g);
    }

    // 2) Magnetic saturation at 4x.
    juce::dsp::AudioBlock<float> block (data, (size_t) numCh, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    const int upN = (int) up.getNumSamples();
    const double osRate = sampleRate * (double) (1 << oversamplingOrder);

    const double biasStart = biasSmoothed.getCurrentValue();
    biasSmoothed.skip (n);
    const double biasEnd = biasSmoothed.getCurrentValue();

    double inputEnergy = 0.0, errorEnergy = 0.0;

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* x = up.getChannelPointer ((size_t) ch);
        auto& state = channels[(size_t) ch];

        double lp = state.hfLoss;
        const double T = 1.0 / osRate;

        double prevBias = -1.0, k = 1.0, c = 0.7, asym = 0.0, rest = 0.0, norm = 1.0, coeff = 1.0;
        for (int j = 0; j < upN; ++j)
        {
            const double bias = biasStart + (biasEnd - biasStart) * (double) (j + 1) / (double) upN;
            if (std::abs (bias - prevBias) > 1.0e-9)
            {
                prevBias = bias;
                // Under-bias: wide hysteresis, gritty and slightly asymmetric (even harmonics).
                // Over-bias: narrow hysteresis, smooth, with a darker top end.
                k = biasCoercivity (bias);
                c = biasReversibility (bias);
                asym = biasAsymmetry (bias);
                rest = hysteresisRestAt (bias);
                norm = 1.0 / (hysteresisGainAt (bias) * recordLevel);
                const double fc = juce::jmin (26000.0 * std::pow (0.5, bias), osRate * 0.45);
                coeff = 1.0 - std::exp (-twoPi * fc / osRate);
            }

            const double in = (double) x[j];
            const double m = state.tape.process (in * recordLevel + asym, T, k, c);
            const double shaped = (m - rest) * norm;
            lp += coeff * (shaped - lp);

            inputEnergy += in * in;
            const double err = in - shaped;
            errorEnergy += err * err;

            x[j] = (float) lp;
        }

        state.hfLoss = std::abs (lp) < 1.0e-20 ? 0.0 : lp;
    }

    oversampling->processSamplesDown (block);

    // Saturation meter (fast attack, slow release).
    const double drive = inputEnergy > 1.0e-12 ? std::sqrt (errorEnergy / inputEnergy) : 0.0;
    const double meterTarget = juce::jlimit (0.0, 1.0, drive * 3.0);
    const double release = std::exp (-(double) n / (0.35 * sampleRate));
    meterState = meterTarget > meterState ? meterTarget : meterState * release + meterTarget * (1.0 - release);
    saturationMeter.store ((float) meterState, std::memory_order_relaxed);

    // 3) Playback EQ, transport and output stage.
    const double scale = transportScale (currentSpeed);
    const double wowMax = maxWowMs * 0.001 * sampleRate * scale;
    const double flutterMax = maxFlutterMs * 0.001 * sampleRate * scale;
    const double wowInc = twoPi * 0.55 / sampleRate;
    const double flutterInc1 = twoPi * 6.3 / sampleRate;
    const double flutterInc2 = twoPi * 11.7 / sampleRate;
    const int driftPeriod = juce::jmax (1, (int) (sampleRate * 0.45));
    const double driftCoeff = 1.0 - std::exp (-1.0 / (0.35 * sampleRate));

    for (int i = 0; i < n; ++i)
    {
        // Transport modulation (shared by both channels: one tape, one capstan).
        if (--driftCounter <= 0)
        {
            driftCounter = driftPeriod;
            driftTarget = random.nextDouble() * 2.0 - 1.0;
        }
        driftState += driftCoeff * (driftTarget - driftState);

        const double wowSignal = 0.7 * std::sin (wowPhase) + 0.3 * driftState;
        const double flutterSignal = 0.65 * std::sin (flutterPhase1) + 0.35 * std::sin (flutterPhase2);
        wowPhase += wowInc;           if (wowPhase > twoPi) wowPhase -= twoPi;
        flutterPhase1 += flutterInc1; if (flutterPhase1 > twoPi) flutterPhase1 -= twoPi;
        flutterPhase2 += flutterInc2; if (flutterPhase2 > twoPi) flutterPhase2 -= twoPi;

        const double wowAmt = wowDepth.getNextValue();
        const double flutterAmt = flutterDepth.getNextValue();
        const double deviation = wowAmt * wowMax * wowSignal + flutterAmt * flutterMax * flutterSignal;
        const double delay = transportBaseDelay + deviation;

        const double inv = 1.0 / (double) gainScratch[(size_t) i];
        const double autoAmt = autoGainAmount.getNextValue();
        const double makeup = 1.0 + (inv - 1.0) * autoAmt;
        const double wetMix = mixAmount.getNextValue();
        const double out = outputGain.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& s = channels[(size_t) ch];
            double y = (double) data[ch][i];
            y = s.deEmphasis.process (y);
            y = s.headBump.process (y);
            y = s.lowCut.process (y);
            y = s.highCut.process (y);

            s.transport[(size_t) transportWrite] = (float) y;
            const double wet = readDelay (ch, delay) * makeup;

            // Latency-matched dry signal.
            const double dry = (double) s.dry[(size_t) s.dryWrite];
            s.dry[(size_t) s.dryWrite] = dryScratch.getSample (ch, i);

            data[ch][i] = (float) ((dry + (wet - dry) * wetMix) * out);
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& s = channels[(size_t) ch];
            if (++s.dryWrite >= dryLength)
                s.dryWrite = 0;
        }
        transportWrite = (transportWrite + 1) & transportMask;
    }
}

} // namespace sila
