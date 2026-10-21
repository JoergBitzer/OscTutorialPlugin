#include <cmath>
#include "Oscillators.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kHalfPeriod = 0.5;
constexpr double kQuarterPeriod = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
// the triangle rises by 4 per period (from -1 to +1 in half a period); at a corner its slope changes by 8 per period
constexpr double kTriangleSlope = 4.0;
constexpr double kTriangleSlopeChange = 2.0 * kTriangleSlope;

// wraps a phase into 0..1
double wrapPhase(double phase)
{
    return phase - std::floor(phase);
}
} // namespace

namespace vss
{
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
    m_waveform = waveform;
}

void Oscillator::reset()
{
    m_phase = 0.0;
}

void Oscillator::setPhase(double phase)
{
    m_phase = wrapPhase(phase);
}

double Oscillator::getPhase() const
{
    return m_phase;
}

void Oscillator::advancePhase()
{
    m_phase += m_phaseIncrement;
    if (m_phase >= 1.0)
    {
        m_phase -= 1.0;
    }
}

float Oscillator::naiveSample() const
{
    double value = 0.0;
    switch (m_waveform)
    {
        case Waveform::Sine:
            value = std::sin(kTwoPi * m_phase);
            break;
        case Waveform::Triangle:
            // three straight lines: up from 0 to +1, down to -1, up to 0
            if (m_phase < kQuarterPeriod)
            {
                value = kTriangleSlope * m_phase;
            }
            else if (m_phase < kThreeQuarterPeriod)
            {
                value = 2.0 - kTriangleSlope * m_phase;
            }
            else
            {
                value = kTriangleSlope * m_phase - kTriangleSlope;
            }
            break;
        case Waveform::Rectangle:
            if (m_phase < kHalfPeriod)
            {
                value = 1.0;
            }
            else
            {
                value = -1.0;
            }
            break;
        case Waveform::Sawtooth:
        {
            // shifted by half a period: 0 at phase 0, jump from +1 to -1 at phase 0.5
            const double shiftedPhase = wrapPhase(m_phase + kHalfPeriod);
            value = 2.0 * shiftedPhase - 1.0;
            break;
        }
    }
    return static_cast<float>(value);
}

// ------------------------------------------------------------------ Naive
float NaiveOscillator::processSample()
{
    const float out = naiveSample();
    advancePhase();
    return out;
}

// ------------------------------------------------------------------ Wavetable
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
        const double amplitude = harmonicAmplitude(waveform, k);
        constexpr double kMinAmplitude = 1e-12; // the missing harmonics (e.g. the even ones of a rectangle) have 0
        if (std::abs(amplitude) < kMinAmplitude)
        {
            continue;
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

// ------------------------------------------------------------------ PolyBLEP / PolyBLAMP
double PolyBlepOscillator::polyBlep(double t, double dt)
{
    if (dt <= 0.0)
    {
        return 0.0;
    }
    if (t < dt)
    {
        // the first sample after the jump
        const double x = t / dt;
        return x + x - x * x - 1.0;
    }
    if (t > 1.0 - dt)
    {
        // the last sample before the jump
        const double x = (t - 1.0) / dt;
        return x * x + x + x + 1.0;
    }
    return 0.0;
}

double PolyBlepOscillator::polyBlamp(double t, double dt)
{
    constexpr double kOneSixth = 1.0 / 6.0;
    if (dt <= 0.0)
    {
        return 0.0;
    }
    double distance = 1.0; // distance to the corner in samples
    if (t < dt)
    {
        distance = t / dt;
    }
    else if (t > 1.0 - dt)
    {
        distance = (1.0 - t) / dt;
    }
    const double x = 1.0 - distance;
    return kOneSixth * x * x * x;
}

float PolyBlepOscillator::processSample()
{
    double value = naiveSample();
    const double dt = m_phaseIncrement;
    switch (m_waveform)
    {
        case Waveform::Sine:
            break; // no discontinuity, nothing to correct
        case Waveform::Triangle:
            // corner at phase 0.25 (slope +4 -> -4 per period) and at 0.75 (-4 -> +4)
            value -= kTriangleSlopeChange * dt * polyBlamp(wrapPhase(m_phase - kQuarterPeriod), dt);
            value += kTriangleSlopeChange * dt * polyBlamp(wrapPhase(m_phase - kThreeQuarterPeriod), dt);
            break;
        case Waveform::Rectangle:
            // jump -1 -> +1 at phase 0, +1 -> -1 at phase 0.5
            value += polyBlep(m_phase, dt);
            value -= polyBlep(wrapPhase(m_phase - kHalfPeriod), dt);
            break;
        case Waveform::Sawtooth:
            // jump +1 -> -1 at phase 0.5
            value -= polyBlep(wrapPhase(m_phase - kHalfPeriod), dt);
            break;
    }
    advancePhase();
    return static_cast<float>(value);
}
} // namespace vss
