/*
    OversamplingOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: the brute-force method: the naive waveform at 8 times the sample rate, a low-pass
    filter (FIR, 512 taps, windowed sinc, cut-off 0.42 fs), then every 8th sample. Harmonics between
    fs/2 and 4 fs no longer alias, they are removed by the filter; only the (weak) harmonics above 4 fs
    still fold back.
    Teaching points: easy to understand, works for every waveform (and every non-linearity), but costs
    8 naive samples and 512 multiplications per output sample. The filter delays by 255.5 samples at the
    high rate; the oscillator runs that much ahead (phase lead), so it stays phase aligned with the
    other models.
    License: MIT
*/
#pragma once
#include <vector>
#include "Oscillator.h"

namespace otp
{
class OversamplingOscillator : public Oscillator
{
public:
    OversamplingOscillator();
    float processSample() override;

    static constexpr int kFactor = 8;
    static constexpr int kNumTaps = 512;
    static constexpr double kCutoff = 0.42; // in units of the (low) sample rate fs

protected:
    void restart() override;

private:
    void designFilter();
    void push(double value);
    // the filter delays by (kNumTaps - 1) / 2 high-rate samples: the phase lead that compensates it
    double phaseLead() const;

    std::vector<double> m_filter;
    // the last kNumTaps high-rate samples, written twice (at i and i + kNumTaps), so that the
    // newest kNumTaps samples are always contiguous: m_history[m_writeIndex .. m_writeIndex + kNumTaps - 1]
    std::vector<double> m_history;
    int m_writeIndex = 0;
};
} // namespace otp
