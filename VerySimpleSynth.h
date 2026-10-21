#pragma once

#include <atomic>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "tools/SynchronBlockProcessor.h"
#include "tools/ParameterSpec.h"
#include "tools/ChoiceParameterSpec.h"
#include "PluginSettings.h"
#include "oscillators/Oscillator.h"
#include "oscillators/WavetableOscillator.h"
#include "oscillators/PolyBlepOscillator.h"
#include "oscillators/MinBlepOscillator.h"
#include "oscillators/BlitOscillator.h"
#include "oscillators/DpwOscillator.h"
#include "oscillators/OversamplingOscillator.h"
#include "SimpleEnvelope.h"
#include "ScopeCapture.h"
#include "Oscilloscope.h"

class VerySimpleSynthAudioProcessor;

// This is how we define our parameter as globals to use it in the audio processor as well as in the editor.
// Everything about a parameter is defined here once; tools/ParameterSpec.h makes the parameter
// (jade::makeParameter) and its help text (jade::helpText, e.g. as tooltip) from it.
// Choice parameters (combo boxes): tools/ChoiceParameterSpec.h. The order of the choices must
// match the enums vss::OscillatorModel and vss::Waveform in oscillators/Oscillator.h.

// ---------- oscillator ----------
const struct
{
	const std::string ID = "Model";
	const std::string name = "Model";
	const std::vector<std::string> choices = { "Naive", "Wavetable", "PolyBLEP", "PolyBLEP 4-point", "minBLEP",
	                                           "BLIT", "DPW", "Oversampling 8x" };
	const int defaultIndex = 0;
	const std::string help = "How the oscillator makes the waveform: Naive (sampled ideal waveform, aliasing), "
	                         "Wavetable (band-limited tables, one per octave), PolyBLEP and PolyBLEP 4-point (naive "
	                         "waveform with polynomial corrections at jumps and corners), minBLEP (minimum-phase "
	                         "corrections from a table), BLIT (integrated band-limited impulse trains), DPW "
	                         "(differentiated antiderivative), Oversampling 8x (naive at 8 fs, filtered).";
}g_paramModel;

const struct
{
	const std::string ID = "LanczosSigma";
	const std::string name = "Lanczos sigma";
	const bool defaultIsOn = false;
	const std::string help = "Wavetable only: weights the harmonics with Lanczos sigma factors, which reduces "
	                         "the overshoot at jumps (Gibbs phenomenon) from 9 % to about 1 %.";
}g_paramLanczosSigma;

const struct
{
	const std::string ID = "Waveform";
	const std::string name = "Waveform";
	const std::vector<std::string> choices = { "Sine", "Triangle", "Rectangle", "Sawtooth" };
	const int defaultIndex = 3;
	const std::string help = "The waveform of the oscillator (Rectangle: 50 % duty cycle).";
}g_paramWaveform;

const struct
{
	const std::string ID = "Octave";
	const std::string name = "Octave";
	const std::string unitName = "";
	const float minValue = -3.f;
	const float maxValue = 3.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Shifts the pitch by whole octaves.";
}g_paramOctave;

const struct
{
	const std::string ID = "FineTune";
	const std::string name = "Fine tune";
	const std::string unitName = "cent";
	const float minValue = -100.f;
	const float maxValue = 100.f;
	const float defaultValue = 0.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Fine tuning in cent (100 cent = 1 semitone).";
}g_paramFineTune;

// ---------- envelope (ADSR) ----------
// logFrequency = true gives a logarithmic range in whole units, here whole ms
const struct
{
	const std::string ID = "Attack";
	const std::string name = "Attack";
	const std::string unitName = "ms";
	const float minValue = 1.f;
	const float maxValue = 5000.f;
	const float defaultValue = 10.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = true;
	const std::string help = "Time to rise to full level after a key is pressed.";
}g_paramAttack;

const struct
{
	const std::string ID = "Decay";
	const std::string name = "Decay";
	const std::string unitName = "ms";
	const float minValue = 1.f;
	const float maxValue = 5000.f;
	const float defaultValue = 300.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = true;
	const std::string help = "Time to fall from full level to the sustain level.";
}g_paramDecay;

const struct
{
	const std::string ID = "Sustain";
	const std::string name = "Sustain";
	const std::string unitName = "%";
	const float minValue = 0.f;
	const float maxValue = 100.f;
	const float defaultValue = 70.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = false;
	const std::string help = "Level while the key is held (in % of full level).";
}g_paramSustain;

const struct
{
	const std::string ID = "Release";
	const std::string name = "Release";
	const std::string unitName = "ms";
	const float minValue = 1.f;
	const float maxValue = 5000.f;
	const float defaultValue = 300.f;
	const int numDecimalPlaces = 0;
	const bool logFrequency = true;
	const std::string help = "Time to fall to silence after the key is released.";
}g_paramRelease;

// ---------- output ----------
const struct
{
	const std::string ID = "Level";
	const std::string name = "Level";
	const std::string unitName = "dB";
	const float minValue = -60.f;
	const float maxValue = 0.f;
	const float defaultValue = -12.f;
	const int numDecimalPlaces = 1;
	const bool logFrequency = false;
	const std::string help = "Output level (at velocity 127).";
}g_paramLevel;


class VerySimpleSynthAudio : public SynchronBlockProcessor
{
public:
    VerySimpleSynthAudio(VerySimpleSynthAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    virtual int processSynchronBlock(juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock);

    // parameter handling
  	void addParameter(std::vector < std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>&  vts);

    // some necessary info for the host
    int getLatency(){return m_Latency;};
    // the recordings of the oscillator output for the oscilloscope (GUI thread: copyLatest, setCycles)
    ScopeCapture& getScope(){return m_scope;};

private:
    void updateParameters();
    void handleMidiMessage(const juce::MidiMessage& message);
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();
    void updateFrequency();
    // renders the samples [startSample, endSample) into channel 0
    void render(juce::AudioBuffer<float>& buffer, int startSample, int endSample);

    static constexpr int kNoNote = -1;
    static constexpr int kNumMidiNotes = 128;
    static constexpr double kPitchBendRangeSemitones = 2.0;
    static constexpr double kLevelRampSeconds = 0.02;

	VerySimpleSynthAudioProcessor* m_processor;
    int m_Latency = 0;

    // parameters (read once per synchronous block)
    std::atomic<float>* m_model = nullptr;
    std::atomic<float>* m_waveform = nullptr;
    std::atomic<float>* m_lanczosSigma = nullptr;
    std::atomic<float>* m_octave = nullptr;
    std::atomic<float>* m_fineTune = nullptr;
    std::atomic<float>* m_attack = nullptr;
    std::atomic<float>* m_decay = nullptr;
    std::atomic<float>* m_sustain = nullptr;
    std::atomic<float>* m_release = nullptr;
    std::atomic<float>* m_level = nullptr;

    // all oscillator models; m_activeOscillator points to the chosen one
    vss::NaiveOscillator m_naiveOscillator;
    vss::WavetableOscillator m_wavetableOscillator;
    vss::PolyBlepOscillator m_polyBlepOscillator { vss::PolyBlepOscillator::Order::TwoPoint };
    vss::PolyBlepOscillator m_polyBlep4Oscillator { vss::PolyBlepOscillator::Order::FourPoint };
    vss::MinBlepOscillator m_minBlepOscillator;
    vss::BlitOscillator m_blitOscillator;
    vss::DpwOscillator m_dpwOscillator;
    vss::OversamplingOscillator m_oversamplingOscillator;
    // in the order of vss::OscillatorModel
    std::vector<vss::Oscillator*> m_oscillators;
    vss::Oscillator* m_activeOscillator = &m_naiveOscillator;
    vss::OscillatorModel m_activeModel = vss::OscillatorModel::Naive;

    ScopeCapture m_scope;

    vss::SimpleEnvelope m_envelope;

    // monophonic voice: the held keys in the order they were pressed, the last one sounds
    std::vector<int> m_heldNotes;
    int m_currentNote = kNoNote;
    juce::SmoothedValue<float> m_velocityGain;
    double m_pitchBendSemitones = 0.0;
    double m_octaveShift = 0.0;
    double m_fineTuneCent = 0.0;
    juce::SmoothedValue<float> m_levelGain;
};

class VerySimpleSynthGUI : public juce::Component
{
public:
	VerySimpleSynthGUI(VerySimpleSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

	void paint(juce::Graphics& g) override;
	void resized() override;
private:
    // a knob with its name above; value and unit come from the parameter (via the attachment)
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    // a combo box with its name above
    struct Choice
    {
        juce::ComboBox comboBox;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    template <class Spec>
    void setupKnob(Knob& knob, const Spec& spec);
    template <class Spec>
    void setupChoice(Choice& choice, const Spec& spec);
    template <class Spec>
    void setupSwitch(Switch& control, const Spec& spec);
    void placeKnob(Knob& knob, juce::Rectangle<int> area);
    // label in the first row, combo box in the second
    void placeChoice(Choice& choice, juce::Rectangle<int>& area);
    // Lanczos sigma only acts on the wavetable model
    void updateLanczosSigmaEnabled();
    float scaleFactor() const;

	VerySimpleSynthAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;

    juce::GroupComponent m_oscillatorGroup;
    juce::GroupComponent m_envelopeGroup;
    juce::GroupComponent m_outputGroup;

    Choice m_model;
    Choice m_waveform;
    Switch m_lanczosSigma;
    Knob m_octave;
    Knob m_fineTune;
    Knob m_attack;
    Knob m_decay;
    Knob m_sustain;
    Knob m_release;
    Knob m_level;

    Oscilloscope m_oscilloscope;
};
