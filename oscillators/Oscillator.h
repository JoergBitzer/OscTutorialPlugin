/*
    Oscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: the common part of all oscillator models (phase accumulator) and the naive model.
    The other models, one file each (see planning.md):
      WavetableOscillator.h     band-limited tables (one per octave), optional Lanczos sigma
      PolyBlepOscillator.h      naive + polyBLEP/polyBLAMP corrections, 2-point and 4-point
      MinBlepOscillator.h       naive + minimum-phase BLEP/BLAMP from a table
      BlitOscillator.h          integrated band-limited impulse trains
      DpwOscillator.h           differentiated antiderivative (DPW)
      OversamplingOscillator.h  naive at 8 fs, low-pass filter, decimation
    All waveforms are phase aligned: at phase 0 they are 0 and rising (fundamental = +sin).
    No GUI code here (the algorithm can be tested headless).
    License: MIT
*/
#pragma once

namespace vss
{
// the order must match g_paramWaveform and g_paramModel in VerySimpleSynth.h
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
    PolyBlep,
    PolyBlep4,
    MinBlep,
    Blit,
    Dpw,
    Oversampling
};

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// wraps a phase into 0..1
double wrapPhase(double phase);
// the ideal waveform at a phase (0..1), also used for the "ideal" curve of the oscilloscope
double naiveWaveform(Waveform waveform, double phase);
// its slope in units per period (the derivative with respect to the phase)
double naiveSlope(Waveform waveform, double phase);
// Blackman window, i = 0 .. length-1 (for the filters of minBLEP and oversampling)
double blackmanWindow(int i, int length);

// Where the naive waveform jumps (sawtooth, rectangle) or has a corner (triangle). The models with
// corrections (polyBLEP, minBLEP) add a correction for each of them.
struct Discontinuity
{
    double phase = 0.0;       // where (0..1)
    double jump = 0.0;        // height of the jump (value after minus value before)
    double slopeChange = 0.0; // change of the slope in units per period (slope after minus slope before)
};
constexpr int kMaxDiscontinuities = 2;
// fills "list" and returns how many discontinuities the waveform has per period (0..2)
int getDiscontinuities(Waveform waveform, Discontinuity (&list)[kMaxDiscontinuities]);

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
    double getPhaseIncrement() const;

    // returns one sample and advances the phase
    virtual float processSample() = 0;

    static constexpr double kMaxFrequencyRatio = 0.45; // highest frequency in units of fs (below fs/2)

protected:
    void advancePhase();
    // models with an inner state (integrators, correction buffers, filters) set it here to fit
    // the current phase and waveform; called after reset(), setPhase() and a change of the waveform
    virtual void restart() {}

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
} // namespace vss
