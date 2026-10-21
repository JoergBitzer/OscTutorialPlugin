/*
    DpwOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: DPW oscillator ("differentiated parabolic waveform", Valimaki 2005). Instead of the
    waveform we compute its antiderivative F (for the sawtooth: a parabola, s^2 / 4) and differentiate it
    numerically:
        y = (F(phase + dt/2) - F(phase - dt/2)) / dt
    This is the mean of the naive waveform over one sample interval (a box filter of 1 sample width):
    the jumps become ramps of one sample, the harmonics fall faster (1/k^2 instead of 1/k), aliasing
    drops. The same works for every waveform with a known antiderivative (here all four). Centred
    difference: no delay, phase aligned with the other models.
    Teaching points: very cheap; the box filter also damps the top octave a little (sinc droop); with
    float numbers and low notes F(a) - F(b) loses precision (we use double).
    License: MIT
*/
#pragma once
#include "Oscillator.h"

namespace vss
{
class DpwOscillator : public Oscillator
{
public:
    float processSample() override;

    // the antiderivative of the naive waveform with respect to the phase (periodic)
    static double antiderivative(Waveform waveform, double phase);
};
} // namespace vss
