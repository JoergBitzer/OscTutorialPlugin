#include "SimpleEnvelope.h"

namespace
{
constexpr float kMsPerSecond = 1000.0f;
constexpr float kMaxLevel = 1.0f;
} // namespace

namespace otp
{
void SimpleEnvelope::prepare(double sampleRate)
{
    m_sampleRate = sampleRate;
    reset();
}

void SimpleEnvelope::setParameters(float attackMs, float decayMs, float sustainLevel, float releaseMs)
{
    m_attackMs = attackMs;
    m_decayMs = decayMs;
    m_sustainLevel = sustainLevel;
    m_releaseMs = releaseMs;
}

void SimpleEnvelope::noteOn()
{
    m_stage = Stage::Attack;
}

void SimpleEnvelope::noteOff()
{
    if (m_stage == Stage::Idle)
    {
        return;
    }
    m_stage = Stage::Release;
    m_releaseStep = m_level / samplesFromMs(m_releaseMs);
}

void SimpleEnvelope::reset()
{
    m_stage = Stage::Idle;
    m_level = 0.0f;
}

float SimpleEnvelope::processSample()
{
    switch (m_stage)
    {
        case Stage::Idle:
            break;
        case Stage::Attack:
            m_level += kMaxLevel / samplesFromMs(m_attackMs);
            if (m_level >= kMaxLevel)
            {
                m_level = kMaxLevel;
                m_stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
            m_level -= (kMaxLevel - m_sustainLevel) / samplesFromMs(m_decayMs);
            if (m_level <= m_sustainLevel)
            {
                m_level = m_sustainLevel;
                m_stage = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            // follows a change of the sustain parameter directly (a small step, see planning.md)
            m_level = m_sustainLevel;
            break;
        case Stage::Release:
            m_level -= m_releaseStep;
            if (m_level <= 0.0f)
            {
                m_level = 0.0f;
                m_stage = Stage::Idle;
            }
            break;
    }
    return m_level;
}

bool SimpleEnvelope::isActive() const
{
    return m_stage != Stage::Idle;
}

SimpleEnvelope::Stage SimpleEnvelope::getStage() const
{
    return m_stage;
}

float SimpleEnvelope::samplesFromMs(float timeMs) const
{
    float time = timeMs;
    if (time < kMinTimeMs)
    {
        time = kMinTimeMs;
    }
    return time / kMsPerSecond * static_cast<float>(m_sampleRate);
}
} // namespace otp
