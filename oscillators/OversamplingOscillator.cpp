#include <cmath>
#include "OversamplingOscillator.h"

namespace vss
{
OversamplingOscillator::OversamplingOscillator()
{
    designFilter();
    m_history.assign(static_cast<size_t>(2 * kNumTaps), 0.0);
}

void OversamplingOscillator::designFilter()
{
    // windowed sinc (Blackman), cut-off kCutoff * fs = kCutoff / kFactor at the high rate, DC gain 1
    const double centre = 0.5 * (kNumTaps - 1);
    const double cutoff = kCutoff / kFactor; // in units of the high sample rate
    m_filter.assign(static_cast<size_t>(kNumTaps), 0.0);
    double sum = 0.0;
    for (int i = 0; i < kNumTaps; ++i)
    {
        const double t = i - centre;
        const double sinc = std::sin(kTwoPi * cutoff * t) / (kPi * t); // t is never 0 (even number of taps)
        const double window = blackmanWindow(i, kNumTaps);
        m_filter[static_cast<size_t>(i)] = sinc * window;
        sum += sinc * window;
    }
    for (auto& coefficient : m_filter)
    {
        coefficient /= sum;
    }
}

double OversamplingOscillator::phaseLead() const
{
    const double delayInHighRateSamples = 0.5 * (kNumTaps - 1);
    return delayInHighRateSamples * m_phaseIncrement / kFactor;
}

void OversamplingOscillator::push(double value)
{
    // newest sample at the lowest index: move the write position backwards
    m_writeIndex = (m_writeIndex + kNumTaps - 1) % kNumTaps;
    m_history[static_cast<size_t>(m_writeIndex)] = value;
    m_history[static_cast<size_t>(m_writeIndex + kNumTaps)] = value;
}

void OversamplingOscillator::restart()
{
    // fill the filter with the waveform as it was before the current phase (no transient)
    const double highRateIncrement = m_phaseIncrement / kFactor;
    for (int i = kNumTaps - 1; i >= 0; --i)
    {
        push(naiveWaveform(m_waveform, m_phase + phaseLead() - i * highRateIncrement));
    }
}

float OversamplingOscillator::processSample()
{
    // kFactor new high-rate samples, the last one at the current phase + lead
    const double highRateIncrement = m_phaseIncrement / kFactor;
    const double lead = phaseLead();
    for (int j = kFactor - 1; j >= 0; --j)
    {
        push(naiveWaveform(m_waveform, m_phase + lead - j * highRateIncrement));
    }
    // FIR filter, evaluated only for the samples we keep (decimation)
    double out = 0.0;
    const double* newest = m_history.data() + m_writeIndex;
    for (int i = 0; i < kNumTaps; ++i)
    {
        out += m_filter[static_cast<size_t>(i)] * newest[i];
    }
    advancePhase();
    return static_cast<float>(out);
}
} // namespace vss
