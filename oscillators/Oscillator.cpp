#include <cmath>
#include "Oscillator.h"

namespace
{
constexpr double kHalfPeriod = 0.5;
constexpr double kQuarterPeriod = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
// the triangle rises by 4 per period (from -1 to +1 in half a period)
constexpr double kTriangleSlope = 4.0;
// the sawtooth rises by 2 per period (from -1 to +1)
constexpr double kSawtoothSlope = 2.0;
} // namespace

namespace vss
{
double wrapPhase(double phase)
{
    return phase - std::floor(phase);
}

double naiveWaveform(Waveform waveform, double phase)
{
    const double p = wrapPhase(phase);
    switch (waveform)
    {
        case Waveform::Sine:
            return std::sin(kTwoPi * p);
        case Waveform::Triangle:
            // three straight lines: up from 0 to +1, down to -1, up to 0
            if (p < kQuarterPeriod)
            {
                return kTriangleSlope * p;
            }
            if (p < kThreeQuarterPeriod)
            {
                return 2.0 - kTriangleSlope * p;
            }
            return kTriangleSlope * p - kTriangleSlope;
        case Waveform::Rectangle:
            if (p < kHalfPeriod)
            {
                return 1.0;
            }
            return -1.0;
        case Waveform::Sawtooth:
        {
            // shifted by half a period: 0 at phase 0, jump from +1 to -1 at phase 0.5
            const double shiftedPhase = wrapPhase(p + kHalfPeriod);
            return kSawtoothSlope * shiftedPhase - 1.0;
        }
    }
    return 0.0;
}

double naiveSlope(Waveform waveform, double phase)
{
    const double p = wrapPhase(phase);
    switch (waveform)
    {
        case Waveform::Sine:
            return kTwoPi * std::cos(kTwoPi * p);
        case Waveform::Triangle:
            if (p < kQuarterPeriod || p >= kThreeQuarterPeriod)
            {
                return kTriangleSlope;
            }
            return -kTriangleSlope;
        case Waveform::Rectangle:
            return 0.0;
        case Waveform::Sawtooth:
            return kSawtoothSlope;
    }
    return 0.0;
}

double blackmanWindow(int i, int length)
{
    constexpr double kA0 = 0.42;
    constexpr double kA1 = 0.5;
    constexpr double kA2 = 0.08;
    const double x = static_cast<double>(i) / (length - 1);
    return kA0 - kA1 * std::cos(kTwoPi * x) + kA2 * std::cos(2.0 * kTwoPi * x);
}

int getDiscontinuities(Waveform waveform, Discontinuity (&list)[kMaxDiscontinuities])
{
    constexpr double kJump = 2.0; // from -1 to +1
    constexpr double kTriangleSlopeChange = 2.0 * kTriangleSlope; // from +4 to -4 per period
    switch (waveform)
    {
        case Waveform::Sine:
            return 0;
        case Waveform::Triangle:
            list[0] = { kQuarterPeriod, 0.0, -kTriangleSlopeChange };     // top corner
            list[1] = { kThreeQuarterPeriod, 0.0, kTriangleSlopeChange }; // bottom corner
            return 2;
        case Waveform::Rectangle:
            list[0] = { 0.0, kJump, 0.0 };          // -1 -> +1
            list[1] = { kHalfPeriod, -kJump, 0.0 }; // +1 -> -1
            return 2;
        case Waveform::Sawtooth:
            list[0] = { kHalfPeriod, -kJump, 0.0 }; // +1 -> -1
            return 1;
    }
    return 0;
}

// ------------------------------------------------------------------ Oscillator (phase accumulator)
void Oscillator::prepare(double sampleRate)
{
    m_sampleRate = sampleRate;
    reset();
}

void Oscillator::setFrequency(double frequencyHz)
{
    double frequency = frequencyHz;
    const double maxFrequency = kMaxFrequencyRatio * m_sampleRate;
    if (frequency > maxFrequency)
    {
        frequency = maxFrequency;
    }
    if (frequency < 0.0)
    {
        frequency = 0.0;
    }
    m_phaseIncrement = frequency / m_sampleRate;
}

void Oscillator::setWaveform(Waveform waveform)
{
    if (waveform == m_waveform)
    {
        return;
    }
    m_waveform = waveform;
    restart();
}

void Oscillator::reset()
{
    m_phase = 0.0;
    restart();
}

void Oscillator::setPhase(double phase)
{
    m_phase = wrapPhase(phase);
    restart();
}

double Oscillator::getPhase() const
{
    return m_phase;
}

double Oscillator::getPhaseIncrement() const
{
    return m_phaseIncrement;
}

void Oscillator::advancePhase()
{
    m_phase += m_phaseIncrement;
    if (m_phase >= 1.0)
    {
        m_phase -= 1.0;
    }
}

// ------------------------------------------------------------------ Naive
float NaiveOscillator::processSample()
{
    const double out = naiveWaveform(m_waveform, m_phase);
    advancePhase();
    return static_cast<float>(out);
}
} // namespace vss
