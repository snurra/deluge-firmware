/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "gui/menu_item/integer.h"
#include "gui/menu_item/menu_item_with_cc_learning.h"
#include "gui/menu_item/param.h"
#include "gui/ui/sound_editor.h"
#include "gui/views/automation_view.h"
#include "gui/views/view.h"
#include "model/instrument/kit.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include "model/model_stack.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "open303/drive_control.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_drum.h"
#include <cmath>
#include <cstdint>

namespace deluge::gui::menu_item::fx {

class Clipping final : public IntegerWithOff, public Param, public MenuItemWithCCLearning {
public:
	explicit Clipping(l10n::String name)
	    : IntegerWithOff(name), Param(deluge::modulation::params::UNPATCHED_OPEN303_DRIVE) {}

	void readCurrentValue() override {
		if (isOpen303()) {
			char memory[MODEL_STACK_MAX_SIZE];
			auto* modelStack = getModelStack(memory);
			setValue(static_cast<int32_t>(std::lround(deluge::dsp::open303::driveAmountFromValue(
			    modelStack->autoParam->getCurrentValue(), soundEditor.currentSound->clippingAmount))));
		}
		else {
			setValue(soundEditor.currentModControllable->clippingAmount);
		}
	}
	bool usesAffectEntire() override { return true; }

	void writeCurrentValue() override {
		const int32_t value = getValue();
		if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {
			for (Drum* drum = getCurrentKit()->firstDrum; drum; drum = drum->next) {
				if (drum->type != DrumType::SOUND)
					continue;
				auto* sound = static_cast<SoundDrum*>(drum);
				if (sound->getSynthMode() == SynthMode::OPEN303) {
					char memory[MODEL_STACK_MAX_SIZE];
					auto* modelStack = getModelStackFromSoundDrum(memory, sound)->getUnpatchedAutoParamFromId(getP());
					writeDrive(modelStack, value);
				}
				else {
					sound->clippingAmount = value;
				}
			}
		}
		else if (isOpen303()) {
			char memory[MODEL_STACK_MAX_SIZE];
			writeDrive(getModelStack(memory), value);
		}
		else {
			soundEditor.currentModControllable->clippingAmount = value;
		}

		if (isOpen303()) {
			char memory[MODEL_STACK_MAX_SIZE];
			auto* modelStack = getModelStack(memory);
			const int32_t knobPos =
			    modelStack->paramCollection->paramValueToKnobPos(modelStack->autoParam->getCurrentValue(), modelStack);
			view.sendMidiFollowFeedback(modelStack, knobPos);
			if (getRootUI() == &automationView)
				automationView.possiblyRefreshAutomationEditorGrid(getCurrentClip(), getParamKind(), getP());
		}
	}

	[[nodiscard]] int32_t getMaxValue() const override { return 15; }
	[[nodiscard]] int32_t getMinValue() const override { return 0; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return BAR; }

	deluge::modulation::params::Kind getParamKind() override {
		return isOpen303() ? deluge::modulation::params::Kind::UNPATCHED_SOUND : deluge::modulation::params::Kind::NONE;
	}
	uint32_t getParamIndex() override { return isOpen303() ? getP() : 255; }
	ModelStackWithAutoParam* getModelStack(void* memory) override {
		return isOpen303() ? soundEditor.getCurrentModelStack(memory)->getUnpatchedAutoParamFromId(getP()) : nullptr;
	}
	MenuItem* selectButtonPress() override {
		return isOpen303() ? Param::selectButtonPress() : IntegerWithOff::selectButtonPress();
	}
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override {
		return isOpen303() ? Param::buttonAction(b, on, inCardRoutine)
		                   : IntegerWithOff::buttonAction(b, on, inCardRoutine);
	}
	void horizontalEncoderAction(int32_t offset) override {
		if (isOpen303())
			Param::horizontalEncoderAction(offset);
	}
	bool allowsLearnMode() override { return isOpen303(); }
	ParamDescriptor getLearningThing() override {
		ParamDescriptor descriptor;
		descriptor.setToHaveParamOnly(deluge::modulation::params::UNPATCHED_START + getP());
		return descriptor;
	}
	void learnKnob(MIDICable* cable, int32_t whichKnob, int32_t modKnobMode, int32_t midiChannel) override {
		if (isOpen303())
			MenuItemWithCCLearning::learnKnob(cable, whichKnob, modKnobMode, midiChannel);
	}
	void unlearnAction() override {
		if (isOpen303())
			MenuItemWithCCLearning::unlearnAction();
	}

protected:
	ParamSet* getParamSet() override { return soundEditor.currentParamManager->getUnpatchedParamSet(); }

private:
	static bool isOpen303() {
		return soundEditor.currentSound && soundEditor.currentModControllable == soundEditor.currentSound
		       && soundEditor.currentSound->getSynthMode() == SynthMode::OPEN303;
	}
	static void writeDrive(ModelStackWithAutoParam* modelStack, int32_t amount) {
		modelStack->autoParam->setCurrentValueInResponseToUserInput(deluge::dsp::open303::driveValueFromAmount(amount),
		                                                            modelStack);
	}
};

} // namespace deluge::gui::menu_item::fx
