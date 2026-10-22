/*
    SimpleEnvelope.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: a very simple linear ADSR envelope (straight lines), for teaching.
      Attack:  from the current level up to 1 (a new note starts from where the old one was: no click)
      Decay:   from 1 down to the sustain level
      Sustain: holds the sustain level while the key is down
      Release: from the current level down to 0 in the release time
    License: MIT
*/
#pragma once

namespace otp
{
class SimpleEnvelope
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void prepare(double sampleRate);
    // times in ms, sustain level 0..1; may be called at any time
    void setParameters(float attackMs, float decayMs, float sustainLevel, float releaseMs);
    void noteOn();
    void noteOff();
    void reset();

    float processSample();
    bool isActive() const;
    Stage getStage() const;

private:
    float samplesFromMs(float timeMs) const;

    static constexpr float kMinTimeMs = 0.1f; // avoids a division by zero

    double m_sampleRate = 48000.0;
    Stage m_stage = Stage::Idle;
    float m_level = 0.0f;
    float m_attackMs = 10.0f;
    float m_decayMs = 300.0f;
    float m_sustainLevel = 0.7f;
    float m_releaseMs = 300.0f;
    float m_releaseStep = 0.0f; // computed at noteOff (the release starts at the current level)
};
} // namespace otp
