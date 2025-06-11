#include "plugin.hpp"
#include "MillerPuckettePitchShifter.h"

struct Octo : Module
{
	enum ParamId
	{
		PARAM_0_PARAM,
		PARAM_1_PARAM,
		PARAM_2_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_IN_0_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		AUDIO_OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		LIGHTS_LEN
	};

	MillerPuckettePitchShifter *pitchShifter = nullptr;

	Octo()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(PARAM_0_PARAM, 0.25f, 4.f, 2.f, "Speed Change");
		configParam(PARAM_1_PARAM, 512.f, 2048.f, 512.f, "Window Length");
		configParam(PARAM_2_PARAM, 0.f, 1.f, 0.f, "");
		configInput(AUDIO_IN_0_INPUT, "Audio In");
		configOutput(AUDIO_OUT_OUTPUT, "Audio Out");

		float sampleRate = APP->engine->getSampleRate();
		pitchShifter = new MillerPuckettePitchShifter(
			512,
			2.0f,
			sampleRate,
			16);
	}

	void onAdd() override
	{
	}

	void process(const ProcessArgs &args) override
	{
		float audioIn0 = inputs[AUDIO_IN_0_INPUT].getVoltage();

		size_t windowLength = (size_t)(params[PARAM_1_PARAM].getValue());
		pitchShifter->updateWindowLength(windowLength);

		float speedParam = params[PARAM_0_PARAM].getValue();
		pitchShifter->updateSpeedChange(speedParam);

		// Process audio through pitch shifter
		float processed = pitchShifter->process(audioIn0);
		outputs[AUDIO_OUT_OUTPUT].setVoltage(processed);
	}
};

struct OctoWidget : ModuleWidget
{
	OctoWidget(Octo *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Octo.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 39.0)), module, Octo::PARAM_0_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 54.0)), module, Octo::PARAM_1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 69.0)), module, Octo::PARAM_2_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 24.0)), module, Octo::AUDIO_IN_0_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Octo::AUDIO_OUT_OUTPUT));
	}
};

Model *modelOcto = createModel<Octo, OctoWidget>("Octo");
