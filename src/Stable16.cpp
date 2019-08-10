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
		ENUMS(MUTE_PARAM, 8),
		ENUMS(NUDGE_LEFT_PARAM, 8),
		ENUMS(NUDGE_RIGHT_PARAM, 8),
		NUDGE_MODE_PARAM,
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
	bool mute[8] = {false, false, false, false, false, false, false, false};
	bool nudgeModeInternal = false;

	dsp::ClockDivider lightDivider;

	Stable16()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int x = 0; x < 16; x++)
		{
			for (int y = 0; y < 8; y++)
			{
				configParam(Stable16::STEP_PARAM + x + 16 * y, 0.f, 1.f, 0.f);
			}
		}

		for (int y = 0; y < 8; y++)
		{
			configParam(Stable16::START_PARAM + y, 0.f, 15.f, 0.f, "Start");
			configParam(Stable16::END_PARAM + y, 0.f, 15.f, 15.f, "End");
			configParam(Stable16::NUDGE_LEFT_PARAM + y, 0.f, 1.f, 0.f, "Nudge left");
			configParam(Stable16::NUDGE_RIGHT_PARAM + y, 0.f, 1.f, 0.f, "Nudge right");
		}

		configParam(Stable16::CLOCK_PARAM, -2.f, 6.f, 2.f, "Tempo");
		configParam(Stable16::RUN_PARAM, 0.f, 1.f, 0.f, "Run/Stop");
		configParam(Stable16::RESET_PARAM, 0.f, 1.f, 0.f, "Reset");
		configParam(Stable16::NUDGE_MODE_PARAM, 0.f, 1.f, 0.f, "Nudge mode");

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

		// mutes
		json_t *mutesJ = json_array();
		for (int i = 0; i < 8; i++)
		{
			json_array_insert_new(mutesJ, i, json_boolean(mute[i]));
		}
		json_object_set_new(rootJ, "mutes", mutesJ);

		// position
		json_t *rowPositionJ = json_array();
		for (int i = 0; i < 8; i++)
		{
			json_array_insert_new(rowPositionJ, i, json_integer(rowStepIndex[i]));
		}
		json_object_set_new(rootJ, "positions", rowPositionJ);

		// nudge mode
		json_object_set_new(rootJ, "nudge_mode_internal", json_boolean(nudgeModeInternal));

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

		//mutes
		json_t *mutesJ = json_object_get(rootJ, "mutes");
		if (mutesJ)
		{
			for (int i = 0; i < 8; i++)
			{
				json_t *muteJ = json_array_get(mutesJ, i);
				if (muteJ)
				{
					params[MUTE_PARAM + i].setValue(json_boolean_value(muteJ));
				}
			}
		}

		// position
		json_t *positionsJ = json_object_get(rootJ, "positions");
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

		// nudge mode
		json_t *nudgeModeInternalJ = json_object_get(rootJ, "nudge_mode_internal");
		if (nudgeModeInternalJ)
		{
			nudgeModeInternal = json_is_true(nudgeModeInternalJ);
			params[NUDGE_MODE_PARAM].setValue(nudgeModeInternal ? 1.f : 0.f);
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
		int start = row * 16;
		int end = row * 16;

		if (nudgeModeInternal)
		{
			start += (int)params[START_PARAM + row].getValue();
			end += (int)params[END_PARAM + row].getValue();
		}
		else
		{
			end += 15;
		}

		bool tempValue = stepValue[start];
		for (int i = start; i < end; i++)
		{
			stepValue[i] = stepValue[i + 1];
		}
		stepValue[end] = tempValue;
	}

	void nudgeRowRight(int row)
	{
		int start = row * 16;
		int end = row * 16;

		if (nudgeModeInternal)
		{
			start += (int)params[START_PARAM + row].getValue();
			end += (int)params[END_PARAM + row].getValue();
		}
		else
		{
			end += 15;
		}

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
		if (runningTrigger.process(rescale(params[RUN_PARAM].getValue(), 0.1f, 1.f, 0.f, 1.f)))
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
				if (clockTrigger.process(rescale(inputs[EXT_CLOCK_INPUT].getVoltage(), 0.1f, 1.f, 0.f, 1.f)))
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
		if (resetTrigger.process(rescale(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f, 0.f, 1.f)))
		{
			resetStepIndices();
		}

		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime * lightDivider.getDivision());

		// Nudge mode
		nudgeModeInternal = params[NUDGE_MODE_PARAM].getValue() == 1.f;

		// Nudge
		for (int y = 0; y < 8; y++)
		{
			if (nudgeLeftTriggers[y].process(rescale(params[NUDGE_LEFT_PARAM + y].getValue(), 0.1f, 1.f, 0.f, 1.f)))
			{
				nudgeRowLeft(y);
			}

			if (nudgeRightTriggers[y].process(rescale(params[NUDGE_RIGHT_PARAM + y].getValue(), 0.1f, 1.f, 0.f, 1.f)))
			{
				nudgeRowRight(y);
			}
		}

		// Steps
		for (int i = 0; i < 128; i++)
		{
			if (stepTrigger[i].process(params[STEP_PARAM + i].getValue()))
			{
				stepValue[i] = !stepValue[i];
			}

			lights[STEP_LIGHT + i].setSmoothBrightness(stepValue[i] ? 0.7f : 0.0f, args.sampleTime * lightDivider.getDivision());
		}

		// Cursor Position
		for (int y = 0; y < 8; y++)
		{
			int index = getMatrixPosition(y);
			lights[STEP_LIGHT + index].setSmoothBrightness(stepValue[index] ? 1.f : 0.2f, args.sampleTime * lightDivider.getDivision());
		}

		// Outputs and mutes
		for (int y = 0; y < 8; y++)
		{
			mute[y] = params[MUTE_PARAM + y].getValue() == 1.f;
			outputs[ROW_OUTPUT + y].setVoltage((gateIn && !mute[y] && stepValue[getMatrixPosition(y)]) ? 10.0f : 0.0f);
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
				addParam(createParamCentered<LEDButton>(Vec(stepGridX[x], stepGridY[y]), module, Stable16::STEP_PARAM + x + 16 * y));
				addChild(createLightCentered<MediumLight<GreenLight>>(Vec(stepGridX[x], stepGridY[y]), module, Stable16::STEP_LIGHT + x + 16 * y));
			}

			addOutput(createOutputCentered<PJ301MPort>(Vec(gatesOutX, stepGridY[y]), module, Stable16::ROW_OUTPUT + y));
			addParam(createParamCentered<SquareSwitch>(Vec(gatesOutX - 27.f, stepGridY[y]), module, Stable16::MUTE_PARAM + y));
			addChild(createLightCentered<MediumLight<GreenLight>>(Vec(gatesOutX - 27.f, stepGridY[y]), module, Stable16::ROW_LIGHTS + y));

			addParam(createParamCentered<RoundBlackSnapKnob>(Vec(startKnobsX, stepGridY[y]), module, Stable16::START_PARAM + y));
			addParam(createParamCentered<RoundBlackSnapKnob>(Vec(endKnobsX, stepGridY[y]), module, Stable16::END_PARAM + y));

			addParam(createParamCentered<ArrowLeft>(Vec(nudgeLeftButtonX, stepGridY[y]), module, Stable16::NUDGE_LEFT_PARAM + y));
			addParam(createParamCentered<ArrowRight>(Vec(nudgeRightButtonX, stepGridY[y]), module, Stable16::NUDGE_RIGHT_PARAM + y));
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
		addParam(createParamCentered<CKSS>(Vec(othersX, stepGridY[7]), module, Stable16::NUDGE_MODE_PARAM));
	}
};

Model *modelStable16 = createModel<Stable16, Stable16Widget>("Stable16");
