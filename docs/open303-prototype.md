# Open303 prototype

This branch adds a monophonic `303` synth mode using Robin Schmidt's Open303 DSP.
It is an experimental build. The first prototype booted and played on the user's
numeric Deluge; the user also confirmed slide worked, with no obvious playback
issues. This is a limited manual smoke test, not a hardware-validated release.
The bootloader, flash writer and hardware drivers are unchanged.

## Controls

In a synth's sound editor, select **Master → Synth Mode → 303**. A **303** submenu
then exposes waveform, frequency, resonance, envelope amount, decay, accent and
Saturation.
Numeric-display synth controls use the existing 0–50 scale; Saturation uses OFF–15.

| Control | Range | Stored Deluge parameter |
| --- | --- | --- |
| Waveform | Saw / square | Oscillator A type |
| Frequency | 100 Hz–10 kHz, exponential | LPF frequency |
| Resonance | 0–100% | LPF resonance |
| Envelope amount | 0–100% | Oscillator A pulse width |
| Decay | 200–2000 ms, exponential | Envelope 1 decay |
| Accent | 0–100% | Envelope 1 sustain |

For an initial acid patch, try frequency 20, resonance 40, envelope amount 33,
decay 18 and accent 40. Both normal filter knobs and the 303 submenu change the
same cutoff/resonance values. These values reuse existing storage and automation;
the mode is serialized as `mode="open303"`.

With standard knob assignments, 303 mode uses these gold-knob pairs:

| Mod button | Upper gold knob | Lower gold knob |
| --- | --- | --- |
| Cutoff / Resonance | 303 cutoff | 303 resonance |
| Custom 1 | Envelope amount | Decay |
| Custom 2 | Accent amount | Drive (Saturation) |
| Custom 3 | Sample-rate reduction | Bitcrushing |

Level/pan, delay and reverb retain their existing controls. Pressing either
303 filter knob displays `303`; it does not change filter type or switch to an
inactive HPF. Parameter names in popups, automation and relevant sound-editor
menus reflect their 303 function (`ENv` for envelope amount and `ACnt` for accent
on the numeric display).

Matching standard Custom 1 and Custom 2 pairs are converted automatically.
Other learned assignments are preserved. Conversion also happens when loading
earlier 303 presets: the former Attack/Release envelope-amount/decay pair is
restored to Attack/Release, and the former Stutter/Accent pair becomes Accent/Drive.
Leaving 303 mode restores matching custom pairs to their native assignments.
The native Attack/Release controls do not shape the 303's own amplitude envelope.
Envelope amount, decay, accent and 303 Saturation remain learnable and automatable.

The envelope-amount menu now uses its full 0–50 range, matching the gold knob.
Earlier builds incorrectly used only the upper half of the menu range. Saved
parameter values and their sound are unchanged, but the displayed number may
be lower: for example, an old envelope-amount value of 25 now displays 0.
Negative values in old presets are displayed as zero, matching the DSP.

Velocity **100 or higher** enables accent. Overlapping note-ons produce slide;
note-off followed by note-on retriggers the envelope. Releasing the newest held
note returns to the previous held pitch. The engine is monophonic regardless of
the normal polyphony and unison settings.

Output passes through Deluge's panning, effects, reverb send and master volume.
Each sounding engine occupies a normal Deluge voice so voice culling, stop,
mode changes and song teardown can silence it. The engine's own envelopes and
filter replace the normal oscillator/envelope/filter chain for this mode.

## Output-level calibration

The level-corrected build scales the engine to Deluge's voice-buffer level before
the output guard. Earlier prototypes clipped the raw engine at ±4 before conversion;
bright notes could exceed that even with Saturation OFF. The conversion coefficient
is now 1/128 of the earlier coefficient (42.1 dB lower for unclipped samples), with
the guard applied after scaling. The distortion circuit's input gain and drive
character are unchanged.

With Saturation OFF, saw waveform, 10 kHz cutoff, zero resonance/envelope amount
and velocity 64, desktop RMS output at sounding MIDI notes 48 and 60 is within
0.4 dB of a native single-saw reference. That reference uses one oscillator,
filter bypass, full sustain and default local volume. These are voice-buffer
measurements before shared master/effects gain, not hardware output levels or a
complete factory-preset comparison. Normal differences in filter response,
envelopes and accent remain.

Bright accented notes preserve their waveform through output conversion.
The desktop checks cover both waveforms, pitch and control extremes, and drive
settings without hitting the conversion guard. Saved 303 patches will play
substantially quieter than on earlier prototypes. Compare the revised build with
a native patch at the same master level before rebalancing a song.

## Drive through the existing Saturation control

Use **Distortion → Saturation**, **303 → Saturation**, or the existing grid shortcut. In 303 mode,
this controls post-voice drive before the Deluge's panning and effects:

- **OFF:** the original clean sound, with exact bypass after the transition.
- **1–4:** mild soft clipping; try 3 first.
- **5–11:** more pronounced distortion; try 8.
- **12:** the maximum drive from the first distortion build.
- **13–15:** progressively more drive, reaching 18 dB more pre-gain at 15.

OFF–8 preserve the first distortion build's settings. Values 9–15 now drive
harder, so presets saved with those settings will sound more distorted. Set a
previously saved value of 15 to 12 to recover its earlier drive sound.

The menu keeps its OFF–15 range. In 303 mode, Saturation is a dedicated
automatable parameter, saved as `open303Drive` with the clip's sound parameters.
Gold knobs and MIDI controls resolve values between the integer menu settings.
Menu and knob displays use the same 0–15 scale, with zero meaning bypass.

To assign drive to another gold knob:

1. Select the mod-button bank whose knob you want to assign.
2. Open **303 → Saturation** or **Distortion → Saturation**.
3. Hold **LEARN** and turn the chosen gold knob, then release LEARN.
4. Exit the menu and use the knob. Record its movements with the usual automation
   recording workflow. Save the synth preset to retain the assignment.

You can also learn a MIDI CC from this menu, or press **CLIP** while the menu is
open to edit its automation. Saturation appears in the automation parameter
scroll list for 303 sounds. The standard Custom 2 lower knob already controls drive;
learning is only needed to put it elsewhere.

Earlier 303 presets and songs without `open303Drive` retain their saved
`clippingAmount`. On first access through a control, that value becomes the clip's
drive setting. Explicit OFF stays off even when the legacy field was nonzero.
The same compatibility fallback applies to old clip parameters and kit rows.
Save the song to retain drive automation; preset saves retain the current value
and learned knob assignment. Earlier prototype firmware cannot replay the new
drive parameter or its automation.

Other synth modes and audio clips retain the original non-automatable Saturation
control. Switching synth mode keeps the two settings separate.

The 303 drive uses a cubic soft clipper with first-order antiderivative
antialiasing, 5 ms control smoothing and partial output-gain compensation.
Compensation limits gain growth; it does not guarantee equal perceived loudness
for every patch. Compensation stops increasing above 12, so extra drive keeps
the same clipping ceiling. The drive curve is specific to the 303's output level.
Other synth modes retain their existing saturation processing. Driven output and live
Saturation changes still need to be auditioned on hardware.

## Current limits

- Hardware CPU usage, output level, knob response and stability are unmeasured.
  One engine retains double precision and 4x oversampling; multiple tracks may
  be expensive on the Cortex-A9.
- Local modulation-matrix routes and the ordinary synth envelopes/LFOs do not
  drive the 303 core. Local patching is disabled for this mode. Parameter
  automation and the global effects path are available.
- Master transpose, cents and channel pitch bend are supported. Per-note MPE,
  oscillator detuning and kit-level pitch adjustment are not integrated.
- Slide currently uses overlapping notes. There is no dedicated per-step slide
  toggle or faithful 303-style pattern editor.
- The reused parameter slots mean changing modes changes how those settings
  sound. Older firmware does not recognize this engine. Preserve original
  songs and presets, and use copies for experiments.
- Live waveform/parameter changes, preset/song round-trips, arpeggiator behavior,
  voice culling, MIDI forwarding and detailed display behavior still need hardware checks.

## Desktop verification and listening

With CMake, a C++20 compiler and a build tool installed:

```sh
cmake -S tests/open303 -B build/open303-host -DOPEN303_SANITIZERS=ON
cmake --build build/open303-host
ctest --test-dir build/open303-host --output-on-failure
build/open303-host/open303_checks build/open303-host
```

The last command writes `open303-saw.wav` and `open303-square.wav`: the same dry
acid pattern, including accents, slides, rests and a cutoff sweep. These are
desktop renders of the ported DSP, not recordings from the Deluge or demonstrations
of its final gain staging/effects. The tests cover pitch, accent, legato versus
retrigger, note priority, tail completion, reset, real-time allocations and
bounded output. They do not exercise the complete firmware integration.
It also writes saw and square versions with `-drive-3`, `-drive-8`,
`-drive-12`, `-drive-13` and `-drive-15` suffixes, using the same pattern and
recording gain for comparison.
Drive checks cover added harmonics, increasing distortion, smoothed changes,
exact bypass, reset, release, no real-time allocations and output headroom.
Additional checks cover legacy drive migration, explicit OFF, full knob travel,
fractional drive and smooth block-rate automation. Output checks compare clean
level with the measured native-saw reference and verify that bright accented
peaks remain linear through conversion.

## Firmware build and hardware testing

```sh
./dbt configure
./dbt build release
```

Build output is `build/Release/deluge.bin`. Successful compilation is not proof
that the firmware is ready to flash or perform with.

Before testing a numeric Deluge, establish its bootloader version using the
[official precautions](https://github.com/SynthstromAudible/DelugeFirmware/wiki/Bootloader-precautions-for-%22numeric%22-Deluge-developers).
Do not update the bootloader merely to try this prototype. Keep a full SD-card
backup, use a test card and low external monitoring volume, and retain a known
working firmware. The user installed the first prototype from an SD card after
its format, firmware size and copied-file hash were verified. A copy of that
first, clean build is retained at `build/open303-first-prototype/deluge.bin`.

The preferred development path is USB RAM loading after a known working build
with `ENABLE_SYSEX_LOAD=YES` is installed and the developer setting is enabled.
That requires a separate preparation step; a normal release build does not
enable it. See the [developer tools guide](https://delugecommunity.com/development/deluge/tools/).
RAM loading does not prevent the running firmware from modifying SD-card files.


## Controls-build hardware checks

The revised controls retain the extended drive DSP and use the corrected output
level. Before using existing songs, check a disposable 303 patch:

1. Turn both filter knobs, press each, and turn again. They should continue
   changing cutoff and resonance.
2. Select Custom 1 and check upper envelope amount / lower decay. Compare
   envelope amount 0, 25 and 50 in the menu with the full gold-knob travel.
3. Select Custom 2 and check upper accent / lower drive. Use notes below and above
   velocity 100 to hear the accent amount. Check OFF and several drive settings.
4. Save/reload the test patch and check the knob assignments and sound.
5. Learn a different parameter to a knob, save/reload, and verify it is retained.
6. Change synth mode and check that matching standard knob pairs are restored.

These UI, preset-recall and mode-switch checks require hardware; successful
compilation and the desktop DSP tests do not verify them.

## Assignable-drive hardware checks

1. Load an earlier 303 preset with nonzero Saturation. Check its level before and
   after entering the Saturation menu.
2. Learn Saturation to a gold knob, sweep from OFF to 15, and compare both menu
   routes. Save/reload the preset and verify the assignment and value.
3. Record a drive sweep, replay it, then save/reload the song and replay again.
4. Set an old driven preset to OFF, save/reload, and confirm the drive stays off.
5. If using kits, repeat on a 303 row and check AFFECT ENTIRE with mixed native
   and 303 rows. Native rows should keep their normal Saturation behavior.

The firmware build and desktop tests do not verify physical knob learning,
automation recording, preset/song recall, or CPU usage on the hardware.

## Level-corrected build hardware checks

1. At a fixed master level, compare a native single saw and a 303 with Saturation
   OFF, open cutoff and low resonance. Then check a typical musical pattern.
2. Sweep cutoff/resonance with drive OFF, including notes at velocity 110.
   Check for harsh clipping and unexpected level jumps.
3. Verify Custom 1 and Custom 2 on both a new patch and an earlier 303 preset,
   then save/reload and check the assignments again.
4. Recheck drive from OFF through 15 at the corrected output level.

These checks remain pending on the Deluge.
