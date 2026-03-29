#include "plugin.hpp"
#include "ResreverCore.hpp"

struct Resrever : Module
{
	enum ParamId
	{
		MIX_PARAM,
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
		// GreenRedLight uses two slots: green at index, red at index+1
		ENUMS(REVERSE_LIGHT, 2),
		LIGHTS_LEN
	};

	ResreverCore core;
	dsp::SchmittTrigger reverseTrigger;
	dsp::ClockDivider lightDivider;

	Resrever() : core(10.0f, APP->engine->getSampleRate())
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// 0 = fully reversed (wet), 1 = fully dry
		configParam(MIX_PARAM, 0.0f, 1.0f, 0.0f, "Mix", "", 0.0f, 1.0f);
		configInput(AUDIO_INPUT, "Audio Input");
		configInput(REVERSE_INPUT, "Reverse Gate");
		configOutput(AUDIO_OUTPUT, "Audio Output");
		lightDivider.setDivision(512);
	}

	void onSampleRateChange() override
	{
		core.setSampleRate(APP->engine->getSampleRate());
	}

	void process(const ProcessArgs &args) override
	{
		core.mix = params[MIX_PARAM].getValue();

		float input = inputs[AUDIO_INPUT].getVoltage();

		// Process reverse gate with Schmitt trigger (0.1V low, 2.0V high)
		reverseTrigger.process(inputs[REVERSE_INPUT].getVoltage(), 0.1f, 2.0f);
		bool reverseState = reverseTrigger.isHigh();

		float output = core.process(input, reverseState);
		outputs[AUDIO_OUTPUT].setVoltage(output);

		if (lightDivider.process())
		{
			bool reversing    = core.isReversing();
			bool transitioning = core.isCrossfading() || core.isSyncing();
			// Green = forward, Red = reversing, both dim = crossfading/syncing
			lights[REVERSE_LIGHT + 0].setBrightness(transitioning ? 0.2f : (reversing ? 0.0f : 1.0f));
			lights[REVERSE_LIGHT + 1].setBrightness(transitioning ? 0.2f : (reversing ? 1.0f : 0.0f));
		}
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

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 20.0)), module, Resrever::MIX_PARAM));

		addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(10.16, 29.0)), module, Resrever::REVERSE_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 36.0)), module, Resrever::REVERSE_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)), module, Resrever::AUDIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Resrever::AUDIO_OUTPUT));
	}
};

Model *modelResrever = createModel<Resrever, ResreverWidget>("Resrever");
