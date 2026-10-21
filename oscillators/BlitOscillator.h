/*
    BlitOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: BLIT oscillator (Stilson & Smith 1996). A band-limited impulse train (BLIT), one impulse
    per period, is computed in closed form (a Dirichlet kernel with M = 2 floor(P/2) + 1 harmonics below fs/2,
    P = period in samples):
        blit(x) = (1/P) * sin(pi M x) / sin(pi x),   x = phase distance to the impulse
    Each impulse has the area 1. The waveforms are integrals of impulse trains:
      sawtooth  = integral of 2 * (1/P - blit)             (rises by 2 per period, jumps by -2)
      rectangle = integral of 2 * (blit(x) - blit(x - 1/2)) (jumps +2 and -2)
      triangle  = integral of 4/P * rectangle              (a second integrator)
    The integrators are "leaky" (factor slightly below 1, here 1 Hz), otherwise numerical errors
    and the DC of the sampled BLIT would accumulate. Teaching point: the leak tilts the flat parts of
    the rectangle at low notes (a high-pass at 1 Hz; about 9 % at 33 Hz) -- visible on the oscilloscope.
    Second teaching point: the integrator is a running sum, y[n] = y[n-1] + x[n], gain 1/(1 - z^-1). Compared
    to a true integrator (1/(j w)) it boosts the harmonics near fs/2, up to pi/2 = +4 dB at fs/2. The
    jumps therefore overshoot more than the Gibbs phenomenon alone (rectangle: about 1.3 instead of 1.18)
    and ring at almost fs/2 (alternating samples on the oscilloscope).
    License: MIT
*/
#pragma once
#include "Oscillator.h"

namespace vss
{
class BlitOscillator : public Oscillator
{
public:
    void prepare(double sampleRate) override;
    float processSample() override;

    static constexpr double kLeakFrequencyHz = 1.0;
    // the triangle path has two integrators in series: the second one amplifies a DC error of the first by
    // about 1/(1 - leak) (1300 times at 1 Hz), so its errors (e.g. after a start) must die out faster
    static constexpr double kTriangleLeakFrequencyHz = 5.0;
    static constexpr int kRunInSamples = 64; // see restart()

protected:
    void restart() override;

private:
    // the band-limited impulse train at a phase distance x from the impulse
    double blit(double x) const;
    void updateIntegrators();

    double m_leak = 0.999;
    double m_triangleLeak = 0.999;
    int m_numTerms = 1; // M = 2 * (number of harmonics) + 1
    double m_sawtooth = 0.0;
    double m_rectangle = 0.0;
    double m_triangleRectangle = 0.0; // rectangle shifted by a quarter period: integrated, it gives the triangle
    double m_triangle = 0.0;
};
} // namespace vss
