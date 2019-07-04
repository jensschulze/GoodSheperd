#include "GoodSheperd.hpp"
#include "dsp/digital.hpp"

struct Hurdle : Module
{
	enum ParamIds
	{
		NUM_PARAMS
	};
	enum InputIds
	{
		PROBABILITY_INPUT,
		GATE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds
	{
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds
	{
		NUM_LIGHTS
	};

	bool isOpen = false;
	bool lastGateInWasHigh = false;

	Hurdle() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step() override;
};

void Hurdle::step()
{
	float probability = inputs[PROBABILITY_INPUT].value;
	probability = clamp(probability, 0.0f, 10.0f);

	float gateInValue = inputs[GATE_INPUT].value;
	bool gateInIsHigh = gateInValue >= 1.0f;

	if (isOpen)
	{
		// Gate is open
		if (true == gateInIsHigh)
		{
			// Input is high: keep gate open
			isOpen = true;
		}
		else
		{
			// Input is low: close gate
			isOpen = false;
		}
	}
	else
	{
		// Gate closed. Will open only at rising edge
		if (gateInIsHigh && !lastGateInWasHigh)
		{
			// Make a decision!
			if (probability >= randomUniform() * 10.0f)
			{
				isOpen = true;
			}
		}
	}

	if (isOpen)
	{
		outputs[GATE_OUTPUT].value = 10.0f;
	}
	else
	{
		outputs[GATE_OUTPUT].value = 0.0f;
	}

	lastGateInWasHigh = gateInIsHigh;
}

struct HurdleWidget : ModuleWidget
{
	HurdleWidget(Hurdle *module) : ModuleWidget(module)
	{
		setPanel(SVG::load(assetPlugin(plugin, "res/Hurdle.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(Port::create<PJ301MPort>(Vec(11, 97), Port::INPUT, module, Hurdle::PROBABILITY_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(11, 237), Port::INPUT, module, Hurdle::GATE_INPUT));
		addOutput(Port::create<PJ301MPort>(Vec(11, 293), Port::OUTPUT, module, Hurdle::GATE_OUTPUT));
	}
};

// Specify the Module and ModuleWidget subclass plus human-readable module name
Model *modelHurdle = Model::create<Hurdle, HurdleWidget>("Hurdle");
