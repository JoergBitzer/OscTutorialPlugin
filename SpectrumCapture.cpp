#include "SpectrumCapture.h"

SpectrumCapture::SpectrumCapture()
{
    m_buffer.assign(static_cast<size_t>(kCapacity), 0.0f); // all memory here, none in the audio thread
}

void SpectrumCapture::setSampleRate(double sampleRate)
{
    m_sampleRate.store(sampleRate);
}

void SpectrumCapture::push(float value, double phaseIncrement)
{
    m_fundamentalFrequency.store(phaseIncrement * m_sampleRate.load());
    const auto scope = m_fifo.write(1);
    if (scope.blockSize1 > 0)
    {
        m_buffer[static_cast<size_t>(scope.startIndex1)] = value;
    }
    else if (scope.blockSize2 > 0)
    {
        m_buffer[static_cast<size_t>(scope.startIndex2)] = value;
    }
    // FIFO full (nobody reads): the sample is dropped
}

int SpectrumCapture::pull(std::vector<float>& destination)
{
    const int available = m_fifo.getNumReady();
    const auto scope = m_fifo.read(available);
    for (int i = 0; i < scope.blockSize1; ++i)
    {
        destination.push_back(m_buffer[static_cast<size_t>(scope.startIndex1 + i)]);
    }
    for (int i = 0; i < scope.blockSize2; ++i)
    {
        destination.push_back(m_buffer[static_cast<size_t>(scope.startIndex2 + i)]);
    }
    return available;
}

double SpectrumCapture::getSampleRate() const
{
    return m_sampleRate.load();
}

double SpectrumCapture::getFundamentalFrequency() const
{
    return m_fundamentalFrequency.load();
}
