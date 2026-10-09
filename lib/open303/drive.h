#pragma once

#include <algorithm>
#include <cmath>

namespace deluge::dsp::open303 {

// Post-voice soft clipping. First-order antiderivative antialiasing reduces
// folded harmonics without another oversampled oscillator/filter path.
class Drive {
public:
	explicit Drive(double sampleRate) : rampSamples_(std::max(1, static_cast<int>(0.005 * sampleRate))) {}

	void setAmount(double amount) {
		amount = std::isfinite(amount) ? std::clamp(amount, 0.0, 15.0) : 0.0;
		if (amount == amount_)
			return;
		amount_ = amount;
		// Keep the gentle range, reach 30 dB at 12, then add 6 dB per step to 48 dB.
		const double gainDb = amount <= 8    ? 2.0 * amount
		                      : amount <= 12 ? 16.0 + 3.5 * (amount - 8)
		                                     : 30.0 + 6.0 * (amount - 12);
		targetGain_ = std::pow(10.0, gainDb / 20.0);
		// Above 30 dB, retain the clipping ceiling instead of making extra drive quieter.
		targetCompensation_ = std::pow(10.0, -std::min(gainDb, 30.0) / 40.0);
		targetMix_ = std::min(1.0, amount / 4.0);
		// Ramp gain geometrically so a large increase cannot jump at the start.
		gainStep_ = std::pow(targetGain_ / gain_, 1.0 / rampSamples_);
		compensationStep_ = std::pow(targetCompensation_ / compensation_, 1.0 / rampSamples_);
		mixStep_ = (targetMix_ - mix_) / rampSamples_;
		remaining_ = rampSamples_;
	}

	void reset() {
		gain_ = targetGain_;
		compensation_ = targetCompensation_;
		mix_ = targetMix_;
		previousInput_ = 0.0;
		remaining_ = 0;
	}

	double process(double input) {
		if (amount_ == 0 && remaining_ == 0) {
			gain_ = compensation_ = 1.0;
			mix_ = 0.0;
			previousInput_ = input * 0.25;
			return input;
		}
		if (remaining_ > 0) {
			gain_ *= gainStep_;
			compensation_ *= compensationStep_;
			mix_ += mixStep_;
			if (--remaining_ == 0) {
				gain_ = targetGain_;
				compensation_ = targetCompensation_;
				mix_ = targetMix_;
			}
		}
		const double driven = input * 0.25 * gain_;
		const double delta = driven - previousInput_;
		const double shaped = std::abs(delta) > 1.0e-5 ? (integral(driven) - integral(previousInput_)) / delta
		                                               : curve((driven + previousInput_) * 0.5);
		previousInput_ = driven;
		return input + mix_ * (4.0 * shaped * compensation_ - input);
	}

private:
	static double curve(double x) {
		x = std::clamp(x, -1.5, 1.5);
		return x - (4.0 / 27.0) * x * x * x;
	}
	static double integral(double x) {
		x = std::abs(x);
		if (x >= 1.5)
			return x - 0.5625;
		const double square = x * x;
		return 0.5 * square - square * square / 27.0;
	}
	double amount_ = 0;
	int rampSamples_;
	int remaining_ = 0;
	double gainStep_ = 1.0, compensationStep_ = 1.0, mixStep_ = 0.0;
	double targetGain_ = 1.0, gain_ = 1.0;
	double targetCompensation_ = 1.0, compensation_ = 1.0;
	double targetMix_ = 0.0, mix_ = 0.0;
	double previousInput_ = 0.0;
};

} // namespace deluge::dsp::open303
