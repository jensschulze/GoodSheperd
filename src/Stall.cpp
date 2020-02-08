#include "plugin.hpp"

struct Stall : Module
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		CV_IN,
		GATE_IN,
		NUM_INPUTS
	};
	enum OutputIds
	{
		ENUMS(GATE_OUT, 48),
		NUM_OUTPUTS
	};
	enum LightIds
	{
		ENUMS(GATE_LIGHT, 48),
		NUM_LIGHTS
	};

	float cvStep[48];

	Stall()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float oneTwelfth = 1.0 / 12.0;
		for (int i = 0; i < 48; i++)
		{
			cvStep[i] = (float)(i - 25) * oneTwelfth;
		}
	}

	int getNoteNumberFromCv(float cv)
	{
		for (int i = 0; i < 47; i++)
		{
			if (cvStep[i] <= cv && cv < cvStep[i + 1])
			{
				return i;
			}
		}

		// Wrong, but …
		return 0;
	}

	void process(const ProcessArgs &args) override
	{
		float gateOuts[48] = {0};

		int channels = std::max(inputs[CV_IN].getChannels(), 1);

		for (int c = 0; c < channels; c++)
		{
			if (inputs[CV_IN].isConnected() && inputs[GATE_IN].isConnected())
			{
				float cv = inputs[CV_IN].getPolyVoltage(c);
				float gate = inputs[GATE_IN].getPolyVoltage(c);

				gateOuts[getNoteNumberFromCv(cv)] = gate;
			}
		}

		for (int i = 0; i < 48; i++)
		{
			outputs[GATE_OUT + i].setVoltage(gateOuts[i]);
			lights[GATE_LIGHT + i].value = outputs[GATE_OUT + i].value / 10.0f;
		}
	}
};

struct StallWidget : ModuleWidget
{
	StallWidget(Stall *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Stall.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		static const float outGridX[8] = {34.639, 48.165, 61.691, 75.217, 88.743, 102.269, 115.795, 129.321};
		static const float outGridY[6] = {117.973, 99.375, 80.777, 62.179, 43.581, 24.983};
		static const float lightGridY[6] = {111.623, 93.025, 74.427, 55.829, 37.231, 18.633};

		for (int y = 0; y < 6; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(outGridX[x], outGridY[y])), module, Stall::GATE_OUT + x + 8 * y));
				addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(outGridX[x], lightGridY[y])), module, Stall::GATE_LIGHT + x + 8 * y));
			}
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.586, outGridY[5])), module, Stall::CV_IN));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.586, outGridY[4])), module, Stall::GATE_IN));
	}
};

Model *modelStall = createModel<Stall, StallWidget>("Stall");