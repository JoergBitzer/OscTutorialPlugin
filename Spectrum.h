/*
    Spectrum.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: spectrum of the oscillator output (before the envelope), for teaching aliasing.
    FFT with 8192 points and a Blackman-Harris window (side lobes -92 dB), recomputed 30 times per second.
    The frequency axis is LINEAR (0 .. fs/2): the harmonics of f0 are equally spaced, and everything
    between them is aliasing (optional marks at the harmonics). 0 dB = a sine with amplitude 1.
    The number "outside the harmonics" is the energy that is not at a harmonic (or DC), relative to
    the total energy -- the same measurement as tools/analysis/measure_models.py.
    License: MIT
*/
#pragma once
#include <vector>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "SpectrumCapture.h"

class Spectrum : public juce::Component, private juce::Timer
{
public:
    explicit Spectrum(SpectrumCapture& capture);
    ~Spectrum() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void computeSpectrum();
    juce::Rectangle<float> plotArea() const;
    float scaleFactor() const;
    float frequencyToX(double frequency, juce::Rectangle<float> area) const;
    float levelToY(float levelDb, juce::Rectangle<float> area) const;
    void paintGrid(juce::Graphics& g, juce::Rectangle<float> area);
    void paintHarmonics(juce::Graphics& g, juce::Rectangle<float> area);
    void paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void paintInfo(juce::Graphics& g);

    static constexpr int kFftOrder = 13;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kRefreshRateHz = 30;
    static constexpr float kMinLevelDb = -120.0f;
    static constexpr float kMaxLevelDb = 0.0f;
    static constexpr int kHarmonicHalfWidthBins = 5; // main lobe of the Blackman-Harris window: +-4 bins

    SpectrumCapture& m_capture;
    juce::dsp::FFT m_fft { kFftOrder };
    juce::dsp::WindowingFunction<float> m_window { static_cast<size_t>(kFftSize),
                                                   juce::dsp::WindowingFunction<float>::blackmanHarris, false };
    float m_windowGain = 1.0f;     // sum of the window / 2: scales a sine of amplitude 1 to 0 dB
    std::vector<float> m_history;  // the newest samples (up to kFftSize)
    std::vector<float> m_fftData;  // 2 * kFftSize (JUCE's frequency-only transform)
    std::vector<float> m_levelsDb; // kFftSize / 2 + 1 bins
    int m_newSamples = 0;
    bool m_hasSpectrum = false;
    double m_sampleRate = 48000.0;
    double m_fundamental = 0.0;
    float m_outsideHarmonicsDb = 0.0f;
    bool m_outsideHarmonicsValid = false;

    juce::ToggleButton m_showHarmonics { "Harmonic marks" };
};
