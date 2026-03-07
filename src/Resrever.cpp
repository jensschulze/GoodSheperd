#include "plugin.hpp"
#include "ResreverCore.hpp"

struct Resrever : Module
{
	enum ParamId
	{
		REVERSE_BUTTON_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		LIGHTS_LEN
	};

	ResreverCore core;

	Resrever() : core(4.0f, APP->engine->getSampleRate())
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(REVERSE_BUTTON_PARAM, 0.f, 1.f, 0.f, "Reverse");
		configInput(AUDIO_INPUT, "Audio Input");
		configOutput(AUDIO_OUTPUT, "Audio Output");
	}

	void onSampleRateChange() override
	{
		core.setSampleRate(APP->engine->getSampleRate());
	}

	void process(const ProcessArgs &args) override
	{
		bool reversePressed = params[REVERSE_BUTTON_PARAM].getValue() > 0.5f;
		float input = inputs[AUDIO_INPUT].getVoltage();

		float output = core.process(input, reversePressed);
		outputs[AUDIO_OUTPUT].setVoltage(output);
	}
};

struct ResreverWidget : ModuleWidget
{
	ResreverWidget(Resrever *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Resrever.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(10.16, 36.0)), module, Resrever::REVERSE_BUTTON_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)), module, Resrever::AUDIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Resrever::AUDIO_OUTPUT));
	}
};

Model *modelResrever = createModel<Resrever, ResreverWidget>("Resrever");
