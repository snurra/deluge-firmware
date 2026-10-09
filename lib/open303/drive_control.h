#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace deluge::dsp::open303 {

// Nonnegative values are explicit drive settings. The sentinel preserves legacy
// per-sound Saturation until a clip acquires its own drive value or automation.
constexpr int32_t kLegacyDrive = std::numeric_limits<int32_t>::min();
constexpr int32_t kMaxDriveValue = std::numeric_limits<int32_t>::max();

inline int32_t driveValueFromAmount(double amount) {
	amount = std::clamp(amount, 0.0, 15.0);
	return static_cast<int32_t>(amount * (kMaxDriveValue / 15.0) + 0.5);
}

inline int32_t resolveDriveValue(int32_t value, int legacyAmount) {
	return value == kLegacyDrive ? driveValueFromAmount(std::clamp(legacyAmount, 0, 15)) : std::max<int32_t>(0, value);
}

inline double driveAmountFromValue(int32_t value, int legacyAmount) {
	return resolveDriveValue(value, legacyAmount) * (15.0 / kMaxDriveValue);
}

inline int32_t driveValueFromKnobPos(int32_t knobPos) {
	return static_cast<int32_t>((static_cast<int64_t>(std::clamp<int32_t>(knobPos, -64, 64) + 64) * kMaxDriveValue)
	                            / 128);
}

inline int32_t driveKnobPosFromValue(int32_t value, int legacyAmount) {
	const auto resolved = resolveDriveValue(value, legacyAmount);
	return static_cast<int32_t>((static_cast<int64_t>(resolved) * 128 + kMaxDriveValue / 2) / kMaxDriveValue) - 64;
}

} // namespace deluge::dsp::open303
