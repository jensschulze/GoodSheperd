#include "plugin.hpp"

struct Feeder : Module {
	enum ParamId {
		ENUMS(KEY_PARAM, 12),
		INVERSION_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PITCH_INPUT,
		FUNCTION_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(PITCH_OUTPUT, 5),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(FUNCTION_LIGHT, 7),
		ENUMS(KEY_LIGHT,12),
		LIGHTS_LEN
	};

	struct ExtendedNoteData {
		int noteNumber;
		int octave;

		ExtendedNoteData(int n, int o) {
			noteNumber = n;
			octave = o;
		}

		float cv() {
			return (noteNumber / 12.0) + octave;
		}
	};

	std::string noteNames[12] = {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B"};
	int functionNoteMapping[12] = {0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9, 11};
	int functionLightMapping[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
	int diatonicIndicesMapping[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
	int diatonicMapping[14] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23};

	dsp::ClockDivider lightDivider;

	Feeder() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < 12; i++) {
			configParam(KEY_PARAM + i, 0.f, 1.f, 0.f, noteNames[i]);
		}

		configParam(INVERSION_PARAM, 0.f, 6.f, 0.f, "Inversion");

		configInput(PITCH_INPUT, "Key CV");
		configInput(FUNCTION_INPUT, "Function CV");

		for (int i = 0; i < 4; i++) {
			configOutput(PITCH_OUTPUT + i, std::to_string(i + 1));
		}

		configOutput(PITCH_OUTPUT + 4, "Polyphonic");
	}

	int getNoteNumber(float pitch) {
		int spreaded = std::floor(pitch * 12);
		int noteNumber = eucMod(spreaded, 12);

		return noteNumber;
	}

	int diatonicAdd(int noteNumber, int interval) {
		int index = diatonicIndicesMapping[noteNumber] + interval;
		int summedNoteNumber = diatonicMapping[index % 14];

		return summedNoteNumber;
	}

	//void setOutput() {
	//}

	void process(const ProcessArgs& args) override {
		float functionCv = inputs[FUNCTION_INPUT].getVoltage();
		int functionNoteNumber = getNoteNumber(functionCv);
		int functionLight = functionLightMapping[functionNoteNumber];
		int functionNote[4] = {};

		float pitch;
		if (inputs[PITCH_INPUT].isConnected()) {
			pitch = inputs[PITCH_INPUT].getVoltage();
		} else {
			pitch = 2.0; // TODO
		}

		for (int i = 0; i < 7; i++) {
			lights[FUNCTION_LIGHT + i].setSmoothBrightness(i == functionLight ? 0.7 : 0.0, args.sampleTime * lightDivider.getDivision());
		}

		functionNote[0] = functionNoteMapping[functionNoteNumber];
		functionNote[1] = diatonicAdd(functionNote[0], 2);
		functionNote[2] = diatonicAdd(functionNote[0], 4);
		functionNote[3] = diatonicAdd(functionNote[0], 6);

		for (int i = 0; i < 4; i++) {
			outputs[PITCH_OUTPUT + i].setVoltage(pitch + functionNote[i] / 12.0); // monophonic outs
			outputs[PITCH_OUTPUT + 4].setVoltage(pitch + functionNote[i] / 12.0, i); // polyphonic out
		}

		outputs[PITCH_OUTPUT + 4].setChannels(4);
	}
};


struct FeederWidget : ModuleWidget {
	struct RoundHugeBlackSnapKnob : RoundHugeBlackKnob {
		RoundHugeBlackSnapKnob() {
			snap = true;
		}
	};

	FeederWidget(Feeder* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Feeder.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<LEDButton>(mm2px(Vec(41.5, 64.0)), module, Feeder::KEY_PARAM + 1));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(41.5, 64.0)), module, Feeder::KEY_LIGHT + 1));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(56.5, 64.0)), module, Feeder::KEY_PARAM + 3));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(56.5, 64.0)), module, Feeder::KEY_LIGHT + 3));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(86.5, 64.0)), module, Feeder::KEY_PARAM + 6));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(86.5, 64.0)), module, Feeder::KEY_LIGHT + 6));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(101.5, 64.0)), module, Feeder::KEY_PARAM + 8));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(101.5, 64.0)), module, Feeder::KEY_LIGHT + 8));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(116.5, 64.0)), module, Feeder::KEY_PARAM + 10));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(116.5, 64.0)), module, Feeder::KEY_LIGHT + 10));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(34.0, 79.0)), module, Feeder::KEY_PARAM + 0));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(34.0, 79.0)), module, Feeder::KEY_LIGHT + 0));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(49.0, 79.0)), module, Feeder::KEY_PARAM + 2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(49.0, 79.0)), module, Feeder::KEY_LIGHT + 2));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(64.0, 79.0)), module, Feeder::KEY_PARAM + 4));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(64.0, 79.0)), module, Feeder::KEY_LIGHT + 4));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(79.0, 79.0)), module, Feeder::KEY_PARAM + 5));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(79.0, 79.0)), module, Feeder::KEY_LIGHT + 5));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(94.0, 79.0)), module, Feeder::KEY_PARAM + 7));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(94.0, 79.0)), module, Feeder::KEY_LIGHT + 7));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(109.0, 79.0)), module, Feeder::KEY_PARAM + 9));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(109.0, 79.0)), module, Feeder::KEY_LIGHT + 9));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(124.0, 79.0)), module, Feeder::KEY_PARAM + 11));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(124.0, 79.0)), module, Feeder::KEY_LIGHT + 11));

		addParam(createParamCentered<RoundHugeBlackSnapKnob>(mm2px(Vec(124.0, 32.5)), module, Feeder::INVERSION_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.0, 71.5)), module, Feeder::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.0, 99.0)), module, Feeder::FUNCTION_INPUT));

		float pitchOutputY = 106.0;
		for (int i = 0; i < 5; i++) {
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(148.0, pitchOutputY)), module, Feeder::PITCH_OUTPUT + i));
			pitchOutputY -= 21.0;
		}

        float functionY = 34.0;
        for (int i = 0; i < 7; i++) {
			addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(functionY, 99.0)), module, Feeder::FUNCTION_LIGHT + i));
			functionY += 15.0;
		}
	}
};

Model* modelFeeder = createModel<Feeder, FeederWidget>("Feeder");