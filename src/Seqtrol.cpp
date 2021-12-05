#include "plugin.hpp"

struct Seqtrol : Module
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		START_TRIGGER_INPUT,
		CONTINUE_TRIGGER_INPUT,
		STOP_TRIGGER_INPUT,
		CLOCK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		RESET_OUTPUT,
		CLOCK_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		RUNNING_LIGHT,
		NUM_LIGHTS
	};

	dsp::SchmittTrigger startTrigger;
	dsp::SchmittTrigger continueTrigger;
	dsp::SchmittTrigger stopTrigger;
	dsp::SchmittTrigger inputClockTrigger;
	dsp::SchmittTrigger intermediateClockTrigger;

	bool isRunning = false;
	bool isWaitingForClockRisingEdge = false;

	int divisorIndex = 0;
	int clockCounter = 0;

	int counterMax[13] = {1, 3, 6, 12, 24, 48, 96, 2, 4, 8, 16, 32, 64};

	Seqtrol()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void onReset() override
	{
		isRunning = false;
		isWaitingForClockRisingEdge = false;
		divisorIndex = 0;
		clockCounter = 0;
	}

	json_t *dataToJson() override
	{
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "divisorIndex", json_integer(divisorIndex));
		json_object_set_new(rootJ, "clockCounter", json_integer(clockCounter));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override
	{
		json_t *divisorIndexJ = json_object_get(rootJ, "divisorIndex");
		if (divisorIndexJ)
		{
			divisorIndex = json_integer_value(divisorIndexJ);
		}

		json_t *clockCounterJ = json_object_get(rootJ, "clockCounter");
		if (clockCounterJ)
		{
			clockCounter = json_integer_value(clockCounterJ);
		}
	}

	void process(const ProcessArgs &args) override
	{
		bool startWasTriggered = startTrigger.process(rescale(inputs[START_TRIGGER_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));
		bool continueWasTriggered = continueTrigger.process(rescale(inputs[CONTINUE_TRIGGER_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));

		if (startWasTriggered)
		{
			clockCounter = 0;
		}

		if (startWasTriggered || continueWasTriggered)
		{
			isRunning = true;
		};

		if (stopTrigger.process(rescale(inputs[STOP_TRIGGER_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f)))
		{
			isRunning = false;
		}

		inputClockTrigger.process(rescale(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f, 0.f, 1.f));
		if (startWasTriggered || isWaitingForClockRisingEdge)
		{
			if (inputClockTrigger.isHigh())
			{
				outputs[RESET_OUTPUT].setVoltage(inputs[CLOCK_INPUT].getVoltage());
				isWaitingForClockRisingEdge = false;
			}
			else
			{
				outputs[RESET_OUTPUT].setVoltage(0.0f);
				isWaitingForClockRisingEdge = true;
			}
		}
		else
		{
			outputs[RESET_OUTPUT].setVoltage(0.0f);
		}

		float intermediateClock = isRunning && !isWaitingForClockRisingEdge ? inputs[CLOCK_INPUT].getVoltage() : 0.f;
		if (intermediateClockTrigger.process(1.f - rescale(intermediateClock, 0.1f, 2.f, 0.f, 1.f)))
		{
			if (++clockCounter >= counterMax[divisorIndex])
			{
				clockCounter = 0;
			}
		}

		outputs[CLOCK_OUTPUT].setVoltage(clockCounter == 0 ? intermediateClock : 0.f);
		lights[RUNNING_LIGHT].setSmoothBrightness(isRunning ? 1.f : 0.f, 100.f);
	}
};

struct SeqtrolWidget : ModuleWidget
{
	SeqtrolWidget(Seqtrol *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Seqtrol.svg")));

		static const float col[3] = {5.5f, 10.16f, 14.82f};
		static const float row[16] = {24.f, 39.f, 54.f, 69.f, 84.f, 102.f};

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(4.f, 31.5f)), module, Seqtrol::RUNNING_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[1], row[0])), module, Seqtrol::START_TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[1], row[1])), module, Seqtrol::CONTINUE_TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[1], row[2])), module, Seqtrol::STOP_TRIGGER_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(col[1], row[3])), module, Seqtrol::CLOCK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col[1], row[4])), module, Seqtrol::RESET_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(col[1], row[5])), module, Seqtrol::CLOCK_OUTPUT));
	}

	void appendContextMenu(Menu *menu) override
	{
		Seqtrol *module = dynamic_cast<Seqtrol *>(this->module);

		menu->addChild(new MenuEntry);
		menu->addChild(createMenuLabel("Clock divisor"));

		struct DivisorItem : MenuItem
		{
			Seqtrol *module;
			int divisorIndex;
			void onAction(const event::Action &e) override
			{
				module->divisorIndex = divisorIndex;
			}
		};

		std::string divisorNames[13] = {"1:1 (1/96)", "3:1 (1/32)", "6:1 (1/16)", "12:1 (1/8)", "24:1 (1/4)", "48:1 (1/2)", "96:1 (1/1)", "2:1 (1/32T)", "4:1 (1/16T)", "8:1 (1/8T)", "16:1 (1/4T)", "32:1 (1/2T)", "64:1 (1/1T)"};
		for (int i = 0; i < 13; i++)
		{
			DivisorItem *divisorItem = createMenuItem<DivisorItem>(divisorNames[i]);
			divisorItem->rightText = CHECKMARK(module->divisorIndex == i);
			divisorItem->module = module;
			divisorItem->divisorIndex = i;
			menu->addChild(divisorItem);
		}
	}
};

Model *modelSeqtrol = createModel<Seqtrol, SeqtrolWidget>("Seqtrol");
