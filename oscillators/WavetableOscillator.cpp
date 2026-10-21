#include <cmath>
#include "WavetableOscillator.h"

namespace vss
{
WavetableOscillator::WavetableOscillator(bool withLanczosSigma)
: m_withLanczosSigma(withLanczosSigma)
{
}

void WavetableOscillator::prepare(double sampleRate)
{
    Oscillator::prepare(sampleRate);
    constexpr double kSampleRateToleranceHz = 0.5;
    if (std::abs(sampleRate - m_tablesSampleRate) > kSampleRateToleranceHz)
    {
        buildTables();
        m_tablesSampleRate = sampleRate;
    }
}

void WavetableOscillator::setFrequency(double frequencyHz)
{
    Oscillator::setFrequency(frequencyHz);
    // the first table whose octave contains the frequency (the last one for all higher frequencies)
    const double frequency = m_phaseIncrement * m_sampleRate;
    double topFrequency = kTopFrequencyOfFirstTable;
    m_tableIndex = kNumTables - 1;
    for (int table = 0; table < kNumTables; ++table)
    {
        if (frequency <= topFrequency)
        {
            m_tableIndex = table;
            break;
        }
        topFrequency *= 2.0;
    }
}

float WavetableOscillator::processSample()
{
    const auto& table = m_tables[static_cast<size_t>(m_waveform)][static_cast<size_t>(m_tableIndex)];
    if (table.empty())
    {
        return 0.0f; // prepare() was not called yet
    }
    // linear interpolation between two table samples
    const double position = m_phase * kTableSize;
    const int index = static_cast<int>(position);
    const float fraction = static_cast<float>(position - index);
    const float first = table[static_cast<size_t>(index)];
    const float second = table[static_cast<size_t>(index + 1)];
    const float out = first + fraction * (second - first);
    advancePhase();
    return out;
}

void WavetableOscillator::buildTables()
{
    // sin(2 pi k n / N) = sineTable[(k n) mod N]: integer arithmetic instead of millions of sin() calls
    std::vector<double> sineTable(static_cast<size_t>(kTableSize));
    for (int n = 0; n < kTableSize; ++n)
    {
        sineTable[static_cast<size_t>(n)] = std::sin(kTwoPi * n / kTableSize);
    }

    const double nyquist = 0.5 * m_sampleRate;
    for (int wave = 0; wave < kNumWaveforms; ++wave)
    {
        double topFrequency = kTopFrequencyOfFirstTable;
        for (int octave = 0; octave < kNumTables; ++octave)
        {
            // all harmonics of the highest fundamental of this octave must lie below fs/2
            int numHarmonics = static_cast<int>(nyquist / topFrequency);
            if (numHarmonics > kMaxHarmonics)
            {
                numHarmonics = kMaxHarmonics;
            }
            if (numHarmonics < 1)
            {
                numHarmonics = 1;
            }
            fillTable(m_tables[static_cast<size_t>(wave)][static_cast<size_t>(octave)], static_cast<Waveform>(wave),
                      numHarmonics, sineTable);
            topFrequency *= 2.0;
        }
    }
}

void WavetableOscillator::fillTable(std::vector<float>& table, Waveform waveform, int numHarmonics,
                                    const std::vector<double>& sineTable)
{
    std::vector<double> sum(static_cast<size_t>(kTableSize), 0.0);
    for (int k = 1; k <= numHarmonics; ++k)
    {
        double amplitude = harmonicAmplitude(waveform, k);
        constexpr double kMinAmplitude = 1e-12; // the missing harmonics (e.g. the even ones of a rectangle) have 0
        if (std::abs(amplitude) < kMinAmplitude)
        {
            continue;
        }
        if (m_withLanczosSigma)
        {
            amplitude *= lanczosSigma(k, numHarmonics);
        }
        for (int n = 0; n < kTableSize; ++n)
        {
            const int sineIndex = (k * n) % kTableSize;
            sum[static_cast<size_t>(n)] += amplitude * sineTable[static_cast<size_t>(sineIndex)];
        }
    }
    table.assign(static_cast<size_t>(kTableSize + 1), 0.0f);
    for (int n = 0; n < kTableSize; ++n)
    {
        table[static_cast<size_t>(n)] = static_cast<float>(sum[static_cast<size_t>(n)]);
    }
    table[static_cast<size_t>(kTableSize)] = table[0]; // guard sample: one period further = the first sample
}

double WavetableOscillator::harmonicAmplitude(Waveform waveform, int k)
{
    const bool isOdd = (k % 2) == 1;
    switch (waveform)
    {
        case Waveform::Sine:
            if (k == 1)
            {
                return 1.0;
            }
            return 0.0;
        case Waveform::Triangle:
        {
            // 8/pi^2 * (-1)^((k-1)/2) / k^2, odd k only
            if (!isOdd)
            {
                return 0.0;
            }
            double sign = 1.0;
            if (((k - 1) / 2) % 2 == 1)
            {
                sign = -1.0;
            }
            return sign * 8.0 / (kPi * kPi * k * k);
        }
        case Waveform::Rectangle:
            // 4/pi / k, odd k only
            if (!isOdd)
            {
                return 0.0;
            }
            return 4.0 / (kPi * k);
        case Waveform::Sawtooth:
        {
            // 2/pi * (-1)^(k+1) / k
            double sign = 1.0;
            if (!isOdd)
            {
                sign = -1.0;
            }
            return sign * 2.0 / (kPi * k);
        }
    }
    return 0.0;
}

double WavetableOscillator::lanczosSigma(int k, int numHarmonics)
{
    // sinc(k / (H + 1)) = sin(pi x) / (pi x): 1 for the fundamental, falling towards 0 above the last harmonic
    const double x = kPi * k / (numHarmonics + 1);
    return std::sin(x) / x;
}
} // namespace vss
