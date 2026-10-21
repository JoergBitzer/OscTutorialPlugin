/*
    WavetableOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: wavetable oscillator. For each waveform one table per octave of the fundamental
    frequency, made by additive synthesis (Fourier series) with only the harmonics that stay below fs/2
    for the highest fundamental of the octave. Read with linear interpolation.
    With Lanczos sigma factors (a separate model in the list, the constructor flag): a cut-off Fourier series rings at every jump (Gibbs phenomenon,
    about 9 % overshoot, independent of the number of harmonics). Multiplying harmonic k by
    sigma_k = sinc(k / (H + 1)) (H = number of harmonics) smooths the cut-off and reduces the
    overshoot to about 1 %, at the price of a slightly duller sound.
    License: MIT
*/
#pragma once
#include <array>
#include <vector>
#include "Oscillator.h"

namespace vss
{
class WavetableOscillator : public Oscillator
{
public:
    explicit WavetableOscillator(bool withLanczosSigma);
    void prepare(double sampleRate) override;
    void setFrequency(double frequencyHz) override;
    float processSample() override;

    static constexpr int kTableSize = 2048;                   // samples per period (a power of 2)
    static constexpr int kNumTables = 10;                     // one table per octave
    static constexpr double kTopFrequencyOfFirstTable = 40.0; // table k: fundamentals up to 40 Hz * 2^k
    static constexpr int kMaxHarmonics = kTableSize / 4;      // >= 4 table samples per period of the highest harmonic

private:
    void buildTables();
    void fillTable(std::vector<float>& table, Waveform waveform, int numHarmonics, const std::vector<double>& sineTable);
    // amplitude of harmonic k in the Fourier series (sine terms) of a waveform
    static double harmonicAmplitude(Waveform waveform, int k);
    static double lanczosSigma(int k, int numHarmonics);

    bool m_withLanczosSigma;
    // m_tables[waveform][octave]: kTableSize + 1 samples (the last one = the first, for the interpolation)
    std::array<std::array<std::vector<float>, kNumTables>, kNumWaveforms> m_tables;
    double m_tablesSampleRate = 0.0; // the tables are only built again if the sample rate changes
    int m_tableIndex = 0;
};
} // namespace vss
