# VerySimpleSynth: ReadMe

## Check, if everything is OK
Since you read this file, you unzipped the downloaded file.

The directory should contain:
1. This ReadMeFirst.txt file (in MarkDown format, you can rename it in ReadMeFirst.md for a better view)
2. LICENSE and LICENSE-AGPL-3.0.txt, the licenses
3. a directory called VerySimpleSynth.vst3 (the VST3 plugin)
4. the Standalone application (Windows: VerySimpleSynth.exe, macOS: VerySimpleSynth.app, Linux: VerySimpleSynth)
5. (macOS only) VerySimpleSynth.component (the AU plugin)

## Installation

### Windows
Copy the directory VerySimpleSynth.vst3 to C:\Program Files\Common Files\VST3

### Mac VST3
Copy the directory VerySimpleSynth.vst3 to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/VST3

### Mac AU
Copy VerySimpleSynth.component to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/Components

The macOS version is not signed with an Apple Developer ID yet. If macOS reports that the
plugin is damaged or cannot be opened, remove the quarantine flag in the Terminal, e.g.
xattr -cr ~/Library/Audio/Plug-Ins/VST3/VerySimpleSynth.vst3

### Linux
Copy the directory VerySimpleSynth.vst3 to
/home/yourUSERNAME/.vst3/

Done! Start your DAW and let it rescan the plugins.
The Standalone application runs without a DAW: play it with the on-screen keyboard or a MIDI keyboard
(choose the audio and MIDI devices in its Options menu).

The manual (ManualVerySimpleSynth.pdf) explains the controls and is a small tutorial about the
oscillator models: what aliasing is and the pros and cons of each method.

Have fun!

Joerg Bitzer


# Source code and license

## Download source code
You can download the source code at GitHub:

https://github.com/JoergBitzer/VerySimpleSynth

## License
- The source code is open source under the MIT License (see LICENSE),
  (c) Joerg Bitzer.
- The plugin binaries contain third-party code: JUCE (used under the AGPLv3), the VST3 SDK
  by Steinberg (MIT License) and, on macOS, the Audio Unit SDK by Apple (Apache License 2.0).
  Therefore the binaries as a whole are distributed under the GNU Affero General Public
  License v3 (see LICENSE-AGPL-3.0.txt). The complete source code is the repository above
  plus JUCE.

VST is a registered trademark of Steinberg Media Technologies GmbH.

The plugin comes without any warranty (see the licenses).
