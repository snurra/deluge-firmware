#include "open303/drive.h"
#include "open303/drive_control.h"
#include "open303/engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

static std::size_t allocationCount = 0;
void* operator new(std::size_t n) {
	++allocationCount;
	if (void* p = std::malloc(n ? n : 1))
		return p;
	throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
	return ::operator new(n);
}
void operator delete(void* p) noexcept {
	std::free(p);
}
void operator delete[](void* p) noexcept {
	std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
	std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
	std::free(p);
}

using deluge::dsp::open303::Engine;
using deluge::dsp::open303::Parameters;
constexpr int sampleRate = 44100;

void require(bool condition, const char* message) {
	if (!condition)
		throw std::runtime_error(message);
}
double energy(Engine& engine, int length) {
	double result = 0;
	for (int i = 0; i < length; ++i) {
		const double sample = engine.renderSample();
		require(std::isfinite(sample), "Non-finite audio");
		result += sample * sample;
	}
	return result;
}

void articulation() {
	Engine held, legato, retrigger;
	for (auto* engine : {&held, &legato, &retrigger}) {
		engine->noteOn(48, 80);
		require(energy(*engine, 6000) > 0.01, "Note-on must produce audio");
	}
	legato.noteOn(48, 80);
	retrigger.noteOff(48);
	retrigger.noteOn(48, 80);
	double retriggerDifference = 0;
	for (int i = 0; i < 2000; ++i) {
		const double reference = held.renderSample();
		require(std::abs(reference - legato.renderSample()) < 1e-12,
		        "A same-pitch legato note must preserve oscillator and envelope continuity");
		retriggerDifference += std::abs(reference - retrigger.renderSample());
	}
	require(retriggerDifference > 0.01, "A detached note must retrigger the envelope");
	legato.noteOn(55, 110);
	legato.noteOff(48);
	require(legato.gateOn(), "Releasing an older held note must not release the current note");
	require(energy(legato, 1000) > 0.00001, "Slide must continue sounding");
	legato.noteOff(55);
	energy(legato, sampleRate * 2);
	require(!legato.active() && legato.renderSample() == 0.0, "Note-off must drain the tail and become idle");
	held.allNotesOff();
	energy(held, sampleRate * 2);
	require(!held.active(), "Transport stop must release all held notes");
	retrigger.reset();
	require(!retrigger.gateOn() && retrigger.renderQ31() == 0, "Hard reset must silence immediately");
	retrigger.noteOn(48, 80);
	Engine fresh;
	fresh.noteOn(48, 80);
	for (int i = 0; i < 2000; ++i)
		require(std::abs(retrigger.renderSample() - fresh.renderSample()) < 1e-12,
		        "A reset engine must restart without stale envelope or filter state");
}

void pitchAndAccent() {
	Engine engine;
	Parameters p;
	p.cutoff = 20000;
	p.resonance = 0;
	p.envMod = 0;
	engine.setParameters(p);
	engine.noteOn(69, 80);
	energy(engine, 4410);
	std::array<double, 4096> samples{};
	for (auto& s : samples)
		s = engine.renderSample();
	int bestLag = 0;
	double best = -1;
	for (int lag = 85; lag <= 115; ++lag) {
		double correlation = 0;
		for (int i = 0; i < 3900; ++i)
			correlation += samples[i] * samples[i + lag];
		if (correlation > best) {
			best = correlation;
			bestLag = lag;
		}
	}
	require(std::abs(sampleRate / static_cast<double>(bestLag) - 440.0) < 5.0, "MIDI A4 must be tuned to 440 Hz");
	Engine normal, accented;
	p.cutoff = 700;
	p.resonance = 70;
	p.envMod = 70;
	p.accent = 100;
	normal.setParameters(p);
	accented.setParameters(p);
	normal.noteOn(40, 80);
	accented.noteOn(40, 110);
	require(energy(accented, 6000) > energy(normal, 6000) * 1.1, "Accents must audibly affect the voice");
}

void realtimeAndExtremes() {
	Engine engine;
	const auto before = allocationCount;
	for (int note = 0; note < 128; ++note) {
		engine.setDrive(note % 16);
		engine.noteOn(note, 110);
		for (int n = 0; n < 32; ++n)
			engine.renderQ31();
	}
	for (int note = 0; note < 128; ++note)
		engine.noteOff(note);
	energy(engine, sampleRate);
	engine.reset();
	require(allocationCount == before, "Note handling and rendering must not allocate");
	require(!engine.gateOn(), "A full note stack must release cleanly");
	engine.noteOn(48, 80);
	engine.noteOn(55, 80);
	engine.noteOff(55);
	require(engine.gateOn(), "Releasing the latest note must retain an older held note");
	engine.noteOff(48);
	require(!engine.gateOn(), "Last-note return must not leave a stuck gate");
	for (double wave : {0.0, 1.0})
		for (double cutoff : {20.0, 20000.0})
			for (double resonance : {0.0, 100.0})
				for (int note : {0, 36, 72, 127})
					for (int drive : {0, 3, 8, 12, 15}) {
						engine.reset();
						engine.setDrive(drive);
						engine.setParameters({wave, cutoff, resonance, 100, 2000, 100});
						engine.noteOn(note, 120);
						for (int i = 0; i < 8000; ++i) {
							const auto sample = engine.renderQ31();
							require(std::abs(static_cast<int64_t>(sample)) < 536870911,
							        "Valid control extremes must retain headroom without hitting the conversion guard");
						}
					}
	engine.setParameters({NAN, INFINITY, NAN, NAN, NAN, NAN});
	require(energy(engine, 1000) >= 0, "Invalid controls must not poison DSP state");
	engine.reset();
	engine.setPitchBend(48);
	engine.noteOn(127, 110);
	energy(engine, 1000);
	require(engine.active(), "High notes with pitch bend must not poison the oscillator");
}

void controlRangeAndIsolation() {
	Engine controlled, reference;
	const auto controls = Engine::fromDeluge(INT32_MIN, INT32_MIN, 0, INT32_MIN, INT32_MIN, false);
	controlled.setParameters(controls);
	reference.setParameters({0, 100, 0, 0, 200, 0});
	controlled.noteOn(48, 110);
	reference.noteOn(48, 110);
	for (int i = 0; i < 4000; ++i)
		require(std::abs(controlled.renderSample() - reference.renderSample()) < 1e-12,
		        "Minimum Deluge controls must reach the engine's minimum settings");
	Engine unrelated;
	unrelated.setParameters({1, 20000, 100, 100, 2000, 100});
	unrelated.noteOn(75, 120);
	for (int i = 0; i < 4000; ++i) {
		unrelated.renderSample();
		require(std::abs(controlled.renderSample() - reference.renderSample()) < 1e-12,
		        "A second instance must not change the first instance's shared waveforms or voice state");
	}
}

void driveBehaviour() {
	using deluge::dsp::open303::Drive;
	Drive bypass(sampleRate), mild(sampleRate), hard(sampleRate);
	mild.setAmount(3);
	hard.setAmount(15);
	mild.reset();
	hard.reset();
	std::array<double, 2> fundamental{}, third{};
	const auto before = allocationCount;
	for (int i = 0; i < sampleRate / 5; ++i) {
		const double phase = 2.0 * 3.141592653589793 * 1000 * i / sampleRate;
		const double input = std::sin(phase);
		require(bypass.process(input) == input, "Drive off must preserve the clean sound exactly");
		const std::array<double, 2> outputs{mild.process(input), hard.process(input)};
		// Discard the first 100 cycles so startup state cannot affect the spectrum.
		if (i >= sampleRate / 10) {
			for (int j = 0; j < 2; ++j) {
				fundamental[j] += outputs[j] * std::sin(phase);
				third[j] += outputs[j] * std::sin(3.0 * phase);
			}
		}
	}
	require(std::abs(third[1] / fundamental[1]) > 0.1,
	        "High drive must add audible harmonics, not just increase volume");
	require(std::abs(third[1] / fundamental[1]) > std::abs(third[0] / fundamental[0]) * 3.0,
	        "Drive must progress from mild to stronger distortion");
	Drive previousMaximum(sampleRate), extendedMaximum(sampleRate);
	previousMaximum.setAmount(12);
	extendedMaximum.setAmount(15);
	previousMaximum.reset();
	extendedMaximum.reset();
	fundamental.fill(0.0);
	third.fill(0.0);
	// Quiet material must acquire more harmonics as the upper range is increased.
	for (int i = 0; i < sampleRate / 5; ++i) {
		const double phase = 2.0 * 3.141592653589793 * 1000 * i / sampleRate;
		const double input = 0.12 * std::sin(phase);
		const std::array<double, 2> outputs{previousMaximum.process(input), extendedMaximum.process(input)};
		if (i >= sampleRate / 10) {
			for (int j = 0; j < 2; ++j) {
				fundamental[j] += outputs[j] * std::sin(phase);
				third[j] += outputs[j] * std::sin(3.0 * phase);
			}
		}
	}
	require(std::abs(third[1] / fundamental[1]) > std::abs(third[0] / fundamental[0]) * 2.0,
	        "The upper drive range must add distortion to quiet material, not merely gain");
	for (int i = 0; i < sampleRate / 10; ++i) {
		previousMaximum.process(4.0);
		extendedMaximum.process(4.0);
	}
	require(std::abs(previousMaximum.process(4.0) - extendedMaximum.process(4.0)) < 1.0e-8,
	        "Extra drive must retain the established output ceiling");

	for (int i = 0; i < 1000; ++i)
		bypass.process(0.5);
	bypass.setAmount(15);
	require(std::abs(bypass.process(0.5) - 0.5) < 0.001, "Changing drive must ramp rather than abruptly switch gain");
	for (int i = 0; i < sampleRate; ++i) {
		const double output = bypass.process(0.5);
		require(std::isfinite(output) && std::abs(output) < 4.0,
		        "Drive transitions must remain finite and inside output headroom");
	}
	bypass.setAmount(0);
	for (int i = 0; i < sampleRate / 2; ++i)
		bypass.process(0.5);
	require(bypass.process(0.5) == 0.5, "Turning drive off must settle to exact bypass");
	require(allocationCount == before, "Drive processing and control changes must not allocate");

	// Saved scalar drive must survive migration, including an explicit OFF overriding it.
	using namespace deluge::dsp::open303;
	for (int pos = -64; pos <= 64; ++pos) {
		require(driveKnobPosFromValue(driveValueFromKnobPos(pos), 15) == pos,
		        "Drive knob positions must round-trip without dead steps or resurrecting legacy drive");
	}
	require(driveAmountFromValue(driveValueFromKnobPos(-64), 15) == 0
	            && driveAmountFromValue(driveValueFromKnobPos(64), 0) == 15,
	        "Full knob travel must reach exact bypass and maximum drive");
	for (int amount = 0; amount <= 15; ++amount) {
		Drive legacy(sampleRate), restored(sampleRate);
		legacy.setAmount(amount);
		const auto saved = resolveDriveValue(kLegacyDrive, amount);
		restored.setAmount(driveAmountFromValue(saved, 0));
		legacy.reset();
		restored.reset();
		for (int i = 0; i < 1000; ++i) {
			const double input = 0.25 * std::sin(i * 0.1);
			require(std::abs(legacy.process(input) - restored.process(input)) < 1.0e-7,
			        "Migrating a saved drive setting must preserve its audio");
		}
		restored.setAmount(driveAmountFromValue(driveValueFromAmount(0), amount));
		restored.reset();
		require(restored.process(0.25) == 0.25, "Explicit OFF must override legacy saturation");
	}

	Drive continuous(sampleRate), rounded(sampleRate);
	continuous.setAmount(13.25);
	rounded.setAmount(13.0);
	continuous.reset();
	rounded.reset();
	double fractionalDifference = 0;
	for (int i = 0; i < 1000; ++i) {
		const double input = 0.01 * std::sin(i * 0.1);
		fractionalDifference += std::abs(continuous.process(input) - rounded.process(input));
	}
	require(fractionalDifference > 0.01, "Knob drive must resolve values between integer menu steps");

	continuous.setAmount(0);
	continuous.reset();
	double previous = continuous.process(0.5);
	const auto beforeSweep = allocationCount;
	for (int i = 0; i < sampleRate; ++i) {
		if (i % 64 == 0) {
			const double amount = i < sampleRate / 2 ? 30.0 * i / sampleRate : 30.0 * (sampleRate - i) / sampleRate;
			continuous.setAmount(driveAmountFromValue(driveValueFromAmount(amount), 0));
		}
		const double output = continuous.process(0.5);
		require(std::isfinite(output) && std::abs(output) < 4.0,
		        "Automated drive sweeps must remain finite and bounded");
		require(std::abs(output - previous) < 0.01, "Block-rate drive automation must remain smooth");
		previous = output;
	}
	require(allocationCount == beforeSweep, "Continuous drive automation must not allocate");
	continuous.setAmount(std::numeric_limits<double>::quiet_NaN());
	continuous.reset();
	require(continuous.process(0.5) == 0.5, "Invalid drive controls must fall back to bypass");

	Engine clean, driven, fresh;
	driven.setDrive(15);
	fresh.setDrive(15);
	clean.noteOn(40, 110);
	driven.noteOn(40, 110);
	double difference = 0;
	for (int i = 0; i < 8000; ++i)
		difference += std::abs(clean.renderSample() - driven.renderSample());
	require(difference > 1.0, "The engine must apply its drive setting to rendered audio");
	driven.reset();
	driven.noteOn(40, 110);
	fresh.noteOn(40, 110);
	for (int i = 0; i < 8000; ++i)
		require(driven.renderSample() == fresh.renderSample(), "Reset must clear drive history for a fresh note");
	driven.allNotesOff();
	energy(driven, sampleRate * 2);
	require(!driven.active() && driven.renderQ31() == 0, "Driven notes must release to silence");
}

void outputCalibration() {
	// The native single-saw reference measures -65 dBFS in the voice buffer at
	// these pitches: filter bypass, full sustain, velocity 64, default local volume.
	// Common post-FX/master gain is outside both measurements.
	for (int note : {48, 60}) {
		Engine clean;
		clean.setParameters({0, 10000, 0, 0, 502.377, 100});
		clean.noteOn(note, 64);
		double sumSquares = 0;
		for (int i = 0; i < 22048; ++i) {
			const double sample = clean.renderQ31() / 2147483648.0;
			sumSquares += sample * sample;
		}
		const double rmsDb = 10 * std::log10(sumSquares / 22048);
		require(rmsDb > -68 && rmsDb < -62, "Clean 303 level must match the native saw reference within 3 dB");
	}

	Engine raw, converted;
	const Parameters brightAccent{0, 10000, 0, 0, 502.377, 100};
	raw.setParameters(brightAccent);
	converted.setParameters(brightAccent);
	raw.noteOn(48, 110);
	converted.noteOn(48, 110);
	double gain = 0;
	int largePeaks = 0;
	for (int i = 0; i < 22048; ++i) {
		const double sample = raw.renderSample();
		const int32_t voiceSample = converted.renderQ31();
		if (!gain && std::abs(sample) > 0.5)
			gain = voiceSample / sample;
		if (gain && std::abs(sample) > 4) {
			++largePeaks;
			require(std::abs(voiceSample - sample * gain) < 64,
			        "Clean accented peaks must retain the same gain as quieter samples, without hard clipping");
		}
	}
	require(largePeaks > 1000, "The headroom check must exercise bright accented peaks");
}

void writeWord(std::ofstream& file, uint32_t value, int bytes) {
	for (int i = 0; i < bytes; ++i)
		file.put(static_cast<char>(value >> (8 * i)));
}
void demo(const std::string& path, double waveform, int drive = 0) {
	Engine engine;
	engine.setDrive(drive);
	constexpr int stepLength = 5512;
	constexpr int steps = 64;
	constexpr int count = stepLength * steps + sampleRate;
	std::ofstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("Cannot write demo WAV");
	file.write("RIFF", 4);
	writeWord(file, 36 + count * 2, 4);
	file.write("WAVEfmt ", 8);
	writeWord(file, 16, 4);
	writeWord(file, 1, 2);
	writeWord(file, 1, 2);
	writeWord(file, sampleRate, 4);
	writeWord(file, sampleRate * 2, 4);
	writeWord(file, 2, 2);
	writeWord(file, 16, 2);
	file.write("data", 4);
	writeWord(file, count * 2, 4);
	constexpr int notes[16] = {36, 36, 48, 43, -1, 36, 39, 43, 36, -1, 48, 46, 43, 39, 36, -1};
	int held = -1;
	double peak = 0.0;
	double sumSquares = 0.0;
	for (int i = 0; i < count; ++i) {
		const int step = i / stepLength;
		const int offset = i % stepLength;
		if (step < steps && offset == 0) {
			const int note = notes[step % 16];
			engine.setParameters({waveform, 180.0 * std::pow(20.0, step / 63.0), 80, 65, 450, 80});
			if (note >= 0)
				engine.noteOn(note, step % 4 == 0 ? 115 : 80);
			if (held >= 0 && held != note)
				engine.noteOff(held);
			held = note;
		}
		if (step < steps && offset == stepLength / 2 && step % 4 != 2) {
			if (held >= 0)
				engine.noteOff(held);
			held = -1;
		}
		if (step == steps && offset == 0)
			engine.allNotesOff();
		const double sample = engine.renderSample() * 0.25;
		peak = std::max(peak, std::abs(sample));
		sumSquares += sample * sample;
		require(std::abs(sample) < 1.0, "Demo output must not clip");
		writeWord(file, static_cast<uint16_t>(static_cast<int16_t>(sample * 32767)), 2);
	}
	std::printf("%s: peak %.2f dBFS, RMS %.2f dBFS\n", path.c_str(), 20 * std::log10(peak),
	            10 * std::log10(sumSquares / count));
}

int main(int argc, char** argv) {
	try {
		articulation();
		pitchAndAccent();
		realtimeAndExtremes();
		controlRangeAndIsolation();
		driveBehaviour();
		outputCalibration();
		std::puts("PASS: pitch, accent, legato/retrigger, note priority, release, reset, allocation, drive and output "
		          "bounds and native-level calibration");
		if (argc == 2) {
			demo(std::string(argv[1]) + "/open303-saw.wav", 0.0);
			demo(std::string(argv[1]) + "/open303-square.wav", 1.0);
			for (int drive : {3, 8, 12, 13, 15}) {
				demo(std::string(argv[1]) + "/open303-saw-drive-" + std::to_string(drive) + ".wav", 0.0, drive);
				demo(std::string(argv[1]) + "/open303-square-drive-" + std::to_string(drive) + ".wav", 1.0, drive);
			}
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "FAIL: %s\n", e.what());
		return 1;
	}
}
