#include "plugin.hpp"

struct Switch1 : Module
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		TRIGGER_IN_1,
		TRIGGER_IN_2,
		TRIGGER_IN_3,
		TRIGGER_IN_4,
		ENUMS(INPUT, 2),
		NUM_INPUTS
	};
	enum OutputIds
	{
		OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		ENUMS(LIGHT, 2),
		NUM_LIGHTS
	};

	dsp::SchmittTrigger t1Trigger;
	dsp::SchmittTrigger t2Trigger;

	int switchPosition = 0;

	Switch1()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void onReset() override
	{
		switchPosition = 0;
	}

	json_t *dataToJson() override
	{
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "switchPosition", json_integer(switchPosition));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override
	{
		json_t *switchPositionJ = json_object_get(rootJ, "switchPosition");
		if (switchPositionJ)
			switchPosition = json_integer_value(switchPositionJ);
	}

	void process(const ProcessArgs &args) override
	{
		if (t2Trigger.process(rescale(fabs(inputs[TRIGGER_IN_3].getVoltage()) + fabs(inputs[TRIGGER_IN_4].getVoltage()), 0.1f, 2.f, 0.f, 1.f)))
		{
			switchPosition = 1;
		}

		if (t1Trigger.process(rescale(fabs(inputs[TRIGGER_IN_1].getVoltage()) + fabs(inputs[TRIGGER_IN_2].getVoltage()), 0.1f, 2.f, 0.f, 1.f)))
		{
			switchPosition = 0;
		}

		outputs[OUTPUT].setVoltage(inputs[INPUT + switchPosition].getVoltage());

		lights[LIGHT + switchPosition].setSmoothBrightness(1, 100);
		lights[LIGHT + (switchPosition ^ 1)].setSmoothBrightness(0, 100);
	}
};

struct Switch1Widget : ModuleWidget
{
	Switch1Widget(Switch1 *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Switch1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		static const float col[3] = {5.5, 10.16, 14.82};
		static const float row[16] = {24.0, 39.0, 54.0, 69.0, 84.0};
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[0], row[0])), module, Switch1::TRIGGER_IN_1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[0], row[1])), module, Switch1::TRIGGER_IN_2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[2], row[0])), module, Switch1::TRIGGER_IN_3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[2], row[1])), module, Switch1::TRIGGER_IN_4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[0], row[2])), module, Switch1::INPUT + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[2], row[2])), module, Switch1::INPUT + 1));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col[0], row[3])), module, Switch1::LIGHT + 0));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(col[2], row[3])), module, Switch1::LIGHT + 1));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col[1], row[4])), module, Switch1::OUTPUT));
	}
};

Model *modelSwitch1 = createModel<Switch1, Switch1Widget>("Switch1");
