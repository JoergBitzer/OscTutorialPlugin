#include <algorithm>
#include <math.h>
#include "VerySimpleSynth.h"

#include "PluginProcessor.h"

namespace
{
constexpr double kReferenceFrequency = 440.0; // A4
constexpr int kReferenceNote = 69;            // MIDI note of A4
constexpr double kSemitonesPerOctave = 12.0;
constexpr double kCentPerSemitone = 100.0;
constexpr float kPercentToFactor = 0.01f;
} // namespace

VerySimpleSynthAudio::VerySimpleSynthAudio(VerySimpleSynthAudioProcessor* processor)
:SynchronBlockProcessor(), m_processor(processor)
{
    m_heldNotes.reserve(kNumMidiNotes); // no allocation in the audio thread
    m_oscillators = { &m_naiveOscillator, &m_wavetableOscillator, &m_polyBlepOscillator, &m_polyBlep4Oscillator,
                      &m_minBlepOscillator, &m_blitOscillator, &m_dpwOscillator, &m_oversamplingOscillator };
}

void VerySimpleSynthAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock,max_channels);
    int synchronblocksize;
    synchronblocksize = static_cast<int>(round(g_desired_blocksize_ms * sampleRate * 0.001)); // 0.001 to transform ms to seconds;
    if (g_forcePowerOf2)
    {
        int nextpowerof2 = int(log2(synchronblocksize))+1;
        synchronblocksize = int(pow(2,nextpowerof2));
    }
    prepareSynchronProcessing(max_channels,synchronblocksize);
    m_Latency = synchronblocksize; // '=', not '+=': prepareToPlay can be called several times

    for (auto* oscillator : m_oscillators)
    {
        oscillator->prepare(sampleRate);
    }
    m_envelope.prepare(sampleRate);
    m_heldNotes.clear();
    m_currentNote = kNoNote;
    m_pitchBendSemitones = 0.0;
    m_levelGain.reset(sampleRate, kLevelRampSeconds);
    m_velocityGain.reset(sampleRate, kLevelRampSeconds);
    m_velocityGain.setCurrentAndTargetValue(0.0f);
    if (m_level != nullptr)
    {
        m_levelGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(m_level->load()));
    }
}

int VerySimpleSynthAudio::processSynchronBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer &midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(NrOfBlocksSinceLastProcessBlock);
    updateParameters();

    // render up to each MIDI event, then handle it: notes start at the right sample
    const int numSamples = buffer.getNumSamples();
    int position = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventPosition = juce::jlimit(position, numSamples, metadata.samplePosition);
        render(buffer, position, eventPosition);
        handleMidiMessage(metadata.getMessage());
        position = eventPosition;
    }
    render(buffer, position, numSamples);

    // mono synth: the same signal on all channels
    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
    }
    return 0;
}

void VerySimpleSynthAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>> &paramVector)
{
    // one line per parameter, made from its definition in VerySimpleSynth.h (see tools/ParameterSpec.h)
    paramVector.push_back(jade::makeChoiceParameter(g_paramModel));
    paramVector.push_back(jade::makeChoiceParameter(g_paramWaveform));
    paramVector.push_back(jade::makeSwitchParameter(g_paramLanczosSigma));
    paramVector.push_back(jade::makeParameter(g_paramOctave));
    paramVector.push_back(jade::makeParameter(g_paramFineTune));
    paramVector.push_back(jade::makeParameter(g_paramAttack));
    paramVector.push_back(jade::makeParameter(g_paramDecay));
    paramVector.push_back(jade::makeParameter(g_paramSustain));
    paramVector.push_back(jade::makeParameter(g_paramRelease));
    paramVector.push_back(jade::makeParameter(g_paramLevel));
}

void VerySimpleSynthAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState> &vts)
{
    m_model = vts->getRawParameterValue(g_paramModel.ID);
    m_waveform = vts->getRawParameterValue(g_paramWaveform.ID);
    m_lanczosSigma = vts->getRawParameterValue(g_paramLanczosSigma.ID);
    m_octave = vts->getRawParameterValue(g_paramOctave.ID);
    m_fineTune = vts->getRawParameterValue(g_paramFineTune.ID);
    m_attack = vts->getRawParameterValue(g_paramAttack.ID);
    m_decay = vts->getRawParameterValue(g_paramDecay.ID);
    m_sustain = vts->getRawParameterValue(g_paramSustain.ID);
    m_release = vts->getRawParameterValue(g_paramRelease.ID);
    m_level = vts->getRawParameterValue(g_paramLevel.ID);
}

void VerySimpleSynthAudio::updateParameters()
{
    const auto waveform = static_cast<vss::Waveform>(juce::roundToInt(m_waveform->load()));
    m_octaveShift = m_octave->load();
    m_fineTuneCent = m_fineTune->load();
    m_wavetableOscillator.setLanczosSigma(m_lanczosSigma->load() >= 0.5f);

    // oscillator model: the new oscillator gets waveform and frequency, then continues with the
    // phase of the old one (no click; setPhase also sets its inner state to fit)
    int modelIndex = juce::roundToInt(m_model->load());
    modelIndex = juce::jlimit(0, static_cast<int>(m_oscillators.size()) - 1, modelIndex);
    const auto model = static_cast<vss::OscillatorModel>(modelIndex);
    if (model != m_activeModel)
    {
        const double phase = m_activeOscillator->getPhase();
        m_activeOscillator = m_oscillators[static_cast<size_t>(modelIndex)];
        m_activeModel = model;
        m_activeOscillator->setWaveform(waveform);
        updateFrequency();
        m_activeOscillator->setPhase(phase);
    }
    m_activeOscillator->setWaveform(waveform);
    updateFrequency();

    m_envelope.setParameters(m_attack->load(), m_decay->load(), kPercentToFactor * m_sustain->load(), m_release->load());
    m_levelGain.setTargetValue(juce::Decibels::decibelsToGain(m_level->load()));
}

void VerySimpleSynthAudio::handleMidiMessage(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        noteOn(message.getNoteNumber(), message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        noteOff(message.getNoteNumber());
    }
    else if (message.isPitchWheel())
    {
        // 0..16383, centre 8192 -> -1..+1 -> +-kPitchBendRangeSemitones
        constexpr double kPitchWheelCentre = 8192.0;
        m_pitchBendSemitones = kPitchBendRangeSemitones * (message.getPitchWheelValue() - kPitchWheelCentre) / kPitchWheelCentre;
        updateFrequency();
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        allNotesOff();
    }
}

void VerySimpleSynthAudio::noteOn(int note, float velocity)
{
    // a key that is pressed again moves to the end of the list
    m_heldNotes.erase(std::remove(m_heldNotes.begin(), m_heldNotes.end(), note), m_heldNotes.end());
    m_heldNotes.push_back(note);

    const bool isNewSound = !m_envelope.isActive();
    m_currentNote = note;
    updateFrequency();
    if (isNewSound)
    {
        m_activeOscillator->reset(); // a new sound starts at phase 0 (reproducible)
        m_scope.restart();
    }
    if (m_envelope.isActive())
    {
        m_velocityGain.setTargetValue(velocity); // a short ramp, no step in the sounding note
    }
    else
    {
        m_velocityGain.setCurrentAndTargetValue(velocity);
    }
    m_envelope.noteOn(); // starts from the current level: no click
}

void VerySimpleSynthAudio::noteOff(int note)
{
    m_heldNotes.erase(std::remove(m_heldNotes.begin(), m_heldNotes.end(), note), m_heldNotes.end());
    if (note != m_currentNote)
    {
        return; // a key that does not sound was released
    }
    if (m_heldNotes.empty())
    {
        m_envelope.noteOff();
        return;
    }
    // back to the last key that is still held, without a new attack
    m_currentNote = m_heldNotes.back();
    updateFrequency();
}

void VerySimpleSynthAudio::allNotesOff()
{
    m_heldNotes.clear();
    m_envelope.noteOff();
}

void VerySimpleSynthAudio::updateFrequency()
{
    if (m_currentNote == kNoNote)
    {
        return;
    }
    const double semitones = m_currentNote - kReferenceNote + kSemitonesPerOctave * m_octaveShift
                           + m_fineTuneCent / kCentPerSemitone + m_pitchBendSemitones;
    const double frequency = kReferenceFrequency * std::pow(2.0, semitones / kSemitonesPerOctave);
    m_activeOscillator->setFrequency(frequency);
}

void VerySimpleSynthAudio::render(juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    float* out = buffer.getWritePointer(0);
    for (int n = startSample; n < endSample; ++n)
    {
        const float level = m_levelGain.getNextValue();
        const float velocity = m_velocityGain.getNextValue();
        if (!m_envelope.isActive())
        {
            out[n] = 0.0f;
            continue;
        }
        const float envelope = m_envelope.processSample();
        const double phase = m_activeOscillator->getPhase();
        const float oscillator = m_activeOscillator->processSample();
        m_scope.push(oscillator, phase, m_activeOscillator->getPhaseIncrement()); // before the envelope
        out[n] = oscillator * envelope * velocity * level;
    }
}


VerySimpleSynthGUI::VerySimpleSynthGUI(VerySimpleSynthAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
:m_processor(p) ,m_apvts(apvts), m_oscilloscope(p.m_algo.getScope(), apvts)
{
    m_oscillatorGroup.setText("Oscillator");
    m_envelopeGroup.setText("Envelope");
    m_outputGroup.setText("Output");
    addAndMakeVisible(m_oscillatorGroup);
    addAndMakeVisible(m_envelopeGroup);
    addAndMakeVisible(m_outputGroup);

    setupChoice(m_model, g_paramModel);
    setupChoice(m_waveform, g_paramWaveform);
    setupSwitch(m_lanczosSigma, g_paramLanczosSigma);
    setupKnob(m_octave, g_paramOctave);
    setupKnob(m_fineTune, g_paramFineTune);
    setupKnob(m_attack, g_paramAttack);
    setupKnob(m_decay, g_paramDecay);
    setupKnob(m_sustain, g_paramSustain);
    setupKnob(m_release, g_paramRelease);
    setupKnob(m_level, g_paramLevel);

    m_model.comboBox.onChange = [this]() { updateLanczosSigmaEnabled(); };
    updateLanczosSigmaEnabled();

    addAndMakeVisible(m_oscilloscope);
}

template <class Spec>
void VerySimpleSynthGUI::setupKnob(Knob& knob, const Spec& spec)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 1, 1); // the real size is set in placeKnob()
    knob.slider.setTooltip(jade::helpText(spec));
    // range, value and text come from the parameter
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(m_apvts, spec.ID, knob.slider);
    knob.label.setText(spec.name, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(knob.slider);
    addAndMakeVisible(knob.label);
}

template <class Spec>
void VerySimpleSynthGUI::setupChoice(Choice& choice, const Spec& spec)
{
    // the entries first, then the attachment (it selects the entry of the current value)
    choice.comboBox.addItemList(jade::choiceNames(spec), 1);
    choice.comboBox.setTooltip(jade::choiceHelpText(spec));
    choice.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(m_apvts, spec.ID, choice.comboBox);
    choice.label.setText(spec.name, juce::dontSendNotification);
    choice.label.setJustificationType(juce::Justification::bottomLeft);
    addAndMakeVisible(choice.comboBox);
    addAndMakeVisible(choice.label);
}

template <class Spec>
void VerySimpleSynthGUI::setupSwitch(Switch& control, const Spec& spec)
{
    control.button.setButtonText(spec.name);
    control.button.setTooltip(jade::switchHelpText(spec));
    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(m_apvts, spec.ID, control.button);
    addAndMakeVisible(control.button);
}

void VerySimpleSynthGUI::updateLanczosSigmaEnabled()
{
    const int wavetableId = static_cast<int>(vss::OscillatorModel::Wavetable) + 1; // combo box ids start at 1
    m_lanczosSigma.button.setEnabled(m_model.comboBox.getSelectedId() == wavetableId);
}

float VerySimpleSynthGUI::scaleFactor() const
{
    return static_cast<float>(getWidth()) / g_minGuiSize_x;
}

void VerySimpleSynthGUI::paint(juce::Graphics &g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).brighter(0.3f));

    // Take colours from the LookAndFeel (findColour), not fixed ones like juce::Colour(0xffffffff):
    // then your GUI follows the day/night theme (WITH_DAYNIGHT) and any other LookAndFeel.
    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    const float scale = scaleFactor();
    g.setFont (g_versionFontSize * scale);
    
    juce::String text2display = "VerySimpleSynth V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH);
    // in the strip below the oscilloscope (see resized())
    auto versionArea = getLocalBounds().removeFromBottom(static_cast<int>(g_versionHeight * scale));
    versionArea.removeFromRight(static_cast<int>(g_guiMargin * scale));
    g.drawFittedText (text2display, versionArea, juce::Justification::centredRight, 1);
}

void VerySimpleSynthGUI::placeKnob(Knob& knob, juce::Rectangle<int> area)
{
    const float scale = scaleFactor();
    const int labelHeight = static_cast<int>(g_labelHeight * scale);
    knob.label.setFont(juce::FontOptions(g_labelFontSize * scale));
    knob.label.setBounds(area.removeFromTop(labelHeight));
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, area.getWidth(), labelHeight);
    knob.slider.setBounds(area);
}

void VerySimpleSynthGUI::placeChoice(Choice& choice, juce::Rectangle<int>& area)
{
    const float scale = scaleFactor();
    choice.label.setFont(juce::FontOptions(g_labelFontSize * scale));
    choice.label.setBounds(area.removeFromTop(static_cast<int>(g_labelHeight * scale)));
    choice.comboBox.setBounds(area.removeFromTop(static_cast<int>(g_choiceHeight * scale)));
}

void VerySimpleSynthGUI::resized()
{
	auto r = getLocalBounds();
    const float scale = scaleFactor();
    const int margin = static_cast<int>(g_guiMargin * scale);
    const int groupTitleHeight = static_cast<int>(g_groupTitleHeight * scale);
    r.removeFromBottom(static_cast<int>(g_versionHeight * scale)); // version text, see paint()
    r.removeFromTop(margin);
    r.reduce(margin, 0); // at the bottom the version strip is margin enough

    // top: the controls; below: the oscilloscope
    auto controls = r.removeFromTop(static_cast<int>(g_controlsHeight * scale));
    r.removeFromTop(margin);
    m_oscilloscope.setBounds(r);

    // three groups side by side: oscillator (2 columns of choices, 2 knobs), envelope (4 knobs), output (1 knob)
    constexpr int kOscillatorColumns = 4;
    constexpr int kEnvelopeColumns = 4;
    constexpr int kOutputColumns = 1;
    constexpr int kAllColumns = kOscillatorColumns + kEnvelopeColumns + kOutputColumns;
    const int columnWidth = (controls.getWidth() - 2 * margin) / kAllColumns;

    auto oscillatorArea = controls.removeFromLeft(kOscillatorColumns * columnWidth);
    controls.removeFromLeft(margin);
    auto envelopeArea = controls.removeFromLeft(kEnvelopeColumns * columnWidth);
    controls.removeFromLeft(margin);
    auto outputArea = controls;

    m_oscillatorGroup.setBounds(oscillatorArea);
    m_envelopeGroup.setBounds(envelopeArea);
    m_outputGroup.setBounds(outputArea);

    // inside the groups: below the title, a small margin on all sides
    for (auto* area : { &oscillatorArea, &envelopeArea, &outputArea })
    {
        area->removeFromTop(groupTitleHeight);
        area->reduce(margin, margin);
    }

    // oscillator: model, waveform and Lanczos sigma on the left, the two knobs on the right
    auto choices = oscillatorArea.removeFromLeft(oscillatorArea.getWidth() / 2);
    choices.removeFromRight(margin);
    placeChoice(m_model, choices);
    placeChoice(m_waveform, choices);
    choices.removeFromTop(margin);
    m_lanczosSigma.button.setBounds(choices.removeFromTop(static_cast<int>(g_choiceHeight * scale)));
    const int oscillatorKnobWidth = oscillatorArea.getWidth() / 2;
    placeKnob(m_octave, oscillatorArea.removeFromLeft(oscillatorKnobWidth));
    placeKnob(m_fineTune, oscillatorArea);

    const int envelopeKnobWidth = envelopeArea.getWidth() / kEnvelopeColumns;
    placeKnob(m_attack, envelopeArea.removeFromLeft(envelopeKnobWidth));
    placeKnob(m_decay, envelopeArea.removeFromLeft(envelopeKnobWidth));
    placeKnob(m_sustain, envelopeArea.removeFromLeft(envelopeKnobWidth));
    placeKnob(m_release, envelopeArea);

    placeKnob(m_level, outputArea);
}
