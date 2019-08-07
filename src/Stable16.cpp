#include "GoodSheperd.hpp"

struct Stable16 : Module
{
	enum ParamIds
	{
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		ENUMS(STEP_PARAM, 128),
		ENUMS(START_PARAM, 8),
		ENUMS(END_PARAM, 8),
		ENUMS(NUDGE_LEFT_PARAM, 8),
		ENUMS(NUDGE_RIGHT_PARAM, 8),
		NUM_PARAMS
	};
	enum InputIds
	{
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
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
		ENUMS(NUDGE_LEFT_LIGHT, 8),
		ENUMS(NUDGE_RIGHT_LIGHT, 8),
		NUM_LIGHTS
	};

	bool running = true;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger stepTrigger[128];
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger gateTriggers[8];
	dsp::SchmittTrigger nudgeLeftTriggers[8];
	dsp::SchmittTrigger nudgeRightTriggers[8];
	/** Phase of internal LFO */
	float phase = 0.f;
	bool stepValue[128] = {};
	int rowStepIndex[8] = {0, 0, 0, 0, 0, 0, 0};
	int rowStepIncrement[8] = {1, 1, 1, 1, 1, 1, 1, 1};

	dsp::ClockDivider lightDivider;

	Stable16()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int x = 0; x < 16; x++)
		{
			for (int y = 0; y < 8; y++)
			{
				configParam(Stable16::STEP_PARAM + x + 16 * y, 0.0f, 1.0f, 0.0f);
			}
		}

		for (int y = 0; y < 8; y++)
		{
			configParam(Stable16::START_PARAM + y, 0.0f, 15.0f, 0.0f);
			configParam(Stable16::END_PARAM + y, 0.0f, 15.0f, 15.0f);
			configParam(Stable16::NUDGE_LEFT_PARAM + y, 0.0f, 1.0f, 0.0f);
			configParam(Stable16::NUDGE_RIGHT_PARAM + y, 0.0f, 1.0f, 0.0f);
		}
		configParam(Stable16::CLOCK_PARAM, -2.0f, 6.0f, 2.0f);
		configParam(Stable16::RUN_PARAM, 0.0f, 1.0f, 0.0f);
		configParam(Stable16::RESET_PARAM, 0.0f, 1.0f, 0.0f);

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
		for (int i = 0; i < 128; i++)
		{
			stepValue[i] = (random::uniform() > 0.5f);
		}
	}

	json_t *dataToJson() override
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

	void dataFromJson(json_t *rootJ) override
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

	void resetStepIndices()
	{
		phase = 0.f;

		for (int row = 0; row < 8; row++)
		{
			rowStepIndex[row] = (int)params[START_PARAM + row].getValue();
		}
	}

	int getMatrixPosition(int row)
	{
		return 16 * row + rowStepIndex[row];
	}

	void calculateNextIndex()
	{
		for (int row = 0; row < 8; row++)
		{
			rowStepIndex[row] = rowStepIndex[row] + rowStepIncrement[row];
			if (rowStepIndex[row] > (int)params[END_PARAM + row].getValue())
			{
				rowStepIndex[row] = (int)params[START_PARAM + row].getValue();
			}
		}

		phase = 0.f;
	}

	void nudgeRowLeft(int row)
	{
		int start = 0 + row * 16;
		int end = 15 + row * 16;

		bool tempValue = stepValue[start];
		for (int i = start; i < end; i++)
		{
			stepValue[i] = stepValue[i + 1];
		}
		stepValue[end] = tempValue;
	}

	void nudgeRowRight(int row)
	{
		int start = 0 + row * 16;
		int end = 15 + row * 16;

		bool tempValue = stepValue[end];
		for (int i = end; i > start; i--)
		{
			stepValue[i] = stepValue[i - 1];
		}
		stepValue[start] = tempValue;
	}

	void process(const ProcessArgs &args) override
	{
		// Run
		if (runningTrigger.process(params[RUN_PARAM].getValue()))
		{
			running = !running;
		}
		lights[RUNNING_LIGHT].value = (running);

		bool gateIn = false;

		if (running)
		{
			if (inputs[EXT_CLOCK_INPUT].isConnected())
			{
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage()))
				{
					calculateNextIndex();
				}
				gateIn = clockTrigger.isHigh();
			}
			else
			{
				// Internal clock
				float clockTime = powf(2.0f, params[CLOCK_PARAM].getValue() + inputs[CLOCK_INPUT].getVoltage());
				phase += clockTime * args.sampleTime;
				if (phase >= 1.0f)
				{
					calculateNextIndex();
				}
				gateIn = (phase < 0.5f);
			}
		}

		// Reset
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage()))
		{
			resetStepIndices();
		}

		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime * lightDivider.getDivision());

		// Nudge
		for (int y = 0; y < 8; y++)
		{
			if (nudgeLeftTriggers[y].process(params[NUDGE_LEFT_PARAM + y].getValue()))
			{
				nudgeRowLeft(y);
			}

			if (nudgeRightTriggers[y].process(params[NUDGE_RIGHT_PARAM + y].getValue()))
			{
				nudgeRowRight(y);
			}

			lights[NUDGE_LEFT_PARAM + y].setSmoothBrightness(nudgeLeftTriggers[y].isHigh(), args.sampleTime * lightDivider.getDivision());
			lights[NUDGE_RIGHT_PARAM + y].setSmoothBrightness(nudgeRightTriggers[y].isHigh(), args.sampleTime * lightDivider.getDivision());
		}

		// Steps
		for (int i = 0; i < 128; i++)
		{
			if (stepTrigger[i].process(params[STEP_PARAM + i].getValue()))
			{
				stepValue[i] = !stepValue[i];
			}

			lights[STEP_LIGHT + i].setSmoothBrightness(stepValue[i] ? 1.0f : 0.0f, args.sampleTime * lightDivider.getDivision());
		}

		// Cursor Position
		for (int y = 0; y < 8; y++)
		{
			lights[STEP_LIGHT + getMatrixPosition(y)].setSmoothBrightness(0.5f, args.sampleTime * lightDivider.getDivision());
		}

		// Outputs
		for (int y = 0; y < 8; y++)
		{
			outputs[ROW_OUTPUT + y].setVoltage((gateIn && stepValue[getMatrixPosition(y)]) ? 10.0f : 0.0f);
			lights[ROW_LIGHTS + y].value = outputs[ROW_OUTPUT + y].value / 10.0f;
		}

		lights[GATES_LIGHT].setSmoothBrightness(gateIn, args.sampleTime * lightDivider.getDivision());
	}
};

struct Stable16Widget : ModuleWidget
{
	Stable16Widget(Stable16 *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Stable16.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		static const float stepGridX[16] = {20, 40, 60, 80, 100, 120, 140, 160, 180, 200, 220, 240, 260, 280, 300, 320};
		static const float stepGridY[8] = {64, 104, 144, 184, 224, 264, 304, 344};
		static const float gatesOutX = 372.0f;
		static const float startKnobsX = 412.0f;
		static const float endKnobsX = 452.0f;
		static const float nudgeLeftButtonX = 532.0f - 8.0f;
		static const float nudgeRightButtonX = 532.0f + 8.0f;

		for (int y = 0; y < 8; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				addParam(createParamCentered<LEDButton>(Vec(stepGridX[x], stepGridY[y]), module, Stable16::STEP_PARAM + x + 16 * y));				//36px
				addChild(createLightCentered<MediumLight<GreenLight>>(Vec(stepGridX[x], stepGridY[y]), module, Stable16::STEP_LIGHT + x + 16 * y)); // 20px
			}
		}

		for (int y = 0; y < 8; y++)
		{
			addOutput(createOutputCentered<PJ301MPort>(Vec(gatesOutX, stepGridY[y]), module, Stable16::ROW_OUTPUT + y));					// 50px
			addChild(createLightCentered<MediumLight<GreenLight>>(Vec(gatesOutX - 27.0f, stepGridY[y]), module, Stable16::ROW_LIGHTS + y)); // 20px

			addParam(createParamCentered<RoundBlackSnapKnob>(Vec(startKnobsX, stepGridY[y]), module, Stable16::START_PARAM + y)); // 64px
			addParam(createParamCentered<RoundBlackSnapKnob>(Vec(endKnobsX, stepGridY[y]), module, Stable16::END_PARAM + y));	 // 64px

			addParam(createParamCentered<LEDButton>(Vec(nudgeLeftButtonX, stepGridY[y]), module, Stable16::NUDGE_LEFT_PARAM + y));
			addChild(createLightCentered<MediumLight<RedLight>>(Vec(nudgeLeftButtonX, stepGridY[y]), module, Stable16::NUDGE_LEFT_LIGHT + y));
			addParam(createParamCentered<LEDButton>(Vec(nudgeRightButtonX, stepGridY[y]), module, Stable16::NUDGE_RIGHT_PARAM + y));
			addChild(createLightCentered<MediumLight<RedLight>>(Vec(nudgeRightButtonX, stepGridY[y]), module, Stable16::NUDGE_RIGHT_LIGHT + y));
		}

		static const float othersX = 492;
		addParam(createParamCentered<Rogan1PGreen>(Vec(othersX, stepGridY[0]), module, Stable16::CLOCK_PARAM));
		addInput(createInputCentered<PJ301MPort>(Vec(othersX, stepGridY[1]), module, Stable16::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(Vec(othersX, stepGridY[2]), module, Stable16::EXT_CLOCK_INPUT));
		addChild(createLightCentered<MediumLight<GreenLight>>(Vec(othersX, stepGridY[3]), module, Stable16::GATES_LIGHT));
		addParam(createParamCentered<LEDButton>(Vec(othersX, stepGridY[4]), module, Stable16::RUN_PARAM));
		addChild(createLightCentered<MediumLight<GreenLight>>(Vec(othersX, stepGridY[4]), module, Stable16::RUNNING_LIGHT));
		addParam(createParamCentered<LEDButton>(Vec(othersX, stepGridY[5]), module, Stable16::RESET_PARAM));
		addChild(createLightCentered<MediumLight<GreenLight>>(Vec(othersX, stepGridY[5]), module, Stable16::RESET_LIGHT));
		addInput(createInputCentered<PJ301MPort>(Vec(othersX, stepGridY[6]), module, Stable16::RESET_INPUT));
	}
};

Model *modelStable16 = createModel<Stable16, Stable16Widget>("Stable16");
