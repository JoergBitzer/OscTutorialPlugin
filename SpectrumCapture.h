/*
    SpectrumCapture.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: passes the oscillator output (before the envelope) from the audio thread to the
    spectrum display. A single-producer/single-consumer FIFO (juce::AbstractFifo): the audio thread
    writes, the GUI thread reads, nobody waits. If the GUI does not read (window closed), the FIFO
    runs full and new samples are dropped. Also passes the current fundamental frequency, for the
    marks at the harmonics. No GUI code here.
    License: MIT
*/
#pragma once
#include <atomic>
#include <vector>
#include <juce_core/juce_core.h>

class SpectrumCapture
{
public:
    SpectrumCapture();
    void setSampleRate(double sampleRate);
    // audio thread: one sample and the phase increment of the oscillator (f0 = increment * fs)
    void push(float value, double phaseIncrement);
    // GUI thread: appends all samples that arrived since the last call to "destination", returns how many
    int pull(std::vector<float>& destination);
    double getSampleRate() const;
    double getFundamentalFrequency() const;

    static constexpr int kCapacity = 1 << 15;

private:
    juce::AbstractFifo m_fifo { kCapacity };
    std::vector<float> m_buffer;
    std::atomic<double> m_sampleRate { 48000.0 };
    std::atomic<double> m_fundamentalFrequency { 0.0 };
};
