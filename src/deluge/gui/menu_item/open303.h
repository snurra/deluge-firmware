#pragma once

#include "gui/menu_item/patched_param/integer.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/submenu.h"
#include "gui/menu_item/value_scaling.h"
#include "gui/ui/sound_editor.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::open303 {
class Menu final : public Submenu {
public:
	using Submenu::Submenu;
	bool isRelevant(ModControllableAudio* modControllable, int32_t) const override {
		return static_cast<Sound*>(modControllable)->getSynthMode() == SynthMode::OPEN303;
	}
};

class EnvMod final : public patched_param::Integer {
public:
	using Integer::Integer;
	void readCurrentValue() override {
		const auto value = soundEditor.currentParamManager->getPatchedParamSet()->getValue(getP());
		setValue(computeCurrentValueForHalfPrecisionMenuItem(std::max<int32_t>(0, value)));
	}
	int32_t getFinalValue() override { return computeFinalValueForHalfPrecisionMenuItem(getValue()); }
};

class Waveform final : public Selection {
public:
	using Selection::Selection;
	void readCurrentValue() override {
		setValue(soundEditor.currentSound->sources[0].oscType == OscType::SQUARE ? 1 : 0);
	}
	void writeCurrentValue() override {
		soundEditor.currentSound->sources[0].setOscType(getValue() ? OscType::SQUARE : OscType::SAW);
	}
	deluge::vector<std::string_view> getOptions(OptType) override {
		return {l10n::getView(l10n::String::STRING_FOR_SAW), l10n::getView(l10n::String::STRING_FOR_SQUARE)};
	}
};
} // namespace deluge::gui::menu_item::open303
