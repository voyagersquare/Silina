#include <JuceHeader.h>
#include "TapeEngine.h"
#include <chrono>
int main() {
    sila::TapeEngine e; sila::TapeSettings s; s.inputDb = 6; e.setSettings(s);
    e.prepare(44100, 512, 2);
    juce::AudioBuffer<float> b(2, 512); juce::Random r;
    auto t0 = std::chrono::steady_clock::now();
    const int blocks = 44100 * 10 / 512;
    for (int i = 0; i < blocks; ++i) { for (int c = 0; c < 2; ++c) for (int n = 0; n < 512; ++n) b.setSample(c, n, r.nextFloat() - 0.5f); e.process(b); }
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("10 s stereo 44.1k processed in %.3f s -> %.1f %% of one core\n", sec, sec * 10.0);
}
