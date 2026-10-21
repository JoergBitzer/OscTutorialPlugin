#include <algorithm>
#include <cmath>
#include <complex>
#include "MinBlepOscillator.h"

namespace
{
using Complex = std::complex<double>;

constexpr int kFftSize = 16384;       // >> table length, so that the cepstrum does not alias in time
constexpr double kMinMagnitude = 1e-10; // -200 dB: avoids log(0)

// in-place radix-2 FFT (size: power of 2); inverse: without the 1/N scaling
void fft(std::vector<Complex>& data, bool inverse)
{
    const size_t n = data.size();
    // bit-reversed order
    size_t j = 0;
    for (size_t i = 1; i < n; ++i)
    {
        size_t bit = n >> 1;
        while ((j & bit) != 0)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j)
        {
            std::swap(data[i], data[j]);
        }
    }
    // butterflies
    double sign = -1.0;
    if (inverse)
    {
        sign = 1.0;
    }
    for (size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = sign * vss::kTwoPi / static_cast<double>(length);
        const Complex step(std::cos(angle), std::sin(angle));
        for (size_t start = 0; start < n; start += length)
        {
            Complex w(1.0, 0.0);
            for (size_t k = 0; k < length / 2; ++k)
            {
                const Complex even = data[start + k];
                const Complex odd = data[start + k + length / 2] * w;
                data[start + k] = even + odd;
                data[start + k + length / 2] = even - odd;
                w *= step;
            }
        }
    }
}

void inverseFft(std::vector<Complex>& data)
{
    fft(data, true);
    const double scale = 1.0 / static_cast<double>(data.size());
    for (auto& value : data)
    {
        value *= scale;
    }
}
} // namespace

namespace vss
{
MinBlepOscillator::MinBlepOscillator()
{
    buildTables();
    m_corrections.assign(static_cast<size_t>(kTableLengthSamples + 1), 0.0);
}

void MinBlepOscillator::buildTables()
{
    constexpr int kCentre = kZeroCrossings * kOversampling;
    constexpr int kLength = 2 * kCentre + 1;

    // 1. band-limited impulse: sinc with a Blackman window. The cut-off is below fs/2, because the window
    //    widens the transition band: with the cut-off at fs/2 half of it would lie above fs/2 and alias
    //    (measured: the 12th harmonic of a sawtooth at 2093 Hz came back at -35 dB)
    std::vector<Complex> spectrum(static_cast<size_t>(kFftSize), Complex(0.0, 0.0));
    for (int i = 0; i < kLength; ++i)
    {
        const double t = static_cast<double>(i - kCentre) / kOversampling; // in samples
        double sinc = kCutoff;
        if (i != kCentre)
        {
            sinc = std::sin(kPi * kCutoff * t) / (kPi * t);
        }
        const double window = blackmanWindow(i, kLength);
        spectrum[static_cast<size_t>(i)] = Complex(sinc * window, 0.0);
    }

    // 2. minimum phase with the real cepstrum: log|X| -> cepstrum -> keep only the causal part -> exp
    fft(spectrum, false);
    std::vector<Complex> cepstrum(static_cast<size_t>(kFftSize));
    for (size_t k = 0; k < cepstrum.size(); ++k)
    {
        cepstrum[k] = Complex(std::log(std::max(std::abs(spectrum[k]), kMinMagnitude)), 0.0);
    }
    inverseFft(cepstrum);
    const size_t half = static_cast<size_t>(kFftSize / 2);
    for (size_t k = 1; k < half; ++k)
    {
        cepstrum[k] *= 2.0;
    }
    for (size_t k = half + 1; k < cepstrum.size(); ++k)
    {
        cepstrum[k] = Complex(0.0, 0.0);
    }
    fft(cepstrum, false);
    for (auto& value : cepstrum)
    {
        value = std::exp(value);
    }
    inverseFft(cepstrum); // now: the minimum-phase impulse

    // 3. integrate -> minimum-phase step (normalised to end at 1), residual = step - 1
    std::vector<double> step(static_cast<size_t>(kLength));
    double sum = 0.0;
    for (int i = 0; i < kLength; ++i)
    {
        sum += cepstrum[static_cast<size_t>(i)].real();
        step[static_cast<size_t>(i)] = sum;
    }
    m_blepResidual.assign(static_cast<size_t>(kLength + 1), 0.0); // + guard point (0) for the interpolation
    for (int i = 0; i < kLength; ++i)
    {
        m_blepResidual[static_cast<size_t>(i)] = step[static_cast<size_t>(i)] / sum - 1.0;
    }

    // 4. integrate again (in samples) -> ramp residual; its final value is the delay
    std::vector<double> ramp(static_cast<size_t>(kLength));
    double rampSum = 0.0;
    for (int i = 0; i < kLength; ++i)
    {
        ramp[static_cast<size_t>(i)] = rampSum;
        rampSum += m_blepResidual[static_cast<size_t>(i)] / kOversampling;
    }
    m_delayInSamples = rampSum;
    m_blampTransient.assign(static_cast<size_t>(kLength + 1), 0.0);
    for (int i = 0; i < kLength; ++i)
    {
        m_blampTransient[static_cast<size_t>(i)] = ramp[static_cast<size_t>(i)] - m_delayInSamples;
    }
}

double MinBlepOscillator::readTable(const std::vector<double>& table, double timeInSamples)
{
    const double position = timeInSamples * kOversampling;
    const int index = static_cast<int>(position);
    if (index < 0 || index + 1 >= static_cast<int>(table.size()))
    {
        return 0.0;
    }
    const double fraction = position - index;
    const double first = table[static_cast<size_t>(index)];
    const double second = table[static_cast<size_t>(index + 1)];
    return first + fraction * (second - first);
}

void MinBlepOscillator::restart()
{
    std::fill(m_corrections.begin(), m_corrections.end(), 0.0);
}

void MinBlepOscillator::addCorrection(double jump, double slopeChangePerSample, double timeSinceEvent)
{
    const int length = static_cast<int>(m_corrections.size());
    for (int k = 0; k < length; ++k)
    {
        const double time = timeSinceEvent + k;
        const size_t index = static_cast<size_t>((m_readIndex + k) % length);
        m_corrections[index] += jump * readTable(m_blepResidual, time)
                              + slopeChangePerSample * readTable(m_blampTransient, time);
    }
}

float MinBlepOscillator::processSample()
{
    double value = naiveWaveform(m_waveform, m_phase);
    const double dt = m_phaseIncrement;
    if (dt > 0.0 && m_waveform != Waveform::Sine)
    {
        // discontinuities between the last sample and this one
        Discontinuity discontinuities[kMaxDiscontinuities];
        const int count = getDiscontinuities(m_waveform, discontinuities);
        for (int i = 0; i < count; ++i)
        {
            const double phaseSinceEvent = wrapPhase(m_phase - discontinuities[i].phase);
            if (phaseSinceEvent < dt)
            {
                addCorrection(discontinuities[i].jump, discontinuities[i].slopeChange * dt, phaseSinceEvent / dt);
            }
        }
        // the straight parts are delayed like the corners (see the header)
        value += m_delayInSamples * naiveSlope(m_waveform, m_phase) * dt;
    }
    const size_t readIndex = static_cast<size_t>(m_readIndex);
    value += m_corrections[readIndex];
    m_corrections[readIndex] = 0.0;
    m_readIndex = (m_readIndex + 1) % static_cast<int>(m_corrections.size());
    advancePhase();
    return static_cast<float>(value);
}
} // namespace vss
