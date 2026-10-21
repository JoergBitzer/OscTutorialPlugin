/*
    ScopeCapture.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: records the oscillator output for the oscilloscope. A recording starts when a new
    period of the oscillator begins (phase wraps from 1 to 0, the trigger) and lasts 1 or 2 periods.
    For every sample it also stores its position in periods (x = 0..cycles), so the display can
    draw the samples exactly where they are, also if the period is not a whole number of samples.
    Threads: push() runs in the audio thread and never waits (it drops a finished recording if the
    GUI is just copying); copyLatest() runs in the GUI thread. No GUI code here.
    License: MIT
*/
#pragma once
#include <atomic>
#include <vector>
#include <juce_core/juce_core.h>

class ScopeCapture
{
public:
    struct Frame
    {
        std::vector<float> x; // position in periods
        std::vector<float> y; // value
        int numPoints = 0;
        int cycles = 0;
        double samplesPerCycle = 0.0;
    };

    ScopeCapture();
    void setCycles(int cycles); // 1 or 2, from the GUI
    // audio thread: a new note starts, wait for the next trigger (a running recording is dropped)
    void restart();
    // audio thread: one sample, the phase it was computed at and the phase increment
    void push(float value, double phase, double phaseIncrement);
    // GUI thread: copies the newest recording, returns false if there is none since the last call
    bool copyLatest(Frame& frame);

    static constexpr int kMaxPoints = 1 << 16; // 2 periods at 1.5 Hz (48 kHz)

private:
    void publish(double phaseIncrement);

    std::atomic<int> m_cycles { 2 };
    // recording (audio thread only)
    std::vector<float> m_recordX;
    std::vector<float> m_recordY;
    int m_recordCount = 0;
    bool m_isRecording = false;
    int m_recordCycles = 2;
    double m_position = 0.0;
    double m_lastPhase = 1.0;
    // the newest finished recording, protected by m_lock
    juce::SpinLock m_lock;
    Frame m_published;
    bool m_hasNewFrame = false;
};
