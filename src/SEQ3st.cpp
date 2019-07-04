#include "GoodSheperd.hpp"
#include "dsp/digital.hpp"

struct SEQ3st : Module
{
	enum ParamIds
	{
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		ENUMS(ROW1_PARAM, 8),
		ENUMS(ROW2_PARAM, 8),
		ENUMS(ROW3_PARAM, 8),
		ENUMS(GATE_PARAM, 8),
		NUM_PARAMS
	};
	enum InputIds
	{
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		GATES_OUTPUT,
		ROW1_OUTPUT,
		ROW2_OUTPUT,
		ROW3_OUTPUT,
		GATE_ROW1_OUTPUT,
		GATE_ROW2_OUTPUT,
		GATE_ROW3_OUTPUT,
		ENUMS(GATE_OUTPUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds
	{
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		GATE_ROW1_LIGHT,
		GATE_ROW2_LIGHT,
		GATE_ROW3_LIGHT,
		ENUMS(ROW_LIGHTS, 3),
		ENUMS(GATE_LIGHTS, 8),
		NUM_LIGHTS
	};

	bool running = true;
	SchmittTrigger clockTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger gateTriggers[8];
	/** Phase of internal LFO */
	float phase = 0.f;
	int index = 0;
	bool gates[8] = {};
	bool gateRow1IsOpen = false;
	bool gateRow2IsOpen = false;
	bool gateRow3IsOpen = false;

	SEQ3st() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS)
	{
		onReset();
	}

	void onReset() override
	{
		for (int i = 0; i < 8; i++)
		{
			gates[i] = true;
		}
	}

	void onRandomize() override
	{
		for (int i = 0; i < 8; i++)
		{
			gates[i] = (randomUniform() > 0.5f);
		}
	}

	json_t *toJson() override
	{
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// gates
		json_t *gatesJ = json_array();
		for (int i = 0; i < 8; i++)
		{
			json_array_insert_new(gatesJ, i, json_integer((int)gates[i]));
		}
		json_object_set_new(rootJ, "gates", gatesJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override
	{
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);

		// gates
		json_t *gatesJ = json_object_get(rootJ, "gates");
		if (gatesJ)
		{
			for (int i = 0; i < 8; i++)
			{
				json_t *gateJ = json_array_get(gatesJ, i);
				if (gateJ)
					gates[i] = !!json_integer_value(gateJ);
			}
		}
	}

	void setIndex(int index)
	{
		int numSteps = (int)clamp(roundf(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1.0f, 8.0f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	void step() override
	{
		// Run
		if (runningTrigger.process(params[RUN_PARAM].value))
		{
			running = !running;
		}

		bool gateIn = false;
		bool gateRow1Out = gateRow1IsOpen;
		bool gateRow2Out = gateRow2IsOpen;
		bool gateRow3Out = gateRow3IsOpen;

		if (running)
		{
			if (inputs[EXT_CLOCK_INPUT].active)
			{
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value))
				{
					setIndex(index + 1);
					if (params[ROW1_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow1Out = true;
					}
					if (params[ROW2_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow2Out = true;
					}
					if (params[ROW3_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow3Out = true;
					}
				}
				gateIn = clockTrigger.isHigh();
			}
			else
			{
				// Internal clock
				float clockTime = powf(2.0f, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
				phase += clockTime * engineGetSampleTime();
				if (phase >= 1.0f)
				{
					setIndex(index + 1);
					if (params[ROW1_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow1Out = true;
					}
					if (params[ROW2_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow2Out = true;
					}
					if (params[ROW3_PARAM + index].value >= randomUniform() * 10.f)
					{
						gateRow3Out = true;
					}
				}
				gateIn = (phase < 0.5f);
			}
		}

		if (!gateIn)
		{
			gateRow1Out = false;
			gateRow2Out = false;
			gateRow3Out = false;
		}

		gateRow1IsOpen = gateRow1Out;
		gateRow2IsOpen = gateRow2Out;
		gateRow3IsOpen = gateRow3Out;

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value))
		{
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++)
		{
			if (gateTriggers[i].process(params[GATE_PARAM + i].value))
			{
				gates[i] = !gates[i];
			}
			outputs[GATE_OUTPUT + i].value = (running && gateIn && i == index && gates[i]) ? 10.0f : 0.0f;
			lights[GATE_LIGHTS + i].setBrightnessSmooth((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0));
		}

		// Outputs
		outputs[ROW1_OUTPUT].value = params[ROW1_PARAM + index].value;
		outputs[ROW2_OUTPUT].value = params[ROW2_PARAM + index].value;
		outputs[ROW3_OUTPUT].value = params[ROW3_PARAM + index].value;
		outputs[GATES_OUTPUT].value = (gateIn && gates[index]) ? 10.0f : 0.0f;
		lights[RUNNING_LIGHT].value = (running);
		lights[RESET_LIGHT].setBrightnessSmooth(resetTrigger.isHigh());
		lights[GATES_LIGHT].setBrightnessSmooth(gateIn);
		lights[ROW_LIGHTS].value = outputs[ROW1_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.0f;

		outputs[GATE_ROW1_OUTPUT].value = gateRow1Out ? 10.0f : 0.0f;
		outputs[GATE_ROW2_OUTPUT].value = gateRow2Out ? 10.0f : 0.0f;
		outputs[GATE_ROW3_OUTPUT].value = gateRow3Out ? 10.0f : 0.0f;
		lights[GATE_ROW1_LIGHT].value = outputs[GATE_ROW1_OUTPUT].value / 10.0f;
		lights[GATE_ROW2_LIGHT].value = outputs[GATE_ROW2_OUTPUT].value / 10.0f;
		lights[GATE_ROW3_LIGHT].value = outputs[GATE_ROW3_OUTPUT].value / 10.0f;
	}
};

struct SEQ3stWidget : ModuleWidget
{
	struct Rogan1PGreenSnapKnob : Rogan1PGreen
	{
		Rogan1PGreenSnapKnob()
		{
			snap = true;
			smooth = false;
		}
	};

	SEQ3stWidget(SEQ3st *module) : ModuleWidget(module)
	{
		setPanel(SVG::load(assetPlugin(pluginInstance, "res/SEQ3st.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(ParamWidget::create<Rogan1PGreen>(Vec(15, 41), module, SEQ3st::CLOCK_PARAM, -2.0f, 6.0f, 2.0f));
		addParam(ParamWidget::create<LEDButton>(Vec(62, 55), module, SEQ3st::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(66.4f, 59.4f), module, SEQ3st::RUNNING_LIGHT));
		addParam(ParamWidget::create<LEDButton>(Vec(102, 55), module, SEQ3st::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(106.4f, 59.4f), module, SEQ3st::RESET_LIGHT));
		addParam(ParamWidget::create<Rogan1PGreenSnapKnob>(Vec(135, 41), module, SEQ3st::STEPS_PARAM, 1.0f, 8.0f, 8.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(186.4f, 59.4f), module, SEQ3st::GATES_LIGHT));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(226.4f, 59.4f), module, SEQ3st::ROW_LIGHTS));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(266.4f, 59.4f), module, SEQ3st::ROW_LIGHTS + 1));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(306.4f, 59.4f), module, SEQ3st::ROW_LIGHTS + 2));

		static const float portX[8] = {20, 60, 100, 140, 180, 220, 260, 300};
		addInput(Port::create<PJ301MPort>(Vec(portX[0] - 1, 98), Port::INPUT, module, SEQ3st::CLOCK_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(portX[1] - 1, 98), Port::INPUT, module, SEQ3st::EXT_CLOCK_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(portX[2] - 1, 98), Port::INPUT, module, SEQ3st::RESET_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(portX[3] - 1, 98), Port::INPUT, module, SEQ3st::STEPS_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(portX[4] - 1, 98), Port::OUTPUT, module, SEQ3st::GATES_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(portX[5] - 1, 98), Port::OUTPUT, module, SEQ3st::ROW1_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(portX[6] - 1, 98), Port::OUTPUT, module, SEQ3st::ROW2_OUTPUT));
		addOutput(Port::create<PJ301MPort>(Vec(portX[7] - 1, 98), Port::OUTPUT, module, SEQ3st::ROW3_OUTPUT));

		for (int i = 0; i < 8; i++)
		{
			addParam(ParamWidget::create<Rogan1PBlue>(Vec(portX[i] - 6, 157), module, SEQ3st::ROW1_PARAM + i, 0.0f, 10.0f, 0.0f));
			addParam(ParamWidget::create<Rogan1PWhite>(Vec(portX[i] - 6, 198), module, SEQ3st::ROW2_PARAM + i, 0.0f, 10.0f, 0.0f));
			addParam(ParamWidget::create<Rogan1PRed>(Vec(portX[i] - 6, 240), module, SEQ3st::ROW3_PARAM + i, 0.0f, 10.0f, 0.0f));
			addParam(ParamWidget::create<LEDButton>(Vec(portX[i] + 2, 278 - 1), module, SEQ3st::GATE_PARAM + i, 0.0f, 1.0f, 0.0f));
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(portX[i] + 6.4f, 281.4f), module, SEQ3st::GATE_LIGHTS + i));
			addOutput(Port::create<PJ301MPort>(Vec(portX[i] - 1, 307), Port::OUTPUT, module, SEQ3st::GATE_OUTPUT + i));
		}
		addOutput(Port::create<PJ301MPort>(Vec(360, 162), Port::OUTPUT, module, SEQ3st::GATE_ROW1_OUTPUT));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 169), module, SEQ3st::GATE_ROW1_LIGHT));
		addOutput(Port::create<PJ301MPort>(Vec(360, 203), Port::OUTPUT, module, SEQ3st::GATE_ROW2_OUTPUT));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 210), module, SEQ3st::GATE_ROW2_LIGHT));
		addOutput(Port::create<PJ301MPort>(Vec(360, 244), Port::OUTPUT, module, SEQ3st::GATE_ROW3_OUTPUT));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 252), module, SEQ3st::GATE_ROW3_LIGHT));
	}
};

Model *modelSEQ3st = Model::create<SEQ3st, SEQ3stWidget>("SEQ3st");
