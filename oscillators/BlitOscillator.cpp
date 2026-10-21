#include <cmath>
#include "BlitOscillator.h"

namespace
{
constexpr double kHalfPeriod = 0.5;
constexpr double kQuarterPeriod = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
constexpr double kJump = 2.0;           // from -1 to +1
constexpr double kTriangleSlope = 4.0;  // per period
constexpr double kTinySine = 1e-9;      // below: use the limit of sin(pi M x) / sin(pi x)
} // namespace

namespace vss
{
void BlitOscillator::prepare(double sampleRate)
{
    // one-pole leak: y = leak * y + x, cut-off kLeakFrequencyHz
    m_leak = std::exp(-kTwoPi * kLeakFrequencyHz / sampleRate);
    Oscillator::prepare(sampleRate);
}

void BlitOscillator::restart()
{
    // The BLIT impulses are symmetric: a sample shortly after an impulse also needs the part of the impulse
    // BEFORE it. So the integrators start kRunInSamples before the current phase with the ideal values
    // and run in up to the current phase (the error of the missing earlier part falls like 1/(pi n)).
    const double dt = m_phaseIncrement;
    const double startPhase = m_phase;
    m_phase = wrapPhase(startPhase - kRunInSamples * dt);
    const double previousPhase = m_phase - dt;
    m_sawtooth = naiveWaveform(Waveform::Sawtooth, previousPhase);
    m_rectangle = naiveWaveform(Waveform::Rectangle, previousPhase);
    m_triangleRectangle = naiveWaveform(Waveform::Rectangle, previousPhase + kQuarterPeriod);
    m_triangle = naiveWaveform(Waveform::Triangle, previousPhase);
    if (dt > 0.0)
    {
        for (int n = 0; n < kRunInSamples; ++n)
        {
            updateIntegrators();
            advancePhase();
        }
    }
    m_phase = startPhase;
}

double BlitOscillator::blit(double x) const
{
    const double p = wrapPhase(x);
    const double denominator = std::sin(kPi * p);
    const double numTerms = m_numTerms;
    if (std::abs(denominator) < kTinySine)
    {
        return m_phaseIncrement * numTerms; // limit at the impulse (M is odd)
    }
    return m_phaseIncrement * std::sin(kPi * numTerms * p) / denominator;
}

void BlitOscillator::updateIntegrators()
{
    const double dt = m_phaseIncrement;
    // M = 2 floor(P/2) + 1: all harmonics up to fs/2
    const double period = 1.0 / dt;
    m_numTerms = 2 * static_cast<int>(std::floor(0.5 * period)) + 1;

    // all integrators run all the time: a change of the waveform needs no new start
    // sawtooth: jump at phase 0.5
    m_sawtooth = m_leak * m_sawtooth + kJump * (dt - blit(m_phase - kHalfPeriod));
    // rectangle: up at phase 0, down at phase 0.5
    m_rectangle = m_leak * m_rectangle + kJump * (blit(m_phase) - blit(m_phase - kHalfPeriod));
    // triangle: rectangle +1 from phase 0.75 to 0.25, integrated with the slope 4 per period
    m_triangleRectangle = m_leak * m_triangleRectangle
                        + kJump * (blit(m_phase - kThreeQuarterPeriod) - blit(m_phase - kQuarterPeriod));
    m_triangle = m_leak * m_triangle + kTriangleSlope * dt * m_triangleRectangle;
}

float BlitOscillator::processSample()
{
    double out = naiveWaveform(m_waveform, m_phase); // sine (and no frequency yet)
    if (m_phaseIncrement > 0.0)
    {
        updateIntegrators();
        switch (m_waveform)
        {
            case Waveform::Sine:
                break;
            case Waveform::Sawtooth:
                out = m_sawtooth;
                break;
            case Waveform::Rectangle:
                out = m_rectangle;
                break;
            case Waveform::Triangle:
                out = m_triangle;
                break;
        }
    }
    advancePhase();
    return static_cast<float>(out);
}
} // namespace vss
