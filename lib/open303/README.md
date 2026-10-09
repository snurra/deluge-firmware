# Open303 DSP port

Source: https://github.com/RobinSchmidt/Open303

Revision: `313bf0d9ade7c1dcb6b3a74f5ea1780a29d70074`.
The `dsp/` directory is derived from that revision's `Source/DSPCode/`.
Robin Schmidt's MIT license is preserved in `LICENSE`. This is the original
Open303 engine, not JC-303's JUCE wrapper, overdrive, or additional modifications.

Local adaptations:

- Two waveform tables are shared by engine instances. They are constructed when
  a patch is loaded or the mode selected, never on note-on. The tables stay alive
  until the last engine is destroyed. The adapter does not expose table mutation.
- MIDI note priority uses a fixed 128-key stack, replacing the allocating list.
  Repeated note-ons for a held key do not accumulate duplicate entries.
- `UINT32` has an explicit 32-bit type on both desktop and ARM.
- Prototype-table initialization respects the array length; mip-table selection
  clamps an index equal to the table count as well as larger indices.
- FFT scratch allocations use RAII, including partial-construction failure.
- The sample-rate setter updates the stored sample rate.
- An explicit silence reset clears note, envelope and accent smoothing state.
- Standard headers are included explicitly; vendor files use UTF-8 and LF.

The oscillator, filter, envelope algorithms, double precision, and 4x
oversampling are retained. `engine.cpp` owns control validation, pitch bend,
tail termination and conversion to the Deluge audio format. Voice output is scaled
by 2^20 before its ±(2^29−1) guard, calibrated against the native single-saw voice
level. This avoids hard clipping clean accent peaks before conversion. Its finite-value
checks require IEEE arithmetic, so this target disables fast-math and LTO.
Legacy type-punning also requires `-fno-strict-aliasing`.

`drive.h` adds post-voice cubic soft clipping controlled by the Deluge's existing
Saturation menu (OFF–15), with continuous values for knob and automation sweeps. It uses first-order antiderivative antialiasing,
5 ms control smoothing, partial gain compensation and exact settled bypass.
Pre-gain reaches 30 dB at 12 and 48 dB at 15. Above 12, the output clipping
ceiling stays fixed. This stage is separate from the vendored Open303 algorithms.
`drive_control.h` maps the dedicated unpatched drive parameter to the DSP and
preserves scalar Saturation from earlier presets until the parameter is set.

One engine owns the small note/filter/envelope state. The shared tables and FFT
scratch buffers consume roughly 550 KiB in external RAM. No real-time CPU budget
has been measured on a Deluge yet.
