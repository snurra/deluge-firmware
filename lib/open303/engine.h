#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace deluge::dsp::open303 {

struct Parameters {
	double waveform = 0.0;
	double cutoff = 700.0;
	double resonance = 60.0;
	double envMod = 50.0;
	double decayMs = 400.0;
	double accent = 50.0;
	bool operator==(const Parameters&) const = default;
};

// Construct when loading/selecting a patch. Note events and rendering allocate no memory.
class Engine {
public:
	explicit Engine(double sampleRate = 44100.0);
	~Engine();
	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;
	void setParameters(const Parameters& parameters);
	void setPitchBend(double semitones);
	// Continuous Saturation control: 0 is off, 15 is maximum drive.
	void setDrive(double amount);
	void noteOn(int note, int velocity);
	void noteOff(int note);
	void allNotesOff();
	void reset();
	bool active() const;
	bool gateOn() const;
	double renderSample();
	// Calibrated for Deluge's voice buffer; the final guard leaves 12 dB of mixer headroom.
	int32_t renderQ31();
	static Parameters fromDeluge(int32_t cutoff, int32_t resonance, int32_t envMod, int32_t decay, int32_t accent,
	                             bool square);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace deluge::dsp::open303
