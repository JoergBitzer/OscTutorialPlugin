/*
    ChoiceParameterSpec.h
    Author: J. Bitzer @ TGM, Jade Hochschule
    Date: 2026-10-21
    Description: the same idea as ParameterSpec.h, for choice parameters (combo boxes). Each one
    is defined once, in a struct like:

        const struct
        {
            const std::string ID = "Waveform";
            const std::string name = "Waveform";
            const std::vector<std::string> choices = { "Sine", "Triangle" };
            const int defaultIndex = 0;
            const std::string help = "What this control does.";
        } g_paramWaveform;

    - makeChoiceParameter(g_paramWaveform): the AudioParameterChoice (its value is the index
      of the choice; getRawParameterValue() gives the index as float)
    - choiceHelpText(g_paramWaveform): "Waveform (Sine, Triangle; default Sine): What this control does."
    On/off parameters (a choice Off/On, see SwitchParameter.h) work the same way, with a struct
    { ID, name, defaultIsOn, help }: makeSwitchParameter(spec), switchHelpText(spec).
    - choiceNames(g_paramWaveform): the entries, e.g. for a combo box (fill it before its
      ComboBoxAttachment is made: comboBox.addItemList(jade::choiceNames(g_paramWaveform), 1))
    License: MIT
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterSpec.h"
#include "SwitchParameter.h"

namespace jade
{
template <class Spec>
juce::StringArray choiceNames(const Spec& p)
{
    juce::StringArray names;
    for (const auto& choice : p.choices)
        names.add(juce::String(choice));
    return names;
}

template <class Spec>
juce::String choiceHelpText(const Spec& p)
{
    return juce::String(p.name) + " (" + choiceNames(p).joinIntoString(", ") + "; default "
         + juce::String(p.choices[static_cast<size_t>(p.defaultIndex)]) + "): " + juce::String(p.help);
}

template <class Spec>
std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter(const Spec& p)
{
    const int kParameterVersion = 1;
    parameterHelpTexts()[juce::String(p.ID)] = juce::String(p.help);
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(juce::String(p.ID), kParameterVersion),
                                                        juce::String(p.name), choiceNames(p), p.defaultIndex);
}

template <class Spec>
juce::String switchHelpText(const Spec& p)
{
    juce::String defaultText("Off");
    if (p.defaultIsOn)
    {
        defaultText = "On";
    }
    return juce::String(p.name) + " (Off, On; default " + defaultText + "): " + juce::String(p.help);
}

template <class Spec>
std::unique_ptr<juce::AudioParameterChoice> makeSwitchParameter(const Spec& p)
{
    parameterHelpTexts()[juce::String(p.ID)] = juce::String(p.help);
    return makeSwitchParameter(juce::String(p.ID), juce::String(p.name), p.defaultIsOn);
}
} // namespace jade
