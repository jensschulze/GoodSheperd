#include "GoodSheperd.hpp"
#include "dsp/digital.hpp"

struct Stable16 : Module
{
	enum ParamIds
	{
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		ENUMS(STEP_PARAM, 128),
		ENUMS(START_PARAM, 8),
		ENUMS(END_PARAM, 8),
		ENUMS(MOVE_MODE_PARAM, 8),
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
		ENUMS(ROW_OUTPUT, 8),
		ENUMS(GATE_OUTPUT, 8),
		NUM_OUTPUTS
	};
	enum LightIds
	{
		ENUMS(STEP_LIGHT, 128),
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATES_LIGHT,
		ENUMS(ROW_LIGHTS, 8),
		ENUMS(GATE_LIGHTS, 8),
		NUM_LIGHTS
	};

	bool running = true;
	SchmittTrigger clockTrigger;
	SchmittTrigger runningTrigger;
	SchmittTrigger stepTrigger[128];
	SchmittTrigger resetTrigger;
	SchmittTrigger gateTriggers[8];
	/** Phase of internal LFO */
	float phase = 0.f;
	int index = 0;
	bool stepValue[128] = {};
	int rowStepIndex[8] = {0, 0, 0, 0, 0, 0, 0};
	int rowStepIncrement[8] = {1, 1, 1, 1, 1, 1, 1, 1};

	Stable16() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS)
	{
		onReset();
	}

	void onReset() override
	{
		for (int i = 0; i < 8; i++)
		{
			rowStepIndex[i] = 0;
		}
	}

	void onRandomize() override
	{
		// for (int i = 0; i < 8; i++)
		// {
		// 	gates[i] = (randomUniform() > 0.5f);
		// }
	}

	json_t *toJson() override
	{
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));

		// steps
		json_t *stepsJ = json_array();
		for (int i = 0; i < 128; i++)
		{
			json_array_insert_new(stepsJ, i, json_boolean(stepValue[i]));
		}
		json_object_set_new(rootJ, "steps", stepsJ);

		// position
		json_t *rowPositionJ = json_array();
		for (int i = 0; i < 8; i++)
		{
			json_array_insert_new(rowPositionJ, i, json_integer(rowStepIndex[i]));
		}
		json_object_set_new(rootJ, "positions", rowPositionJ);

		// increment
		json_t *rowStepIncrementJ = json_array();
		for (int i = 0; i < 8; i++)
		{
			json_array_insert_new(rowStepIncrementJ, i, json_integer(rowStepIncrement[i]));
		}
		json_object_set_new(rootJ, "increments", rowStepIncrementJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override
	{
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
		{
			running = json_is_true(runningJ);
		}

		// steps
		json_t *stepsJ = json_object_get(rootJ, "steps");
		if (stepsJ)
		{
			for (int i = 0; i < 128; i++)
			{
				json_t *stepJ = json_array_get(stepsJ, i);
				if (stepJ)
				{
					stepValue[i] = json_boolean_value(stepJ);
				}
			}
		}

		// position
		json_t *positionsJ = json_object_get(rootJ, "position");
		if (positionsJ)
		{
			for (int i = 0; i < 8; i++)
			{
				json_t *positionJ = json_array_get(positionsJ, i);
				if (positionJ)
				{
					rowStepIndex[i] = json_integer_value(positionJ);
				}
			}
		}

		// increment (rowStepIncrement)
		json_t *incrementsJ = json_object_get(rootJ, "increment");
		if (incrementsJ)
		{
			for (int i = 0; i < 8; i++)
			{
				json_t *incrementJ = json_array_get(incrementsJ, i);
				if (incrementJ)
				{
					rowStepIncrement[i] = json_integer_value(incrementJ);
				}
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

	int getIndex(int row)
	{
		return 16 * row + rowStepIndex[row];
	}

	void calculateNextIndex()
	{
		for (int row = 0; row < 8; row++)
		{
			rowStepIndex[row] = rowStepIndex[row] + rowStepIncrement[row];
			if (rowStepIndex[row] > (int)params[END_PARAM + row].value)
			{
				rowStepIndex[row] = (int)params[START_PARAM + row].value;
			}
		}

		phase = 0.f;
	}

	void step() override
	{
		// Run
		if (runningTrigger.process(params[RUN_PARAM].value))
		{
			running = !running;
		}
		lights[RUNNING_LIGHT].value = (running);

		bool gateIn = false;

		if (running)
		{
			if (inputs[EXT_CLOCK_INPUT].active)
			{
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value))
				{
					setIndex(index + 1);
					calculateNextIndex();
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
					calculateNextIndex();
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value))
		{
			setIndex(0);
		}

		lights[RESET_LIGHT].setBrightnessSmooth(resetTrigger.isHigh());

		// Steps
		for (int i = 0; i < 128; i++)
		{
			if (stepTrigger[i].process(params[STEP_PARAM + i].value))
			{
				stepValue[i] = !stepValue[i];
			}

			lights[STEP_LIGHT + i].setBrightnessSmooth(stepValue[i] ? 1.0f : 0.0f);
		}

		// Cursor Position
		for (int y = 0; y < 8; y++)
		{
			lights[STEP_LIGHT + getIndex(y)].setBrightnessSmooth(0.5f);
		}

		// Outputs
		// outputs[GATES_OUTPUT].value = (gateIn && gates[index]) ? 10.0f : 0.0f;
		for (int y = 0; y < 8; y++)
		{
			outputs[ROW_OUTPUT + y].value = (gateIn && stepValue[getIndex(y)]) ? 10.0f : 0.0f;
			lights[ROW_LIGHTS + y].value = outputs[ROW_OUTPUT + y].value / 10.0f;
		}

		lights[GATES_LIGHT].setBrightnessSmooth(gateIn);
	}
};

struct Stable16Widget : ModuleWidget
{
	Stable16Widget(Stable16 *module) : ModuleWidget(module)
	{
		setPanel(SVG::load(assetPlugin(plugin, "res/Stable16.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(15, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(15, 365)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 30, 365)));

		static const float stepGridX[16] = {20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320};
		static const float stepGridY[8] = {64, 104, 144, 184, 224, 264, 304, 344};
		static const float gatesOutX = 372.0f;
		static const float startKnobsX = 412.0f;
		static const float endKnobsX = 452.0f;
		for (int y = 0; y < 8; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				addParam(ParamWidget::create<LEDButton>(Vec(stepGridX[x] - 9.0f, stepGridY[y] - 9.0f), module, Stable16::STEP_PARAM + x + 16 * y, 0.0f, 1.0f, 0.0f));   //36px
				addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(stepGridX[x] - 4.6f, stepGridY[y] - 4.6f), module, Stable16::STEP_LIGHT + x + 16 * y)); // 20px
			}
		}

		for (int y = 0; y < 8; y++)
		{
			addOutput(Port::create<PJ301MPort>(Vec(gatesOutX - 12.5f, stepGridY[y] - 12.5f), Port::OUTPUT, module, Stable16::ROW_OUTPUT + y));					// 50px
			addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(gatesOutX - 27.0f - 5.0f, stepGridY[y] - 5.0f), module, Stable16::ROW_LIGHTS + y)); // 20px

			addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(startKnobsX - 16.0f, stepGridY[y] - 16.0f), module, Stable16::START_PARAM + y, 0.0f, 15.0f, 0.0f)); // 64px
			addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(endKnobsX - 16.0f, stepGridY[y] - 16.0f), module, Stable16::END_PARAM + y, 0.0f, 15.0f, 15.0f));	// 64px
		}

		static const float othersX = 492;
		addParam(ParamWidget::create<Rogan1PGreen>(Vec(othersX - 16.0f, stepGridY[0] - 16.0f), module, Stable16::CLOCK_PARAM, -2.0f, 6.0f, 2.0f));
		addInput(Port::create<PJ301MPort>(Vec(othersX - 12.5f, stepGridY[1] - 12.5f), Port::INPUT, module, Stable16::CLOCK_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(othersX - 12.5f, stepGridY[2] - 12.5f), Port::INPUT, module, Stable16::EXT_CLOCK_INPUT));
		addParam(ParamWidget::create<LEDButton>(Vec(othersX - 9.0f, stepGridY[3] - 9.0f), module, Stable16::RUN_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(othersX - 4.6f, stepGridY[3] - 4.6f), module, Stable16::RUNNING_LIGHT));
		addParam(ParamWidget::create<LEDButton>(Vec(othersX - 9.0f, stepGridY[4] - 9.0f), module, Stable16::RESET_PARAM, 0.0f, 1.0f, 0.0f));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(othersX - 4.6f, stepGridY[4] - 4.6f), module, Stable16::RESET_LIGHT));
		addInput(Port::create<PJ301MPort>(Vec(othersX - 12.5f, stepGridY[5] - 12.5f), Port::INPUT, module, Stable16::RESET_INPUT));
		addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(othersX - 16.0f, stepGridY[6] - 16.0f), module, Stable16::STEPS_PARAM, 1.0f, 8.0f, 8.0f));
		addInput(Port::create<PJ301MPort>(Vec(othersX, stepGridY[7]), Port::INPUT, module, Stable16::STEPS_INPUT));
		addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(200.0f, 5.0f), module, Stable16::GATES_LIGHT));

		// addOutput(Port::create<PJ301MPort>(Vec(portX[4] - 1, 98), Port::OUTPUT, module, Stable16::GATES_OUTPUT));
		// addOutput(Port::create<PJ301MPort>(Vec(portX[5] - 1, 98), Port::OUTPUT, module, Stable16::ROW1_OUTPUT));
		// addOutput(Port::create<PJ301MPort>(Vec(portX[6] - 1, 98), Port::OUTPUT, module, Stable16::ROW2_OUTPUT));
		// addOutput(Port::create<PJ301MPort>(Vec(portX[7] - 1, 98), Port::OUTPUT, module, Stable16::ROW3_OUTPUT));

		// for (int i = 0; i < 8; i++)
		// {
		// 	addParam(ParamWidget::create<Rogan1PBlue>(Vec(portX[i] - 6, 157), module, Stable16::ROW1_PARAM + i, 0.0f, 10.0f, 0.0f));
		// 	addParam(ParamWidget::create<Rogan1PWhite>(Vec(portX[i] - 6, 198), module, Stable16::ROW2_PARAM + i, 0.0f, 10.0f, 0.0f));
		// 	addParam(ParamWidget::create<Rogan1PRed>(Vec(portX[i] - 6, 240), module, Stable16::ROW3_PARAM + i, 0.0f, 10.0f, 0.0f));
		// 	addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(portX[i] + 6.4f, 281.4f), module, Stable16::GATE_LIGHTS + i));
		// 	addOutput(Port::create<PJ301MPort>(Vec(portX[i] - 1, 307), Port::OUTPUT, module, Stable16::GATE_OUTPUT + i));
		// }
		// addOutput(Port::create<PJ301MPort>(Vec(360, 162), Port::OUTPUT, module, Stable16::GATE_ROW1_OUTPUT));
		// addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 169), module, Stable16::GATE_ROW1_LIGHT));
		// addOutput(Port::create<PJ301MPort>(Vec(360, 203), Port::OUTPUT, module, Stable16::GATE_ROW2_OUTPUT));
		// addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 210), module, Stable16::GATE_ROW2_LIGHT));
		// addOutput(Port::create<PJ301MPort>(Vec(360, 244), Port::OUTPUT, module, Stable16::GATE_ROW3_OUTPUT));
		// addChild(ModuleLightWidget::create<MediumLight<GreenLight>>(Vec(335, 252), module, Stable16::GATE_ROW3_LIGHT));
	}
};

Model *modelStable16 = Model::create<Stable16, Stable16Widget>("GoodSheperd", "Stable16", "Stable16", SEQUENCER_TAG);
