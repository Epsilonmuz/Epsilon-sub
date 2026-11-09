# Epsilon 808

**The 808 that's heard everywhere and never fights your kick.** A mono sub-bass instrument (VST3 / AU) by Epsilon Sounds, with the producer chain built in: punch, glide, four anti-aliased drive flavours with **Split** (clean sub under the grit), a filter, **Translate** (harmonics so the 808 is heard on phones), one-knob **Glue**, mono-safe **Width**, a clean **Clip**, **Kick Lock** (phase-aligns and ducks the 808 against your kick) and **Phone Check** with four device curves.

![Epsilon 808, Simple view](docs/epsilon-808-simple.png)

Version 1.0.1. Built with JUCE 9.0.5, C++17. macOS 12+ (universal), Windows (VS2022). Engine tests, plugin bench and pluginval (strictness 8, with editor tests) pass on Linux. The release gate is a Mac build and an FL Studio session.

## What's new in 1.0

| Feature | What it does |
|---|---|
| **Kick Lock** | Route your kick into the sidechain input. On every kick the 808 restarts its cycle in phase with the kick, so the two add up instead of cancelling, and ducks for the **Duck** time so the kick's front comes through. **Exact** mode looks 5 ms ahead and lines up perfectly (the delay is reported to your DAW, which compensates it). **Live** mode adds no delay, for playing live. |
| **Split** | Below the Split frequency you hear the clean 808, above it the driven one. Under +30 dB of Hard drive, the 44 Hz sub stays within 0.1 dB of the clean 808. |
| **Cleaner drive and clip** | Every drive shape is anti-aliased (ADAA at 4x). The output clipper runs at 2x with ADAA and a gentle guard knee. Measured with +36 dB drive pushed +6 dB into Clip: aliasing is about −90 dB in C1–C3 and −70 dB at C4, 26 to 33 dB cleaner than v0.9 at C4. Peaks stay under 0 dBFS. |
| **Phone Check devices** | Phone, Laptop, Earbuds and Car (approximate curves). Car is the opposite test: does the sub get too big? |
| **Analog** | Pitch droops up to 30 cents as the tail fades, each note sits a few cents apart, the attack is rounder. 0% = exact. |
| **Playing** | Velocity to Punch and to Drive; note priority (Last, Low, High); Retrigger (fresh punch on every slide). |
| **Presets** | 41 factory presets in 9 categories (Trap, Drill, Clean, Distorted, Short, Melodic, Log Drum, Wide, Mix Ready) plus your own, each with a one-line description. All factory presets are loudness-matched (−13.5 LUFS ±0.4 on a test phrase), so switching compares sounds, not levels. Save, search, favourite, delete, click-to-audition. |
| **Workflow** | Undo / redo (Cmd/Ctrl+Z), short-term loudness meter (LUFS), keyboard velocity by click position, computer-keyboard playing as a toggle (off by default in a DAW). |

![Advanced view](docs/epsilon-808-advanced.png)

## Controls

- **Body.** Tune, Fine, Punch (how far above the note each hit starts), Punch Time, Tone (2nd and 3rd harmonics), Sub (octave down), Click, Analog.
- **Envelope.** Attack, Length (time to fall 60 dB), Sustain, Release, Glide, Bend (wheel range).
- **Effects.** Drive with Tape / Tube / Hard / Fold, Drive Mix, Split, Cutoff, Resonance, Translate, Glue, Width, Output.
- **Playing.** Vel > Punch, Vel > Drive, Note Priority, Retrigger, Glide mode (Legato / Always / Off).
- **Kick Lock.** On/off pill (it says KICK LOCK: NO KICK until a kick arrives on the sidechain), Mode (Exact / Live), Duck.
- **Monitoring.** Phone Check with the device menu (never saved in presets), A/B compare, undo/redo.

**Displays.** The speaker cone moves with the low end, glows with the level, and shows a teal ring when Translate adds harmonics. Under it: note, frequency and cents off the nearest note. The **Tail** view shows four seconds of level and pitch, with teal ticks where Kick Lock caught a kick and shading where it ducked; above it: PEAK, LOUD (short-term LUFS) and GLUE gain reduction.

## Presets

![Preset browser](docs/epsilon-808-presets.png)

**1.0.1:** every factory preset was re-voiced and measured on the same 4-bar trap phrase. Distorted presets now keep a clean sub with Split, the long and melodic ones carry enough Translate to be followed on a phone (every non-Clean preset stays within 19.5 dB of its full-range loudness through Phone Check), and every preset lands at −13.5 LUFS (gated, BS.1770) within ±0.4 LU, so the loudest one no longer wins by default. The measurements are in `docs/preset-audit.txt`.

Click the preset name to open the browser. Click a preset to load it (with Audition on, it plays a short G1 so you hear it straight away); double-click to load and close; click the star to favourite. **Save** writes the current sound as a file in `~/Music/Epsilon Sounds/808/Presets/` (Mac) or `Music\Epsilon Sounds\808\Presets\` (Windows), so presets move between computers and DAWs. Factory presets are also exposed to the host as programs.

## Setting up Kick Lock

1. Turn on Kick Lock in the plugin.
2. Send your kick track to Epsilon 808's sidechain input (the input bus is called **Kick**):
   - **Ableton Live:** in Epsilon 808's device title bar, open the sidechain section and pick the kick track.
   - **Logic Pro:** choose the kick track in the plugin window's Side Chain menu.
   - **FL Studio:** route the kick's mixer track to the 808's mixer track with "Sidechain to this track", then pick it in the plugin's input settings. Confirm this in the release check: FL Studio's handling of instrument sidechain inputs is the one route not yet verified.
3. The pill changes from KICK LOCK: NO KICK to KICK LOCK ON and flashes on each kick.

## Tips from the designer

1. **Tune first.** Play your song's root and watch the cents readout, then set Tune.
2. **Phone Check, then Translate.** Switch Phone Check on and raise Translate until you can follow the bassline (30–50% is typical). Then try Car: if the sub booms, lower Sub or Length.
3. **Heavy distortion without losing weight:** Drive +20 dB or more, Split at 90–120 Hz. Drive Mix can stay at 100%.
4. **808 and kick together:** turn on Kick Lock with Duck 40–80 ms. With a separate kick, you can usually remove the sidechain compressor you used before.
5. **Drill:** Glide mode Always, Glide 180–260 ms, Note Priority Low, Retrigger on so every slide still knocks.
6. **Log drums:** start from the Log Drum presets and play from C1; Tune puts them an octave up.
7. **Accents:** Vel > Punch 40–60% makes programmed velocities matter.

## Try it in the browser

`docs/epsilon-808-lab.html` runs the same engine in Web Audio over a four-bar 140 BPM trap beat (synthesised kick, clap and snare, 808-style hats, keys and a room; the kick feeds Kick Lock), all 41 presets and one-click demos for Translate, the drive flavours, glide styles, Kick Lock, Split and punch.

## Build

**Projucer (recommended):** open `Epsilon-808.jucer` and click the Xcode (or Visual Studio) icon, then build the VST3 / AU targets. JUCE 9.0.5 sits in `./JUCE` in the Projucer package. Debug builds are optimised (-O3).

**CMake:**
```
git clone --depth 1 --branch 9.0.5 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target Epsilon808_VST3 Epsilon808_AU
```

**Engine tests** (no JUCE needed): `cmake --build build --target BoomEngineTests && ctest`, or
`g++ -O3 -std=c++17 -ISource Tests/EngineTests.cpp Source/Boom808Engine.cpp && ./a.out`.

CI (`.github/workflows/build.yml`, manual or on `v*` tags) builds universal VST3 + AU on macOS and runs the engine tests, auval (`aumu E808 EpsS`) and pluginval.

## Verification (this version)

- **Engine, 27 checks:** tuning within ±0.002 cents at 44.1/48/96 kHz; punch; glide and glide-back; click-free retrigger; drive and clipper aliasing; Split flat with no drive and sub unchanged under drive; note priority; Retrigger; Analog droop; velocity to Punch; Kick Lock (808-to-kick correlation from −0.97 to +0.95, Exact and Live); Translate; Width mono-safe; Phone Check models; Clip peaks; CPU about 5% of one core with everything on. See `docs/engine-test-results.txt`.
- **Plugin bench:** MIDI to sound, all 41 presets play under 0 dBFS, state and A/B round trips, Kick Lock bus and reported latency (23 samples, or 263 at 48 kHz in Exact mode), user preset save/load/delete, favourites, undo, editor snapshots. See `docs/plugin-bench-results.txt`.
- **pluginval 1.0.4, strictness 8, including editor tests:** SUCCESS. See `docs/pluginval-linux-strictness8.txt`.

## Identity

Manufacturer code `EpsS`, plugin code `E808`, bundle `com.epsilonsounds.e808`, AU type `aumu`, VST3 category Instrument|Synth, sidechain input bus "Kick". Font: Barlow Semi Condensed (SIL OFL, `Resources/Barlow-OFL.txt`).

© Epsilon Sounds.
