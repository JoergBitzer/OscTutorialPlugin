# VerySimpleSynth -- planning

Status: version 0.1.0 done (written while building, 2026-10-21)

## Goal
A very simple synthesizer for teaching: one oscillator, one ADSR envelope, MIDI input, presets and the
on-screen MIDI keyboard of the AdvancedAudioTemplate (branch AAT2). The main teaching point is the
**oscillator model**: the same waveform can be generated in three ways, and you can hear (and measure)
the difference:

| Model | Idea | Aliasing |
|---|---|---|
| Naive | evaluate the ideal waveform at the phase of each sample | strong (for all waveforms with corners or jumps) |
| Wavetable | one period per octave, made by additive synthesis with only the harmonics below fs/2, read with linear interpolation | almost none (some from interpolation and from switching tables) |
| PolyBLEP / PolyBLAMP | naive waveform plus a short polynomial correction at every jump (BLEP) or corner (BLAMP) | strongly reduced, a little left near fs/2 |

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

## Parameters
| ID | Name | Range | Default |
|---|---|---|---|
| Model | Oscillator model | Naive, Wavetable, PolyBLEP | Naive |
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
      Oscillator* m_activeOscillator --> NaiveOscillator | WavetableOscillator | PolyBlepOscillator   (Oscillators.h/.cpp)
      SimpleEnvelope (linear ADSR)                                                                     (SimpleEnvelope.h/.cpp)
  VerySimpleSynthGUI  -- groups Oscillator / Envelope / Output, combo boxes and knobs
```
The algorithm files do not include any GUI module (they can be tested headless).

### The three oscillator models in detail
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
("Blip" in the task: I understood it as the bandlimited impulse train, BLIT, which is the derivative of BLEP. BLIT
oscillators (Stilson & Smith 1996) integrate a BLIT to get saw/rectangle/triangle; that is listed as a further model
in "How to go on".)

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

## Test results (2026-10-21, Linux, 48 kHz, Debug build, Tester with `--note`)
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
- Analysis scripts used: numpy, reading the 32-bit float wav files of the Tester (not part of the repository yet,
  see "Tests" below).

## How to go on (ideas)
- **Polyphony**: `juce::Synthesiser` with `SynthesiserVoice`s, or an own voice array with voice stealing. Then
  the note stack moves into a voice allocator.
- **More oscillator models**: BLIT (integrated bandlimited impulse train), minBLEP (table based, longer and better
  than polyBLEP), higher-order polyBLEP (4-point), oversampling (2x/4x) + decimation filter on the naive model, DPW
  (differentiated parabolic waveform). This would make the comparison complete.
- **Wavetable**: crossfade between neighbouring tables (no brightness step), cubic (Hermite) interpolation, tables with
  more samples, mipmaps built by FFT, user-loadable single-cycle waveforms, wavetable morphing.
- **Display for teaching**: oscilloscope (one period) and spectrum (FFT) of the output in the GUI, with marks at the
  harmonics: aliasing becomes visible. A "show aliasing" mode that subtracts the wavetable output from the naive one.
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
