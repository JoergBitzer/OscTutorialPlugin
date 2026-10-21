/*
    Oscilloscope.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: oscilloscope for teaching: shows 1 or 2 periods of the oscillator output (before the
    envelope, so the amplitude is always 1), triggered at the start of a period. Optional: the ideal
    waveform as a thin line (shows overshoot, ringing, smoothing and delay of the models) and the
    sample points (shows where the corrections act). The left part of the top row is left free for the
    page buttons of the editor (Oscilloscope | Spectrum). Grid lines at 0, +-0.5, +-1 and at each quarter period.
    License: MIT
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ScopeCapture.h"

class Oscilloscope : public juce::Component, private juce::Timer
{
public:
    Oscilloscope(ScopeCapture& capture, juce::AudioProcessorValueTreeState& apvts);
    ~Oscilloscope() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const;
    float scaleFactor() const;
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> area, int cycles);
    void paintIdeal(juce::Graphics& g, juce::Rectangle<float> area, int cycles);
    void paintTrace(juce::Graphics& g, juce::Rectangle<float> area);
    void paintInfo(juce::Graphics& g, juce::Rectangle<float> area);

    static constexpr int kRefreshRateHz = 30;
    static constexpr float kValueRange = 1.3f;     // the plot shows -1.3 .. +1.3
    static constexpr float kMinDotSpacing = 4.0f;  // sample points only if they are at least 4 px apart

    ScopeCapture& m_capture;
    juce::AudioProcessorValueTreeState& m_apvts;
    ScopeCapture::Frame m_frame;
    bool m_hasFrame = false;

    juce::ComboBox m_cycles;
    juce::ToggleButton m_showIdeal { "Ideal waveform" };
    juce::ToggleButton m_showSamples { "Sample points" };
};
