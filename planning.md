# VerySimpleSynth -- planning

Status: version 0.2.1 (oscilloscope, 9 oscillator models) done, 2026-10-21.
0.2.1: Lanczos sigma is a model of its own ("Wavetable + Lanczos", directly below "Wavetable") instead of a switch.
Version 0.1.0: the first synth with 3 models (sections marked 0.1.0 keep their results).

## Goal
A very simple synthesizer for teaching: one oscillator, one ADSR envelope, MIDI input, presets and the
on-screen MIDI keyboard of the AdvancedAudioTemplate (branch AAT2). The main teaching point is the
**oscillator model**: the same waveform can be generated in nine ways, and you can hear, see (oscilloscope)
and measure the difference:

| Model | Idea | Aliasing | What the oscilloscope shows |
|---|---|---|---|
| Naive | evaluate the ideal waveform at the phase of each sample | strong (all waveforms with corners or jumps) | exact ideal shape, jitter from period to period at high notes |
| Wavetable | one period per octave, additive synthesis with only the harmonics below fs/2, linear interpolation | almost none | Gibbs ringing at jumps (+9 % of the jump) |
| Wavetable + Lanczos | the same, harmonic k weighted with sinc(k/(H+1)) | almost none | overshoot about 1 % of the jump, a little duller |
| PolyBLEP | naive + 2-point polynomial correction at jumps (BLEP) / corners (BLAMP) | strongly reduced | only the 2 samples around a jump are changed |
| PolyBLEP 4-point | the same with a cubic B-spline, 4 samples | about 10 dB better | 4 samples changed, the top harmonics are damped more |
| minBLEP | correction from a minimum-phase table, 32 samples, only after the jump | as good as wavetable | delay of about 3 samples, asymmetric overshoot (no pre-ringing) |
| BLIT | leaky integration of band-limited impulse trains | as good as wavetable | ringing near fs/2, larger overshoot (discrete integrator), slight tilt (leak) |
| DPW | difference of the antiderivative (= mean over one sample) | about 10 dB better than naive | jumps become 1-sample ramps |
| Oversampling 8x | naive at 8 fs, FIR low-pass (512 taps), decimation | about 19 dB better than naive | Gibbs ringing (the low-pass filter) |

## Decisions
- Start from AAT2 (commit ea9b5a5) in `~/AudioDev/VerySimpleSynth`, its own git repository, JUCE 9.0.3 as submodule
  (cloned from the local copy of the template, the URL points to GitHub). No GitHub repository yet.
- Switches: `WITH_PRESETHANDLERGUI`, `WITH_DAYNIGHT`, `WITH_MIDIKEYBOARD`, `FACTORY_PRESETS`.
- Plugin: `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, VST3 categories `Instrument Synth`, AU type `kAudioUnitType_MusicDevice`,
  plugin code `VsS1` (not used by any other plugin in `~/AudioDev`).
- **Monophonic**, last-note priority (a note stack): the simplest voice handling that still plays legato lines
  correctly. Polyphony is the first item in "How to go on".
- Every note-on restarts the envelope **from its current level** (no click); releasing the sounding note while others
  are still held goes back to the last held one without a new attack.
- The oscillator phase restarts at 0 only if the envelope was idle (reproducible tests, no phase jump while sounding).
- All waveforms are phase aligned: each starts at 0 and rises (the fundamental is +sin in all of them). So a change of the
  model or the waveform does not jump in phase, and the three models can be compared sample by sample.
- When the model is switched, the new oscillator takes over the phase of the old one.
- Extra parameter **Level** (dB), and velocity (linear) and pitch bend (+-2 semitones) are used, because the keyboard
  of the template sends them anyway.
- Envelope times have a logarithmic range (`logFrequency = true` in the parameter spec gives a log range in whole units,
  it works for milliseconds as well).
- 0.2.0: the oscilloscope shows the oscillator output **before the envelope** (amplitude always 1, so overshoot can be read
  against the dashed lines at +-1). It is triggered at the start of a period (phase wrap), so the display stands still
  for every model; 1 or 2 periods; the ideal waveform as a thin line and dots at the samples (only if they are at least
  4 px apart, i.e. for high notes). The view settings are not plugin parameters (not saved, not automatable).
- 0.2.1 (author's decision): "Wavetable + Lanczos" is a model in the list, directly below "Wavetable" (not a switch, as in
  0.2.0). The models are ordered for teaching, not for compatibility: presets of 0.2.0 store other model numbers (factory
  presets renumbered, presetversion 3). Two `WavetableOscillator` instances (constructor flag), each builds only its
  own tables in `prepareToPlay`.
- 0.2.0: one file per oscillator model in `oscillators/`, so each method can be read (and taught) on its own. Shared:
  the phase accumulator, `naiveWaveform()`, `naiveSlope()` and the list of jumps and corners per waveform
  (`getDiscontinuities()`, used by polyBLEP and minBLEP).
- 0.2.0: all models stay phase aligned with the naive model, so they can be switched while a note sounds: DPW uses a
  centred difference (no half-sample delay), oversampling runs ahead by the delay of its filter (phase lead), BLIT
  starts its integrators 64 samples early. Exception: minBLEP, whose delay is the point of minimum phase.

## Parameters
| ID | Name | Range | Default |
|---|---|---|---|
| Model | Model | Naive, Wavetable, Wavetable + Lanczos, PolyBLEP, PolyBLEP 4-point, minBLEP, BLIT, DPW, Oversampling 8x | Naive |
| Waveform | Waveform | Sine, Triangle, Rectangle, Sawtooth | Sawtooth |
| Octave | Octave | -3 .. +3 | 0 |
| FineTune | Fine tune | -100 .. +100 cent | 0 |
| Attack | Attack | 1 .. 5000 ms (log) | 10 ms |
| Decay | Decay | 1 .. 5000 ms (log) | 300 ms |
| Sustain | Sustain | 0 .. 100 % | 70 % |
| Release | Release | 1 .. 5000 ms (log) | 300 ms |
| Level | Level | -60 .. 0 dB | -12 dB |

Frequency: `f = 440 Hz * 2^((note - 69 + 12*octave + cent/100 + bend)/12)`, limited to 0.45 fs.

## Architecture
```
PluginProcessor (template)  -- keyboard state merges the on-screen keyboard into the MIDI buffer
  VerySimpleSynthAudio : SynchronBlockProcessor   (VerySimpleSynth.h/.cpp)
      reads parameters once per synchronous block (2 ms)
      splits the block at MIDI events, renders the parts in between
      note stack, pitch bend, velocity, level (smoothed)
      Oscillator* m_activeOscillator --> one of 9 models                    (oscillators/*.h/.cpp)
      SimpleEnvelope (linear ADSR)                                          (SimpleEnvelope.h/.cpp)
      ScopeCapture: records 1-2 periods of the oscillator output            (ScopeCapture.h/.cpp)
  VerySimpleSynthGUI  -- groups Oscillator / Envelope / Output, below the Oscilloscope (Oscilloscope.h/.cpp)
```
The algorithm files do not include any GUI module (they can be tested headless).

Oscilloscope data path: the audio thread pushes every oscillator sample with its phase into `ScopeCapture`. A recording
starts at a phase wrap and stores `x = position in periods`, so the samples are drawn exactly where they belong, also for
periods that are not a whole number of samples. A finished recording is copied into the "published" buffer under a
`juce::SpinLock`, but the audio thread only uses `tryLock` (it never waits; if the GUI is copying, this recording is
dropped). The GUI copies the newest recording 30 times per second. All memory is allocated in the constructor
(65536 points = 2 periods down to 1.5 Hz).

### The oscillator models in detail (0.1.0: naive, wavetable, polyBLEP)
All share the phase accumulator of the base class `Oscillator` (phase 0..1, increment f/fs, double precision).

**Naive** (`NaiveOscillator`): sine `sin(2 pi p)`, triangle from straight lines, rectangle `+1/-1`, sawtooth `2p' - 1`
(with `p' = p + 0.5`, so it starts at 0). Every jump or corner falls between two samples and its infinitely many
harmonics fold back below fs/2 (aliasing): audible as inharmonic tones, which move in the opposite direction
when the pitch is bent.

**Wavetable** (`WavetableOscillator`): for each waveform 10 tables (2048 samples + 1 guard sample), one per octave of
fundamental frequency (table k is for fundamentals up to 40 Hz * 2^k). Each table is the Fourier series of the
waveform, cut off at the last harmonic below fs/2 for the highest frequency of its octave (and at most 512 harmonics, so
that linear interpolation still has 4 table samples per period of the highest harmonic):
- sawtooth: `2/pi * sum (-1)^(k+1) sin(2 pi k p)/k`
- rectangle: `4/pi * sum_odd sin(2 pi k p)/k`
- triangle: `8/pi^2 * sum_odd (-1)^((k-1)/2) sin(2 pi k p)/k^2`
The tables are built in `prepareToPlay` (only when the sample rate changes), with a sine table and integer index
arithmetic instead of millions of `sin()` calls. Teaching points: memory versus computation, the choice of the
table (lowest octave has the most harmonics, but the top octave of each table loses up to one octave of bandwidth),
the step in brightness at table boundaries, interpolation (linear here; cubic or larger tables are better).

**PolyBLEP / PolyBLAMP** (`PolyBlepOscillator`): naive waveform plus a correction in the two samples around each
discontinuity (Valimaki & Huovilainen 2007; Esqueda, Valimaki & Bilbao 2016 for BLAMP):
- jump (sawtooth, rectangle): 2-point polyBLEP residual, `t/dt` = distance to the jump in samples,
  `2x - x^2 - 1` after and `x^2 + 2x + 1` before the jump (for a jump of height 2);
- corner (triangle): 2-point polyBLAMP, the integral of the BLEP residual: `(1 - |d|)^3 / 6` for a change of the slope by
  1 per sample, `|d| < 1` = distance in samples. A triangle corner changes the slope by `8 f/fs` per sample.
- sine: no correction needed.
("Blip" in the task: I understood it as the bandlimited impulse train, BLIT, which is the derivative of BLEP; it is
model 6 since 0.2.0.)

### The models of 0.2.0
**Wavetable + Lanczos** (Lanczos sigma factors): a Fourier series cut off after H harmonics overshoots at every jump by about 9 % of the
jump, however large H is (Gibbs phenomenon); more harmonics only make the ringing narrower. Multiplying harmonic k by
`sigma_k = sinc(k/(H+1)) = sin(pi k/(H+1)) / (pi k/(H+1))` is the same as averaging the cut-off series over the
period of its highest harmonic: the overshoot drops to about 1 %, the price is a slightly duller top octave
(measured: max 1.18 -> 1.02, fundamental unchanged). Best seen with the presets "Gibbs Demo" / "Lanczos Demo"
(rectangle at C5: 37 harmonics, the ringing is wide).

**PolyBLEP 4-point**: the band-limited impulse is a cubic B-spline over 4 samples instead of a triangle over 2. The
residuals are piecewise polynomials of degree 4 (BLEP) and 5 (BLAMP), derived by integrating the B-spline (see the
code). Better suppression (+10 dB), but the B-spline is also a stronger low-pass in the pass band.

**minBLEP** (Brandt 2001): windowed sinc (Blackman, 16 zero crossings each side, 64 points per sample), made minimum
phase with the real cepstrum (FFT 16384: log|X| -> IFFT -> fold the anticausal part onto the causal part -> FFT ->
exp -> IFFT), integrated = minBLEP. At each jump the residual is added to a ring buffer for the next 32 samples.
Corners need the integral (minBLAMP); its tail does not return to 0 but to -2.5 samples (the delay), so the straight
parts are delayed by the same amount (`delay * slope` term) and only the difference is taken from the table.
"Why is minBLEP shifted downwards on the oscilloscope?" (question of the author): it is not an offset but the delay.
The mean is 0 like that of the other models (measured at C6: sawtooth -0.0003, triangle +0.0001; wavetable +0.0001).
Shifted back by about 3 samples, minBLEP matches the wavetable output (rms difference 0.005 for the triangle). A delayed
rising ramp lies below the ideal one at the same moment (by slope x delay), so next to the ideal curve the rising
parts look lower. Minimum phase means: all of the response after the jump, no pre-ringing -- the price is this delay.
Found while testing: with the cut-off at fs/2, half of the window's transition band lies above fs/2 and aliases (the
12th harmonic of a sawtooth at 2093 Hz came back at -35 dB); with the cut-off at 0.85 fs/2 it is gone (-85 dB).

**BLIT** (Stilson & Smith 1996): `blit(x) = (1/P) sin(pi M x) / sin(pi x)`, M = 2 floor(P/2) + 1, integrated:
sawtooth = integral of `2 (1/P - blit)`, rectangle = integral of `2 (blit(x) - blit(x - 1/2))`, triangle = integral of
`4/P * rectangle` (shifted by a quarter period). Leaky integrators (1 Hz). Three teaching effects, all visible on the
oscilloscope: (1) the leak tilts the rectangle at low notes (with 5 Hz it was about 30 % at 33 Hz, so 1 Hz now);
(2) the running sum is not a true integrator: it boosts the harmonics near fs/2 by up to pi/2, so the overshoot is 1.3
instead of 1.18 and there is ringing near fs/2; (3) the impulses are symmetric, so a start needs the part of the impulse
before the start: the integrators run in for 64 samples (found while testing: without it the rectangle started at +3).

**DPW** (Valimaki 2005): `y = (F(p + dt/2) - F(p - dt/2)) / dt` with the antiderivative F of the naive waveform
(sawtooth: the parabola s^2/4, rectangle: a triangle, triangle: piecewise parabolas, sine: -cos/(2 pi)). This is the
mean of the naive waveform over one sample (box filter): first-order anti-aliasing, cheap, about 10 dB better than
naive. Higher orders (DPW3, DPW4) would use higher antiderivatives and repeated differences.

**Oversampling 8x**: naive at 8 fs, FIR low-pass 512 taps (Blackman-windowed sinc, cut-off 0.42 fs), keep every 8th
sample (only those outputs are computed). Harmonics between fs/2 and 4 fs are removed, only those above 4 fs fold
back: about 20 log10(8) = 18 dB better (measured 19 dB). The filter delay (255.5 high-rate samples) is compensated by
a phase lead. The same method works for any non-linear process, which makes it the general tool.

## Work plan and status
- [x] Copy AAT2, rename, JUCE submodule, first commit on `main`; work on branch `feature/very-simple-synth`
- [x] CMake: synth, MIDI, keyboard, factory presets, plugin code
- [x] Oscillators (3 models x 4 waveforms), envelope, MIDI handling
- [x] GUI (groups, combo boxes, knobs), window 640 x 400 .. 1600 x 1000
- [x] Factory presets (9): Naive Saw, Wavetable Saw, PolyBLEP Saw, Aliasing Demo, Soft Sine Pad, Triangle Flute,
      Square Bass, Pluck, Saw Swell
- [x] Tester: options `--note`, `--velocity`, `--hold` (render a MIDI note), needed to test a synth
- [x] Tests: pitch, aliasing, envelope, pluginval (Debug and Release), GUI snapshots (see "Test results")
- [x] README (new; the template README is now `docs/AAT2_README.md`), `docs/manual/controls.tex`, CLAUDE.md
- [x] Release build installed to `~/.vst3/`
- [x] 0.2.0: oscilloscope (1/2 periods, ideal waveform, sample points), window 880 x 660 .. 1760 x 1320
- [x] 0.2.0: models PolyBLEP 4-point, minBLEP, BLIT, DPW, Oversampling 8x; Lanczos sigma for the wavetable
- [x] 0.2.1: Lanczos sigma as the model "Wavetable + Lanczos" instead of a switch
- [x] 0.2.0: 7 new factory presets (Gibbs Demo, Lanczos Demo, PolyBLEP4 Saw, minBLEP Saw, BLIT Rectangle, DPW Saw,
      Oversampled Saw), presetversion 2 (0.2.1: renumbered models, presetversion 3)
- [x] 0.2.0: `tools/analysis/measure_models.py`: measures aliasing, peak value and correlation of all models
- [ ] Manual text (`docs/manual/ManualVerySimpleSynth.tex` is still the template text; only the controls list and the
      version are updated) and PDF
- [ ] Tried in a DAW (only possible for the author)
- [ ] Not tested: Windows, macOS (CI would build them on a release tag)

## Test plan
1. Build Debug without warnings.
2. Pitch: render A4 (note 69) with octave 0 / +1 / -1 and fine tune +100 cent; measured fundamental must be 440 / 880 /
   220 / 466.16 Hz (within 0.1 %).
3. Aliasing: render a high note (e.g. note 96, 2093 Hz) as sawtooth with each model and measure the energy that is not at
   a harmonic of f0. Expected order: naive >> PolyBLEP > wavetable.
4. Envelope: note on/off, check that the output decays to 0 after the release time.
5. pluginval strictness 10 (Debug and Release), 3 runs each, no assertions.
6. GUI snapshot in both themes (virtual display).

## Test results 0.2.0 (2026-10-21, Linux, 48 kHz)
`tools/analysis/measure_models.py build/VerySimpleSynth_Tester 60 96 108` (Debug build). alias = energy outside the
harmonics and DC in dB (floor of the measurement about -57 dB at C4, -93 dB at C7); max = largest value (1.18 = Gibbs
overshoot of a jump of 2); corr = correlation with the wavetable output.

C7 (2093 Hz, 22.9 samples per period):

| Model | Triangle alias / max | Rectangle alias / max / corr | Sawtooth alias / max / corr |
|---|---|---|---|
| Naive | -40.3 / 1.00 | -14.7 / 1.00 / 0.980 | -12.8 / 1.00 / 0.967 |
| Wavetable | -92.5 / 0.96 | -86.9 / 1.18 / 1.000 | -85.5 / 1.08 / 1.000 |
| PolyBLEP | -54.0 / 0.94 | -31.9 / 1.00 / 0.997 | -28.5 / 0.91 / 0.995 |
| PolyBLEP 4-point | -65.0 / 0.92 | -43.0 / 1.00 / 0.994 | -38.4 / 0.87 / 0.992 |
| minBLEP | -92.5 / 0.94 | -87.0 / 1.44 / 0.510 | -85.3 / 0.85 / 0.355 |
| BLIT | -92.1 / 1.00 | -84.8 / 1.30 / 0.950 | -83.5 / 1.20 / 0.931 |
| DPW | -48.0 / 0.96 | -25.4 / 1.00 / 0.996 | -22.6 / 0.96 / 0.994 |
| Oversampling 8x | -80.2 / 0.96 | -34.1 / 1.19 / 1.000 | -32.2 / 1.08 / 1.000 |

C4 (262 Hz), rectangle / sawtooth alias: naive -23.6 / -21.8, PolyBLEP -39.6 / -37.7, PolyBLEP 4-point -49.1 / -47.3,
DPW -33.7 / -31.9, Oversampling -43.2 / -41.5; wavetable, minBLEP, BLIT at the floor of the measurement (-58).
C8 (4186 Hz) sawtooth: naive -9.6, PolyBLEP -23.8, 4-point -32.9, DPW -18.5, oversampling -28.8, minBLEP -82.2,
BLIT -80.9, wavetable -82.7. Sine: the same in all models (-93 dB at C7).

Readings: the band-limited methods with long kernels (wavetable, minBLEP, BLIT) are at the floor of the measurement;
the short corrections (polyBLEP, DPW) improve by 10..25 dB; oversampling helps the triangle much more than the
jumps (its harmonics fall with 1/k^2). minBLEP's low correlation is its phase (delay), not an error.

- Wavetable + Lanczos (max value, compared to Wavetable; 0.2.1: same 1.179 -> 1.024 at C5): rectangle 1.178 -> 1.024 (C3), 1.179 -> 1.024 (C5), 1.182 -> 1.023 (C7);
  sawtooth 1.171 -> 1.013 (C3); fundamental unchanged (-0.01 dB, -0.14 dB at C7).
- BLIT start (after the run-in fix): the first period has the same peak values as the steady state.
- Builds Debug and Release without warnings. pluginval (strictness 10, 3 runs): Debug and Release all SUCCESS.
- GUI snapshots (virtual display): oscilloscope with wavetable (Gibbs), PolyBLEP, minBLEP, BLIT, Lanczos in the day theme.

## Test results 0.1.0 (2026-10-21, Linux, 48 kHz, Debug build, Tester with `--note`)
- Build: Debug and Release without warnings.
- Pitch (sine, wavetable): note 69 -> 440.000 Hz; octave +1 -> 880.000; octave -1 -> 220.000; +100 cent -> 466.164;
  -50 cent -> 427.474; note 60 -> 261.626; note 21 / octave -3 -> 3.438 Hz. All within 0.01 %.
- Envelope (A 100 ms, D 200 ms, S 50 %, R 300 ms, note-off at 1 s): measured 0.51 / 1.00 / 0.75 / 0.50 / 0.50 / 0.25 / 0.00
  at 50 / 100 / 200 / 300 / 900 / 1150 / 1300 ms, as expected. Output starts after 96 samples (2 ms synchronous block,
  reported to the host as latency).
- Aliasing: energy outside the harmonics in dB (1 s, Blackman window; about -57 dB at C4 is the floor of this measurement):

  | Note | Waveform | Naive | Wavetable | PolyBLEP |
  |---|---|---|---|---|
  | C4 (262 Hz) | Rectangle | -23.6 | -57.7 | -39.6 |
  | C4 | Sawtooth | -21.8 | -57.9 | -37.7 |
  | C7 (2093 Hz) | Triangle | -40.3 | -92.5 | -54.0 |
  | C7 | Rectangle | -14.7 | -86.9 | -31.9 |
  | C7 | Sawtooth | -12.8 | -85.5 | -28.5 |
  | C8 (4186 Hz) | Triangle | -31.4 | -86.8 | -45.9 |
  | C8 | Rectangle | -11.7 | -84.6 | -29.2 |
  | C8 | Sawtooth | -9.6 | -82.7 | -23.8 |

  Sine: the same in all models. The three models are phase aligned (correlation with the wavetable output >= 0.93,
  1.000 for sine and triangle). Teaching point: the wavetable loses some brightness in the top part of each octave
  (peak of the sawtooth at C8: 0.97 wavetable, 0.83 polyBLEP, because polyBLEP also damps the highest harmonics).
- pluginval 2.0.0-beta3, strictness 10, 3 runs each: Debug and Release all SUCCESS, no assertions
  (the first run had assertions, see "Template feedback": `processBlockBypassed`).
- GUI snapshots (virtual display), night and day theme: fine after moving the version text into its own strip.
- Analysis scripts used: numpy, reading the 32-bit float wav files of the Tester (since 0.2.0 in the repository:
  `tools/analysis/measure_models.py`).

## How to go on (ideas)
- **Polyphony**: `juce::Synthesiser` with `SynthesiserVoice`s, or an own voice array with voice stealing. Then
  the note stack moves into a voice allocator.
- ~~More oscillator models~~ (done in 0.2.0). Further variants: DPW of higher order (DPW3/DPW4), BLIT with a trapezoidal
  integrator (no boost near fs/2), minBLEP with more zero crossings / Kaiser window, oversampling with 2x/4x and a
  half-band filter cascade (cheaper), a "model cost" display (CPU per model).
- **Wavetable**: crossfade between neighbouring tables (no brightness step), cubic (Hermite) interpolation, tables with
  more samples, mipmaps built by FFT, user-loadable single-cycle waveforms, wavetable morphing. Other sigma
  factors (Fejer = triangle window, raised cosine) next to Lanczos.
- **Display for teaching**: ~~oscilloscope~~ (done in 0.2.0). Next: a spectrum (FFT) of the output with marks at the
  harmonics, so aliasing becomes visible (it is hard to see in the time signal); a "difference" mode (model minus
  wavetable); oscilloscope after the envelope; freeze button; a cursor that shows the value of a sample.
- **Pulse width** for the rectangle (PWM), hard sync (a good BLEP exercise), a second oscillator, noise.
- **Filter**: a simple state-variable or ladder low-pass with its own envelope, then the synth becomes "subtractive".
- **Envelope**: exponential segments (more natural), velocity to attack, envelope display; a sustain change currently
  jumps (small step, could be smoothed).
- **Modulation**: LFO for vibrato (mod wheel is shown by the template, but not used yet), portamento/glide.
- **Velocity curve** (now linear), MPE.
- **Tests**: an automated test (ctest) with the pitch and aliasing measurements from the test plan, so regressions are found.
- **Template feedback (AAT2)**: see the section below.

## Template feedback (AAT2)
Found while building this plugin:
- `prepareToPlay` of the template algorithm does `m_Latency += synchronblocksize`: the latency grows with every call,
  and `setLatencySamples` is only called in the constructor (where the latency is still 0), so every AAT plugin
  reports latency 0 although the synchronous block delays by `g_desired_blocksize_ms`. Fixed here
  (`=` and `setLatencySamples` in `prepareToPlay`). As soon as a latency is reported, JUCE's default
  `processBlockBypassed` asserts (pluginval: hundreds of assertions in "Parameter thread safety"): an effect must then
  delay its input by the latency when bypassed, a synth outputs silence (done here). Both should go back into the template.
- The preset bar shows the name of the first preset at the start although the default parameters are active
  (seen in the GUI snapshot), probably the same in all AAT plugins.
- The Tester can only render audio files, a synth needs MIDI: option `--note` added here; could go back into the template.
- `ParameterSpec.h` covers only float parameters; choice parameters (combo boxes) need a spec as well
  (`tools/ChoiceParameterSpec.h` here).
