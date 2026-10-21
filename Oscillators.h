/*
    Oscillators.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: three models of the same oscillator, for teaching (see planning.md):
      - NaiveOscillator:     the ideal waveform, sampled -> aliasing
      - WavetableOscillator: band-limited tables (one per octave), linear interpolation
      - PolyBlepOscillator:  naive waveform + polyBLEP (jumps) / polyBLAMP (corners) correction
    All waveforms are phase aligned: at phase 0 they are 0 and rising (fundamental = +sin).
    No GUI code here (the algorithm can be tested headless).
    License: MIT
*/
#pragma once
#include <array>
#include <vector>

namespace vss
{
enum class Waveform
{
    Sine = 0,
    Triangle,
    Rectangle,
    Sawtooth
};
constexpr int kNumWaveforms = 4;

enum class OscillatorModel
{
    Naive = 0,
    Wavetable,
    PolyBlep
};

// The common part of all models: the phase accumulator (phase 0..1, increment f/fs).
class Oscillator
{
public:
    virtual ~Oscillator() = default;

    virtual void prepare(double sampleRate);
    // the frequency is limited to kMaxFrequencyRatio * fs
    virtual void setFrequency(double frequencyHz);
    void setWaveform(Waveform waveform);
    void reset();
    void setPhase(double phase);
    double getPhase() const;

    // returns one sample and advances the phase
    virtual float processSample() = 0;

    static constexpr double kMaxFrequencyRatio = 0.45; // highest frequency in units of fs (below fs/2)

protected:
    void advancePhase();
    // naive (trivially sampled) waveforms, used by NaiveOscillator and PolyBlepOscillator
    float naiveSample() const;

    double m_sampleRate = 48000.0;
    double m_phase = 0.0;
    double m_phaseIncrement = 0.0;
    Waveform m_waveform = Waveform::Sawtooth;
};

class NaiveOscillator : public Oscillator
{
public:
    float processSample() override;
};

class WavetableOscillator : public Oscillator
{
public:
    void prepare(double sampleRate) override;
    void setFrequency(double frequencyHz) override;
    float processSample() override;

    static constexpr int kTableSize = 2048;                 // samples per period (a power of 2)
    static constexpr int kNumTables = 10;                   // one table per octave
    static constexpr double kTopFrequencyOfFirstTable = 40.0; // table k: fundamentals up to 40 Hz * 2^k
    static constexpr int kMaxHarmonics = kTableSize / 4;    // >= 4 table samples per period of the highest harmonic

private:
    void buildTables();
    void fillTable(std::vector<float>& table, Waveform waveform, int numHarmonics, const std::vector<double>& sineTable);
    // amplitude of harmonic k in the Fourier series (sine terms) of a waveform
    static double harmonicAmplitude(Waveform waveform, int k);

    // m_tables[waveform][octave]: kTableSize + 1 samples (the last one = the first, for the interpolation)
    std::array<std::array<std::vector<float>, kNumTables>, kNumWaveforms> m_tables;
    double m_tablesSampleRate = 0.0; // the tables are only built again if the sample rate changes
    int m_tableIndex = 0;
};

class PolyBlepOscillator : public Oscillator
{
public:
    float processSample() override;

private:
    // t: phase distance after the discontinuity (0..1), dt: phase increment.
    // Residual of a jump of height +2 (subtract it for a jump of -2).
    static double polyBlep(double t, double dt);
    // Residual of a change of the slope by +1 per sample (a corner).
    static double polyBlamp(double t, double dt);
};
} // namespace vss
