#include "plugin.hpp"
#include "ResreverCore.hpp"

struct Resrever : Module
{
	enum ParamId
	{
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_INPUT,
		REVERSE_INPUT,
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
	dsp::SchmittTrigger reverseTrigger;

	Resrever() : core(10.0f, APP->engine->getSampleRate())
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(AUDIO_INPUT, "Audio Input");
		configInput(REVERSE_INPUT, "Reverse Gate");
		configOutput(AUDIO_OUTPUT, "Audio Output");
	}

	void onSampleRateChange() override
	{
		core.setSampleRate(APP->engine->getSampleRate());
	}

	void process(const ProcessArgs &args) override
	{
		float input = inputs[AUDIO_INPUT].getVoltage();

		// Process reverse gate with Schmitt trigger (0.1V low, 2.0V high)
		reverseTrigger.process(inputs[REVERSE_INPUT].getVoltage(), 0.1f, 2.0f);
		bool reverseState = reverseTrigger.isHigh();

		float output = core.process(input, reverseState);
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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 36.0)), module, Resrever::REVERSE_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)), module, Resrever::AUDIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Resrever::AUDIO_OUTPUT));
	}
};

Model *modelResrever = createModel<Resrever, ResreverWidget>("Resrever");
