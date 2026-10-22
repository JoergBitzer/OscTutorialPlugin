#!/usr/bin/env python3
"""
measure_models.py -- measures all oscillator models of OscTutorialPlugin (needs numpy).

Renders one note per model and waveform with the Tester (build it with -DAAT_BUILD_TESTER=ON) and prints
  - alias: energy that is not at a harmonic of f0 (or DC), relative to the total energy (dB, lower = better)
  - max:   the largest value (overshoot of the Gibbs phenomenon: about 1.09 at a jump)
  - corr:  correlation with the wavetable output (1 = same shape and phase)

Usage:  tools/analysis/measure_models.py <path/to/OscTutorialPlugin_Tester> [note ...]   (default notes 60 96)
The Tester runs with a temporary HOME (your presets stay untouched).
"""
import os
import struct
import subprocess
import sys
import tempfile

import numpy as np

SAMPLE_RATE = 48000
SECONDS = 1.5
MODELS = ["Naive", "Wavetable", "Wavetable + Lanczos", "PolyBLEP", "PolyBLEP 4-point", "minBLEP", "BLIT", "DPW", "Oversampling 8x"]
WAVEFORMS = ["Sine", "Triangle", "Rectangle", "Sawtooth"]
WAVETABLE = 1
HARMONIC_HALF_WIDTH_BINS = 4


def write_silence(path, seconds):
    n = int(seconds * SAMPLE_RATE)
    data = np.zeros(n, dtype="<i2").tobytes()
    header = (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt "
              + struct.pack("<IHHIIHH", 16, 1, 1, SAMPLE_RATE, SAMPLE_RATE * 2, 2, 16)
              + b"data" + struct.pack("<I", len(data)))
    with open(path, "wb") as f:
        f.write(header + data)


def read_float_wav(path):
    with open(path, "rb") as f:
        b = f.read()
    i = 12
    channels = 1
    data = b""
    while i < len(b):
        chunk_id = b[i:i + 4]
        size = struct.unpack("<I", b[i + 4:i + 8])[0]
        body = b[i + 8:i + 8 + size]
        if chunk_id == b"fmt ":
            channels = struct.unpack("<HH", body[:4])[1]
        if chunk_id == b"data":
            data = body
        i += 8 + size + (size & 1)
    return np.frombuffer(data, dtype="<f4").reshape(-1, channels)[:, 0]


def alias_db(x, f0):
    n = len(x)
    spectrum = np.abs(np.fft.rfft(x * np.blackman(n))) ** 2
    freqs = np.fft.rfftfreq(n, 1.0 / SAMPLE_RATE)
    is_harmonic = np.zeros(len(spectrum), dtype=bool)
    bin_width = SAMPLE_RATE / n
    # DC is not aliasing (minBLEP has a tiny DC offset): not counted, like a harmonic
    for h in np.arange(0.0, SAMPLE_RATE / 2, f0):
        is_harmonic |= np.abs(freqs - h) < HARMONIC_HALF_WIDTH_BINS * bin_width
    return 10 * np.log10(spectrum[~is_harmonic].sum() / spectrum.sum())


def render(tester, home, silence, out, note, model, waveform):
    env = dict(os.environ, HOME=home)
    subprocess.run([tester, "render", silence, out, "--note", str(note), "--hold", str(SECONDS),
                    "--set", f"Model={model}", "--set", f"Waveform={waveform}", "--set", "Level=0",
                    "--set", "Attack=1", "--set", "Sustain=100"],
                   check=True, env=env, stdout=subprocess.DEVNULL)
    return read_float_wav(out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    tester = os.path.abspath(sys.argv[1])
    notes = [int(n) for n in sys.argv[2:]] or [60, 96]
    with tempfile.TemporaryDirectory() as tmp:
        silence = os.path.join(tmp, "silence.wav")
        write_silence(silence, SECONDS)
        for note in notes:
            f0 = 440.0 * 2 ** ((note - 69) / 12)
            print(f"\nnote {note} ({f0:.1f} Hz)")
            print(f"{'model':18s}" + "".join(f"{w:>27s}" for w in WAVEFORMS))
            print(f"{'':18s}" + "        alias   max   corr" * len(WAVEFORMS))
            segments = {}
            for model in range(len(MODELS)):
                for waveform in range(len(WAVEFORMS)):
                    x = render(tester, tmp, silence, os.path.join(tmp, "out.wav"), note, model, waveform)
                    segments[(model, waveform)] = x[int(0.3 * SAMPLE_RATE):int(1.3 * SAMPLE_RATE)]
            for model in range(len(MODELS)):
                row = f"{MODELS[model]:18s}"
                for waveform in range(len(WAVEFORMS)):
                    x = segments[(model, waveform)]
                    reference = segments[(WAVETABLE, waveform)]
                    corr = np.corrcoef(x, reference)[0, 1]
                    row += f"       {alias_db(x, f0):6.1f}  {np.max(x) / (100 / 127):4.2f}  {corr:5.3f}"
                print(row)
    return 0


if __name__ == "__main__":
    sys.exit(main())
