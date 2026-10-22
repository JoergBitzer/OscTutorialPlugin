/*
    PolyBlepOscillator.h
    Author: J. Bitzer @ TGM, Jade Hochschule (written with Claude Code)
    Date: 2026-10-21
    Description: naive waveform plus a short polynomial correction around each discontinuity:
      - jump (sawtooth, rectangle): polyBLEP = band-limited step minus ideal step
      - corner (triangle):          polyBLAMP = its integral (band-limited ramp minus ideal ramp)
    The correction is a function of the distance d (in samples) between the sample and the
    discontinuity, so it can also be applied BEFORE the discontinuity (the phase tells us where the
    next one is).
      2-point: |d| < 1, the step is the integral of a triangle (linear B-spline), Valimaki & Huovilainen 2007
      4-point: |d| < 2, the step is the integral of a cubic B-spline, Valimaki, Pekonen & Nam 2012
               (less aliasing, but it also damps the highest harmonics more)
    BLAMP: Esqueda, Valimaki & Bilbao 2016.
    License: MIT
*/
#pragma once
#include "Oscillator.h"

namespace otp
{
class PolyBlepOscillator : public Oscillator
{
public:
    enum class Order
    {
        TwoPoint,
        FourPoint
    };
    explicit PolyBlepOscillator(Order order);
    float processSample() override;

    // residuals for a unit jump (BLEP) and for a change of the slope by 1 per sample (BLAMP);
    // d = signed distance from the discontinuity in samples (negative: before it)
    static double blep2(double d);
    static double blamp2(double d);
    static double blep4(double d);
    static double blamp4(double d);

private:
    Order m_order;
};
} // namespace otp
