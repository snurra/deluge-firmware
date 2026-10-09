#include "engine.h"
#include "drive.h"
#include "dsp/rosic_Open303.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace deluge::dsp::open303 {
namespace {
struct Tables {
	rosic::MipMappedWaveTable saw;
	rosic::MipMappedWaveTable square;
};

std::shared_ptr<Tables> acquireTables() {
	static std::weak_ptr<Tables> shared;
	auto tables = shared.lock();
	if (!tables) {
		tables = std::make_shared<Tables>();
		shared = tables;
	}
	return tables;
}

double bounded(double value, double lo, double hi, double fallback) {
	return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback;
}
double unit(int32_t value) {
	return (static_cast<double>(value) + 2147483648.0) / 4294967295.0;
}
} // namespace

struct Engine::Impl {
	explicit Impl(double sampleRate) : drive(sampleRate) {}
	std::shared_ptr<Tables> tables = acquireTables();
	rosic::Open303 synth{tables->saw, tables->square};
	Drive drive;
	Parameters parameters;
	bool parametersSet = false;
	bool active = false;
	unsigned quietSamples = 0;
	double bend = 0.0;
};

Engine::Engine(double sampleRate) : impl_(std::make_unique<Impl>(bounded(sampleRate, 8000.0, 192000.0, 44100.0))) {
	impl_->synth.setSampleRate(bounded(sampleRate, 8000.0, 192000.0, 44100.0));
	setParameters({});
}
Engine::~Engine() = default;

void Engine::setParameters(const Parameters& input) {
	Parameters p{
	    bounded(input.waveform, 0.0, 1.0, 0.0),      bounded(input.cutoff, 20.0, 20000.0, 700.0),
	    bounded(input.resonance, 0.0, 100.0, 60.0),  bounded(input.envMod, 0.0, 100.0, 50.0),
	    bounded(input.decayMs, 30.0, 3000.0, 400.0), bounded(input.accent, 0.0, 100.0, 50.0),
	};
	auto& s = impl_->synth;
	const auto& old = impl_->parameters;
	if (!impl_->parametersSet || p.waveform != old.waveform)
		s.setWaveform(p.waveform);
	if (!impl_->parametersSet || p.cutoff != old.cutoff)
		s.setCutoff(p.cutoff);
	if (!impl_->parametersSet || p.resonance != old.resonance)
		s.setResonance(p.resonance);
	if (!impl_->parametersSet || p.envMod != old.envMod)
		s.setEnvMod(p.envMod);
	if (!impl_->parametersSet || p.decayMs != old.decayMs)
		s.setDecay(p.decayMs);
	if (!impl_->parametersSet || p.accent != old.accent)
		s.setAccent(p.accent);
	impl_->parameters = p;
	impl_->parametersSet = true;
}

void Engine::setPitchBend(double semitones) {
	semitones = bounded(semitones, -48.0, 48.0, 0.0);
	if (semitones != impl_->bend) {
		impl_->synth.setPitchBend(semitones);
		impl_->bend = semitones;
	}
}

void Engine::setDrive(double amount) {
	impl_->drive.setAmount(amount);
}

void Engine::noteOn(int note, int velocity) {
	if (note < 0 || note > 127)
		return;
	if (velocity <= 0) {
		noteOff(note);
		return;
	}
	if (!impl_->active)
		impl_->drive.reset();
	impl_->synth.noteOn(note, std::min(velocity, 127), 0.0);
	impl_->active = true;
	impl_->quietSamples = 0;
}
void Engine::noteOff(int note) {
	if (note >= 0 && note <= 127)
		impl_->synth.noteOn(note, 0, 0.0);
}
void Engine::allNotesOff() {
	impl_->synth.allNotesOff();
}
void Engine::reset() {
	impl_->synth.resetToSilence();
	impl_->drive.reset();
	impl_->active = false;
	impl_->quietSamples = 0;
}
bool Engine::active() const {
	return impl_->active;
}
bool Engine::gateOn() const {
	return impl_->synth.gateOn();
}

double Engine::renderSample() {
	if (!impl_->active)
		return 0.0;
	double sample = impl_->synth.getSample();
	if (!std::isfinite(sample)) {
		reset();
		return 0.0;
	}
	if (!gateOn() && impl_->synth.ampEnv.endIsReached() && std::abs(sample) < 1.0e-7) {
		if (++impl_->quietSamples >= 256)
			reset();
	}
	else
		impl_->quietSamples = 0;
	const double output = impl_->drive.process(sample);
	if (!std::isfinite(output)) {
		reset();
		return 0.0;
	}
	return output;
}
int32_t Engine::renderQ31() {
	// Match the voice-buffer level of a native single saw at full sustain and
	// default local volume. Scale before limiting so clean accents retain their peaks.
	constexpr double voiceScale = 1048576.0;
	constexpr double voiceLimit = 536870911.0;
	return static_cast<int32_t>(std::clamp(renderSample() * voiceScale, -voiceLimit, voiceLimit));
}
Parameters Engine::fromDeluge(int32_t cutoff, int32_t resonance, int32_t envMod, int32_t decay, int32_t accent,
                              bool square) {
	return {square ? 1.0 : 0.0,
	        100.0 * std::pow(100.0, unit(cutoff)),
	        100.0 * unit(resonance),
	        100.0 * std::clamp(envMod / 2147483647.0, 0.0, 1.0),
	        200.0 * std::pow(10.0, unit(decay)),
	        100.0 * unit(accent)};
}
} // namespace deluge::dsp::open303
