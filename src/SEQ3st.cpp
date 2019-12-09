#include "GoodSheperd.hpp"

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
		SHAPE_PARAM,
		NUM_PARAMS
	};
	enum InputIds
	{
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
		SHAPE_INPUT,
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
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger runningTrigger;
	dsp::SchmittTrigger resetTrigger;
	dsp::SchmittTrigger gateTriggers[8];
	/** Phase of internal LFO */
	float phase = 0.f;
	int index = 0;
	bool gates[8] = {};
	bool gateRow1IsOpen = false;
	bool gateRow2IsOpen = false;
	bool gateRow3IsOpen = false;
	dsp::ClockDivider lightDivider;

	SEQ3st()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SEQ3st::CLOCK_PARAM, -2.0f, 6.0f, 2.0f, "Clock");
		configParam(SEQ3st::RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(SEQ3st::RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(SEQ3st::STEPS_PARAM, 1.0f, 8.0f, 8.0f, "Steps");
		configParam(SEQ3st::SHAPE_PARAM, -5.f, 5.f, 0.f, "Shape");
		for (int i = 0; i < 8; i++)
		{
			configParam(SEQ3st::ROW1_PARAM + i, 0.0f, 10.0f, 0.0f, "Value");
			configParam(SEQ3st::ROW2_PARAM + i, 0.0f, 10.0f, 0.0f, "Value");
			configParam(SEQ3st::ROW3_PARAM + i, 0.0f, 10.0f, 0.0f, "Value");
			configParam(SEQ3st::GATE_PARAM + i, 0.0f, 1.0f, 0.0f, "Gate");
		}

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
			gates[i] = (random::uniform() > 0.5f);
		}
	}

	json_t *dataToJson() override
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

	void dataFromJson(json_t *rootJ) override
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
		int numSteps = (int)clamp(roundf(params[STEPS_PARAM].getValue() + inputs[STEPS_INPUT].getVoltage()), 1.0f, 8.0f);
		phase = 0.f;
		this->index = index;
		if (this->index >= numSteps)
			this->index = 0;
	}

	float getShapedRandom(float shapeValue)
	{
		float shape = clamp(roundf(shapeValue), -5.f, 5.f) * .2f * .99f;
		const float partialA = (4.0 * shape) / ((1.0 - shape) * (1.0 + shape));
		const float partialB = (1.0 - shape) / (1.0 + shape);

		float rawRandom = random::uniform() * 2.f - 1.f;
		float shapedRandom = rawRandom * (partialA + partialB) / ((abs(rawRandom) * partialA) + partialB);

		return (shapedRandom + 1.f) * 5.f;
	}

	void process(const ProcessArgs &args) override
	{
		// Run
		if (runningTrigger.process(params[RUN_PARAM].getValue()))
		{
			running = !running;
		}

		bool gateIn = false;
		bool gateRow1Out = gateRow1IsOpen;
		bool gateRow2Out = gateRow2IsOpen;
		bool gateRow3Out = gateRow3IsOpen;

		if (running)
		{
			float shapeValue = params[SHAPE_PARAM].getValue() + inputs[SHAPE_INPUT].getVoltage();

			if (inputs[EXT_CLOCK_INPUT].isConnected())
			{
				// External clock
				if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage()))
				{
					setIndex(index + 1);
					if (params[ROW1_PARAM + index].getValue() >= getShapedRandom(shapeValue))
					{
						gateRow1Out = true;
					}
					if (params[ROW2_PARAM + index].getValue() >= getShapedRandom(shapeValue))
					{
						gateRow2Out = true;
					}
					if (params[ROW3_PARAM + index].getValue() >= getShapedRandom(shapeValue))
					{
						gateRow3Out = true;
					}
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
					setIndex(index + 1);
					if (params[ROW1_PARAM + index].getValue() >= getShapedRandom(shapeValue))
					{
						gateRow1Out = true;
					}
					if (params[ROW2_PARAM + index].getValue() >= getShapedRandom(shapeValue))
					{
						gateRow2Out = true;
					}
					if (params[ROW3_PARAM + index].getValue() >= getShapedRandom(shapeValue))
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
		if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage()))
		{
			setIndex(0);
		}

		// Gate buttons
		for (int i = 0; i < 8; i++)
		{
			if (gateTriggers[i].process(params[GATE_PARAM + i].getValue()))
			{
				gates[i] = !gates[i];
			}
			outputs[GATE_OUTPUT + i].setVoltage((running && gateIn && i == index && gates[i]) ? 10.0f : 0.0f);
			lights[GATE_LIGHTS + i].setSmoothBrightness((gateIn && i == index) ? (gates[i] ? 1.f : 0.33) : (gates[i] ? 0.66 : 0.0), args.sampleTime * lightDivider.getDivision());
		}

		// Outputs
		outputs[ROW1_OUTPUT].setVoltage(params[ROW1_PARAM + index].getValue());
		outputs[ROW2_OUTPUT].setVoltage(params[ROW2_PARAM + index].getValue());
		outputs[ROW3_OUTPUT].setVoltage(params[ROW3_PARAM + index].getValue());
		outputs[GATES_OUTPUT].setVoltage((gateIn && gates[index]) ? 10.0f : 0.0f);
		lights[RUNNING_LIGHT].value = (running);
		lights[RESET_LIGHT].setSmoothBrightness(resetTrigger.isHigh(), args.sampleTime * lightDivider.getDivision());
		lights[GATES_LIGHT].setSmoothBrightness(gateIn, args.sampleTime * lightDivider.getDivision());
		lights[ROW_LIGHTS].value = outputs[ROW1_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 1].value = outputs[ROW2_OUTPUT].value / 10.0f;
		lights[ROW_LIGHTS + 2].value = outputs[ROW3_OUTPUT].value / 10.0f;

		outputs[GATE_ROW1_OUTPUT].setVoltage(gateRow1Out ? 10.0f : 0.0f);
		outputs[GATE_ROW2_OUTPUT].setVoltage(gateRow2Out ? 10.0f : 0.0f);
		outputs[GATE_ROW3_OUTPUT].setVoltage(gateRow3Out ? 10.0f : 0.0f);
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

	SEQ3stWidget(SEQ3st *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SEQ3st.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 30, 365)));

		addParam(createParam<Rogan1PGreen>(Vec(15, 41), module, SEQ3st::CLOCK_PARAM));
		addParam(createParam<LEDButton>(Vec(62, 55), module, SEQ3st::RUN_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(66.4f, 59.4f), module, SEQ3st::RUNNING_LIGHT));
		addParam(createParam<LEDButton>(Vec(102, 55), module, SEQ3st::RESET_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(106.4f, 59.4f), module, SEQ3st::RESET_LIGHT));
		addParam(createParam<Rogan1PGreenSnapKnob>(Vec(135, 41), module, SEQ3st::STEPS_PARAM));
		addParam(createParam<Rogan1PGreen>(Vec(175, 41), module, SEQ3st::SHAPE_PARAM));
		addChild(createLight<MediumLight<GreenLight>>(Vec(226.4f, 59.4f), module, SEQ3st::GATES_LIGHT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(266.4f, 59.4f), module, SEQ3st::ROW_LIGHTS));
		addChild(createLight<MediumLight<GreenLight>>(Vec(306.4f, 59.4f), module, SEQ3st::ROW_LIGHTS + 1));
		addChild(createLight<MediumLight<GreenLight>>(Vec(346.4f, 59.4f), module, SEQ3st::ROW_LIGHTS + 2));

		static const float portX[9] = {20, 60, 100, 140, 180, 220, 260, 300, 340};
		addInput(createInput<PJ301MPort>(Vec(portX[0] - 1, 98), module, SEQ3st::CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[1] - 1, 98), module, SEQ3st::EXT_CLOCK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[2] - 1, 98), module, SEQ3st::RESET_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[3] - 1, 98), module, SEQ3st::STEPS_INPUT));
		addInput(createInput<PJ301MPort>(Vec(portX[4] - 1, 98), module, SEQ3st::SHAPE_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[5] - 1, 98), module, SEQ3st::GATES_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[6] - 1, 98), module, SEQ3st::ROW1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[7] - 1, 98), module, SEQ3st::ROW2_OUTPUT));
		addOutput(createOutput<PJ301MPort>(Vec(portX[8] - 1, 98), module, SEQ3st::ROW3_OUTPUT));

		for (int i = 0; i < 8; i++)
		{
			addParam(createParam<Rogan1PBlue>(Vec(portX[i] - 6, 157), module, SEQ3st::ROW1_PARAM + i));
			addParam(createParam<Rogan1PWhite>(Vec(portX[i] - 6, 198), module, SEQ3st::ROW2_PARAM + i));
			addParam(createParam<Rogan1PRed>(Vec(portX[i] - 6, 240), module, SEQ3st::ROW3_PARAM + i));
			addParam(createParam<LEDButton>(Vec(portX[i] + 2, 278 - 1), module, SEQ3st::GATE_PARAM + i));
			addChild(createLight<MediumLight<GreenLight>>(Vec(portX[i] + 6.4f, 281.4f), module, SEQ3st::GATE_LIGHTS + i));
			addOutput(createOutput<PJ301MPort>(Vec(portX[i] - 1, 307), module, SEQ3st::GATE_OUTPUT + i));
		}
		addOutput(createOutput<PJ301MPort>(Vec(360, 162), module, SEQ3st::GATE_ROW1_OUTPUT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(335, 169), module, SEQ3st::GATE_ROW1_LIGHT));
		addOutput(createOutput<PJ301MPort>(Vec(360, 203), module, SEQ3st::GATE_ROW2_OUTPUT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(335, 210), module, SEQ3st::GATE_ROW2_LIGHT));
		addOutput(createOutput<PJ301MPort>(Vec(360, 244), module, SEQ3st::GATE_ROW3_OUTPUT));
		addChild(createLight<MediumLight<GreenLight>>(Vec(335, 252), module, SEQ3st::GATE_ROW3_LIGHT));
	}
};

Model *modelSEQ3st = createModel<SEQ3st, SEQ3stWidget>("SEQ3st");
