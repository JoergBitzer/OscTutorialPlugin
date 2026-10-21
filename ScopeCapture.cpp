#include "ScopeCapture.h"

ScopeCapture::ScopeCapture()
{
    // all memory here, none in the audio thread
    m_recordX.assign(static_cast<size_t>(kMaxPoints), 0.0f);
    m_recordY.assign(static_cast<size_t>(kMaxPoints), 0.0f);
    m_published.x.assign(static_cast<size_t>(kMaxPoints), 0.0f);
    m_published.y.assign(static_cast<size_t>(kMaxPoints), 0.0f);
}

void ScopeCapture::setCycles(int cycles)
{
    m_cycles.store(cycles);
}

void ScopeCapture::restart()
{
    m_isRecording = false;
    m_lastPhase = 1.0;
}

void ScopeCapture::push(float value, double phase, double phaseIncrement)
{
    const bool newPeriodStarts = phase < m_lastPhase;
    m_lastPhase = phase;
    if (!m_isRecording)
    {
        if (!newPeriodStarts)
        {
            return;
        }
        // trigger: start at the first sample of the new period
        m_isRecording = true;
        m_recordCount = 0;
        m_recordCycles = m_cycles.load();
        m_position = phase;
    }
    const size_t index = static_cast<size_t>(m_recordCount);
    m_recordX[index] = static_cast<float>(m_position);
    m_recordY[index] = value;
    ++m_recordCount;
    const bool isComplete = m_position >= m_recordCycles;
    m_position += phaseIncrement;
    if (isComplete || m_recordCount >= kMaxPoints)
    {
        publish(phaseIncrement);
        m_isRecording = false;
    }
}

void ScopeCapture::publish(double phaseIncrement)
{
    const juce::SpinLock::ScopedTryLockType lock(m_lock);
    if (!lock.isLocked())
    {
        return; // the GUI is copying: drop this recording, the next one comes soon
    }
    std::copy(m_recordX.begin(), m_recordX.begin() + m_recordCount, m_published.x.begin());
    std::copy(m_recordY.begin(), m_recordY.begin() + m_recordCount, m_published.y.begin());
    m_published.numPoints = m_recordCount;
    m_published.cycles = m_recordCycles;
    m_published.samplesPerCycle = 0.0;
    if (phaseIncrement > 0.0)
    {
        m_published.samplesPerCycle = 1.0 / phaseIncrement;
    }
    m_hasNewFrame = true;
}

bool ScopeCapture::copyLatest(Frame& frame)
{
    const juce::SpinLock::ScopedLockType lock(m_lock);
    if (!m_hasNewFrame)
    {
        return false;
    }
    const auto count = static_cast<size_t>(m_published.numPoints);
    frame.x.assign(m_published.x.begin(), m_published.x.begin() + static_cast<std::ptrdiff_t>(count));
    frame.y.assign(m_published.y.begin(), m_published.y.begin() + static_cast<std::ptrdiff_t>(count));
    frame.numPoints = m_published.numPoints;
    frame.cycles = m_published.cycles;
    frame.samplesPerCycle = m_published.samplesPerCycle;
    m_hasNewFrame = false;
    return true;
}
