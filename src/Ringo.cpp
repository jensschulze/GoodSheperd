#include "plugin.hpp"


struct Ringo : Module {
	enum ParamId {
		OFFSET_PARAM,
		SCALE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		AUDIO_IN_0_INPUT,
		ADUIO_IN_1_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	static const int oversamplingFactor = 4;
	dsp::Upsampler<oversamplingFactor, 6> audioIn0Src;
	dsp::Upsampler<oversamplingFactor, 6> audioIn1Src;
	dsp::Decimator<oversamplingFactor, 6> audioOutSrc;

	Ringo() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(AUDIO_IN_0_INPUT, "Audio In 1");
		configInput(ADUIO_IN_1_INPUT, "Audio In 2");
		configParam(SCALE_PARAM, -1.f, 1.f, .2f, "Scale Audio 2");
		configParam(OFFSET_PARAM, 0.f, 1.f, 0.f, "Offset Audio 2");
		configOutput(AUDIO_OUT_OUTPUT, "Audio Out");
	}

	void process(const ProcessArgs& args) override {
		float audioIn0 = inputs[AUDIO_IN_0_INPUT].getVoltage();
		float audioIn1 = inputs[ADUIO_IN_1_INPUT].getVoltage();
		float scale = params[SCALE_PARAM].getValue();
		float offset = params[OFFSET_PARAM].getValue();

		float bufferIn0[oversamplingFactor];
		float bufferIn1[oversamplingFactor];
		audioIn0Src.process(audioIn0, bufferIn0);
		audioIn1Src.process(audioIn1, bufferIn1);

		float bufferOut[oversamplingFactor];
		for (int i = 0; i < oversamplingFactor; i++) {
			bufferOut[i] = bufferIn0[i] * (bufferIn1[i] * scale + offset);
		}

		float audioOut = audioOutSrc.process(bufferOut);
		outputs[AUDIO_OUT_OUTPUT].setVoltage(audioOut);
	}
};


struct RingoWidget : ModuleWidget {
	RingoWidget(Ringo* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ringo.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 54.0)), module, Ringo::SCALE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 69.0)), module, Ringo::OFFSET_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 24.0)), module, Ringo::AUDIO_IN_0_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 39.0)), module, Ringo::ADUIO_IN_1_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Ringo::AUDIO_OUT_OUTPUT));
	}
};


Model* modelRingo = createModel<Ringo, RingoWidget>("Ringo");