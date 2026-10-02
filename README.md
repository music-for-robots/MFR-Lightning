# MFR-Lightning

[![Build](https://github.com/music-for-robots/MFR-Lightning/actions/workflows/build.yml/badge.svg)](https://github.com/music-for-robots/MFR-Lightning/actions/workflows/build.yml)

A free, open-source guitar effect by [Music For Robots](https://musicforrobots.com): an **early-'80s thrash rhythm rig**. It's a gate, a mid-hump overdrive pushing a cranked British master-volume head, a 4×12 and a plate reverb, for tight, scooped, palm-muted gallops.

## The rig

- **Gate**, for clean stops between palm mutes.
- **Overdrive:** an op-amp mid-hump design that only adds gain and clips above about 720 Hz (about 12 dB more gain at 1.5 kHz than at 150 Hz). With drive low and level high, it tightens and pushes an already-distorted amp rather than adding fizz.
- **Head:** a cascaded-preamp master-volume design, with a bright first stage, a cold-biased second stage, and the classic passive treble/middle/bass tone stack. The tone stack is computed from the circuit (the Yeh & Smith analog model) in double precision. After it come an EL34-style power stage with Presence, and a 4×12 with an upper-mid bite.
- **Plate:** a small eight-line feedback-delay-network reverb, for that big early-'80s room.

The default EQ (Bass 9, Middle 1, Treble 3, Presence 7) was found by searching against the measured tonal balance of a reference recording's rhythm guitars. It lands within about 1.9 dB on average, from 125 Hz to 6.3 kHz. `LightTest --fit` reruns that search.

## Controls

| Section | Controls |
| --- | --- |
| **Gate** | Threshold (all the way down = off), plus an open/closed light |
| **Overdrive** | ON, Drive, Tone, Level |
| **Amp** | Preamp, Master, AMP + CAB on/off |
| **EQ** | Bass, Middle, Treble, Presence (0–10) |
| **Plate** | Reverb amount |
| **Out** | Output, and INIT to reset everything |

Knobs: double-click or Alt-click resets a knob, Ctrl/Shift-drag gives fine control, and you can click a value to type one in.

Formats: VST3 and Standalone (Windows 64-bit). Stereo output (from the reverb), mono or stereo input.

## Installing

Download the latest release from the [Releases](../../releases) page. Use the installer, or copy `MFR-Lightning.vst3` into `C:\Program Files\Common Files\VST3\` yourself, then rescan plugins in your DAW.

## Building from source

Requirements: CMake 3.22+ and Visual Studio 2022 with the C++ desktop workload. JUCE 8 is included as a git submodule.

```
git clone --recursive https://github.com/music-for-robots/MFR-Lightning.git
cd MFR-Lightning
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin ends up in `build/Lightning_artefacts/Release/VST3/`. There are two offline tools:

- **`LightTest`** checks the tone stack, the overdrive's mid hump, voicing against the reference curve, held-chord stability (8-second chords must decay smoothly), the gate and the output level. It fails on regressions.
- **`LightSnapshot`** renders the UI to a PNG.

To package it locally, run `scripts/package.ps1`. It needs [Inno Setup 6](https://jrsoftware.org/isinfo.php).

## Releasing

1. Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Add a section for the new version to `CHANGELOG.md`.
3. Commit, then tag and push, e.g. `git tag v1.0.1 && git push origin v1.0.1`.

GitHub Actions then builds the plugin, runs `LightTest` and [pluginval](https://github.com/Tracktion/pluginval) (strictness 10), and publishes a release with the zip and the installer.

## Source layout

| Path | What's in it |
| --- | --- |
| `Source/LightningDSP.h` | Gate, overdrive, head, tone stack, cab and plate (plain C++) |
| `Source/PluginProcessor.*` | Parameters, 4× oversampling, stereo output |
| `Source/PluginEditor.*` | User interface |
| `tools/` | `LightTest` and `LightSnapshot` |
| `installer/`, `scripts/package.ps1` | Installer script and packaging |
| `.github/workflows/build.yml` | CI build, validation and releases |

## License

MFR-Lightning is licensed under the [GNU AGPLv3](LICENSE) (or any later version). It is built with [JUCE](https://juce.com), which is used under the AGPLv3, and with the Steinberg VST3 SDK. VST is a trademark of Steinberg Media Technologies GmbH. MFR-Lightning is an independent project.
