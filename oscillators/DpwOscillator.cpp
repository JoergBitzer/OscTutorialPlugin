#include <cmath>
#include "DpwOscillator.h"

namespace
{
constexpr double kHalfPeriod = 0.5;
constexpr double kQuarterPeriod = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
constexpr double kMinPhaseIncrement = 1e-9; // below: no division by dt, use the naive value
} // namespace

namespace vss
{
double DpwOscillator::antiderivative(Waveform waveform, double phase)
{
    const double p = wrapPhase(phase);
    switch (waveform)
    {
        case Waveform::Sine:
            return -std::cos(kTwoPi * p) / kTwoPi;
        case Waveform::Triangle:
            // integrals of the three straight lines, continuous at 0.25 and 0.75, F(0) = F(1) = 0
            if (p < kQuarterPeriod)
            {
                return 2.0 * p * p;
            }
            if (p < kThreeQuarterPeriod)
            {
                return 2.0 * p - 2.0 * p * p - 0.25;
            }
            return 2.0 * p * p - 4.0 * p + 2.0;
        case Waveform::Rectangle:
            // +1 then -1: up and down
            if (p < kHalfPeriod)
            {
                return p;
            }
            return 1.0 - p;
        case Waveform::Sawtooth:
        {
            // s = 2 p' - 1 with ds/dp = 2  ->  F = s^2 / 4 (the parabola)
            const double s = naiveWaveform(Waveform::Sawtooth, p);
            return 0.25 * s * s;
        }
    }
    return 0.0;
}

float DpwOscillator::processSample()
{
    const double dt = m_phaseIncrement;
    double out = 0.0;
    if (dt < kMinPhaseIncrement)
    {
        out = naiveWaveform(m_waveform, m_phase);
    }
    else
    {
        const double halfStep = 0.5 * dt;
        out = (antiderivative(m_waveform, m_phase + halfStep) - antiderivative(m_waveform, m_phase - halfStep)) / dt;
    }
    advancePhase();
    return static_cast<float>(out);
}
} // namespace vss
