#include <cmath>
#include "PolyBlepOscillator.h"

namespace
{
constexpr double kHalfPeriod = 0.5;
constexpr double kReachTwoPoint = 1.0;  // the 2-point correction acts within 1 sample of the discontinuity
constexpr double kReachFourPoint = 2.0; // the 4-point correction within 2 samples
} // namespace

namespace vss
{
PolyBlepOscillator::PolyBlepOscillator(Order order)
: m_order(order)
{
}

// --- 2-point: the band-limited impulse is a triangle of width 2 samples
double PolyBlepOscillator::blep2(double d)
{
    const double a = std::abs(d);
    if (a >= kReachTwoPoint)
    {
        return 0.0;
    }
    const double x = 1.0 - a;
    if (d < 0.0)
    {
        return 0.5 * x * x; // before the jump: the band-limited step has already begun to rise
    }
    return -0.5 * x * x; // after the jump: it has not yet reached the top
}

double PolyBlepOscillator::blamp2(double d)
{
    // the integral of blep2; even in d
    constexpr double kOneSixth = 1.0 / 6.0;
    const double a = std::abs(d);
    if (a >= kReachTwoPoint)
    {
        return 0.0;
    }
    const double x = 1.0 - a;
    return kOneSixth * x * x * x;
}

// --- 4-point: the band-limited impulse is a cubic B-spline of width 4 samples
double PolyBlepOscillator::blep4(double d)
{
    const double a = std::abs(d);
    if (a >= kReachFourPoint)
    {
        return 0.0;
    }
    // residual before the jump (d = -a), from integrating the B-spline; after the jump it is the negative (odd in d)
    double residualBefore = 0.0;
    if (a >= 1.0)
    {
        const double x = 2.0 - a;
        residualBefore = x * x * x * x / 24.0;
    }
    else
    {
        const double a2 = a * a;
        residualBefore = 0.5 - 2.0 * a / 3.0 + a2 * a / 3.0 - a2 * a2 / 8.0;
    }
    if (d < 0.0)
    {
        return residualBefore;
    }
    return -residualBefore;
}

double PolyBlepOscillator::blamp4(double d)
{
    // the integral of blep4; even in d
    const double a = std::abs(d);
    if (a >= kReachFourPoint)
    {
        return 0.0;
    }
    if (a >= 1.0)
    {
        const double x = 2.0 - a;
        return x * x * x * x * x / 120.0;
    }
    const double a2 = a * a;
    return 7.0 / 30.0 - a / 2.0 + a2 / 3.0 - a2 * a2 / 12.0 + a2 * a2 * a / 40.0;
}

float PolyBlepOscillator::processSample()
{
    double value = naiveWaveform(m_waveform, m_phase);
    const double dt = m_phaseIncrement;
    if (dt > 0.0)
    {
        Discontinuity discontinuities[kMaxDiscontinuities];
        const int count = getDiscontinuities(m_waveform, discontinuities);
        for (int i = 0; i < count; ++i)
        {
            // signed distance to the nearest occurrence of the discontinuity, in samples
            double phaseDistance = wrapPhase(m_phase - discontinuities[i].phase);
            if (phaseDistance >= kHalfPeriod)
            {
                phaseDistance -= 1.0;
            }
            const double d = phaseDistance / dt;
            // the slope change per sample = per period * dt
            const double slopeChangePerSample = discontinuities[i].slopeChange * dt;
            if (m_order == Order::TwoPoint)
            {
                value += discontinuities[i].jump * blep2(d) + slopeChangePerSample * blamp2(d);
            }
            else
            {
                value += discontinuities[i].jump * blep4(d) + slopeChangePerSample * blamp4(d);
            }
        }
    }
    advancePhase();
    return static_cast<float>(value);
}
} // namespace vss
