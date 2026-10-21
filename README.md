# VerySimpleSynth

A very simple monophonic synthesizer for teaching: one oscillator, one ADSR envelope -- and a switch between three
ways to build the oscillator (naive, wavetable, polyBLEP), so you can hear and measure aliasing.

![VerySimpleSynth](docs/images/GUI.png)

## Features
* One oscillator: waveform Sine, Triangle, Rectangle, Sawtooth; Octave (-3..+3) and Fine tune (+-100 cent)
* Oscillator model (the teaching part):
  * **Naive**: the ideal waveform, sampled. Simple, but with strong aliasing for high notes.
  * **Wavetable**: band-limited tables (Fourier series, one table per octave), linear interpolation.
  * **PolyBLEP**: the naive waveform with polynomial corrections at jumps (polyBLEP) and corners (polyBLAMP).
* Linear ADSR envelope, velocity, pitch bend (+-2 semitones), output level
* MIDI input; on-screen keyboard with pitch and mod wheel
* Presets (preset bar) with factory presets, among them "Naive Saw", "Wavetable Saw", "PolyBLEP Saw" and
  "Aliasing Demo" for the comparison
* Day/night theme, resizable window
* VST3, AU (macOS) and Standalone

Monophonic with last-note priority: hold a key, press another one, release it -- the first one sounds again.

### Try it
Load "Aliasing Demo" and play a scale in the upper octaves: with the model Naive you hear extra tones that do not
belong to the note (they even fall when the pitch rises). Switch to Wavetable or PolyBLEP: they are gone (or much
weaker). Measured energy outside the harmonics (sawtooth, note C7 = 2093 Hz, 48 kHz):

| Naive | PolyBLEP | Wavetable |
|---|---|---|
| -13 dB | -29 dB | -86 dB |

## Controls
| Control | Range | Default | Description |
|---|---|---|---|
| Model | Naive - PolyBLEP | Naive | How the oscillator makes the waveform: Naive (sampled ideal waveform, aliasing), Wavetable (band-limited tables, one per octave), PolyBLEP (naive waveform with polynomial corrections at jumps and corners). |
| Waveform | Sine - Sawtooth | Sawtooth | The waveform of the oscillator (Rectangle: 50 % duty cycle). |
| Octave | -3 - 3 | 0 | Shifts the pitch by whole octaves. |
| Fine tune | -100 cent - 100 cent | 0 cent | Fine tuning in cent (100 cent = 1 semitone). |
| Attack | 1 ms - 5000 ms | 10 ms | Time to rise to full level after a key is pressed. |
| Decay | 1 ms - 5000 ms | 300 ms | Time to fall from full level to the sustain level. |
| Sustain | 0 % - 100 % | 70 % | Level while the key is held (in % of full level). |
| Release | 1 ms - 5000 ms | 300 ms | Time to fall to silence after the key is released. |
| Level | -60.0 dB - 0.0 dB | -12.0 dB | Output level (at velocity 127). |

(generated with `VerySimpleSynth_Tester --manual md`)

## The code
| File | What |
|---|---|
| `Oscillators.h/.cpp` | the three oscillator models -- start reading here |
| `SimpleEnvelope.h/.cpp` | the linear ADSR |
| `VerySimpleSynth.h/.cpp` | parameters, MIDI handling (note stack, pitch bend), rendering, GUI |
| `res/presets/` | factory presets |
| `planning.md` | decisions, how the models work, test results, ideas how to go on |

The plugin is made from the [AdvancedAudioTemplate](https://github.com/JoergBitzer/AdvancedAudioTemplate)
(branch AAT2); its README is in `docs/AAT2_README.md`, the test tools in `HowToTestYourPlugin.md`.

## Build
```console
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target VerySimpleSynth_VST3 VerySimpleSynth_Standalone
```

## License
The source code is under the MIT License (`LICENSE`). JUCE is used under the AGPLv3, so the plugin binaries as a
whole are under the AGPLv3 (`LICENSE-AGPL-3.0.txt`).
