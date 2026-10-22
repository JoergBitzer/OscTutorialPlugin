/*
    MinBlepOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: minBLEP oscillator (Brandt 2001): naive waveform plus a correction from a table
    after each discontinuity. The table is a band-limited step (integral of a windowed sinc, 16 zero
    crossings on each side, cut-off 0.85 fs/2) made minimum phase: all of its energy comes AFTER the jump, so the
    correction never has to look into the future and is long (32 samples) and good.
    The price: a minimum-phase filter delays (about 2.5 samples here) and changes the phase of the
    harmonics -- the waveform looks different on the oscilloscope although it sounds the same.
    Corners (triangle) get the integral of the minBLEP (minBLAMP). Its tail does not go back to 0
    (the delay of the minimum-phase step), so the straight parts are delayed by the same amount
    (term m_delayInSamples * slope), and only the rest is taken from the table.
    The table is built once in the constructor (FFT, real cepstrum).
    License: MIT
*/
#pragma once
#include <vector>
#include "Oscillator.h"

namespace otp
{
class MinBlepOscillator : public Oscillator
{
public:
    MinBlepOscillator();
    float processSample() override;

    static constexpr int kZeroCrossings = 16;  // of the windowed sinc, on each side
    static constexpr int kOversampling = 64;   // table points per sample
    static constexpr int kTableLengthSamples = 2 * kZeroCrossings;
    static constexpr double kCutoff = 0.85;    // of the band-limited impulse, in units of fs/2

protected:
    void restart() override;

private:
    void buildTables();
    // adds the correction of one discontinuity, "timeSinceEvent" (0..1 samples) ago
    void addCorrection(double jump, double slopeChangePerSample, double timeSinceEvent);
    static double readTable(const std::vector<double>& table, double timeInSamples);

    std::vector<double> m_blepResidual;      // minBLEP minus the ideal step
    std::vector<double> m_blampTransient;    // minBLAMP minus the ideal ramp, minus its final value
    double m_delayInSamples = 0.0;           // the final value of minBLAMP minus ramp (negative = delay)
    std::vector<double> m_corrections;       // ring buffer of the corrections for the next samples
    int m_readIndex = 0;
};
} // namespace otp
