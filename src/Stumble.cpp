#include "GoodSheperd.hpp"


struct Stumble : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		PROBABILITY_INPUT,
		GATE_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool isOpen = false;
	bool lastGateInWasHigh = false;

	Stumble()
	{
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	void process(const ProcessArgs &args) override {
		float probability = inputs[PROBABILITY_INPUT].getVoltage();
		probability = clamp(probability, 0.0f, 10.0f);

		float gateInValue = inputs[GATE_INPUT].getVoltage();
		bool gateInIsHigh = gateInValue >= 1.0f;

		if (isOpen) {
			// Gate is open
			if (true == gateInIsHigh) {
				// Input is high: keep gate open
				isOpen = true;
			} else {
				// Input is low: close gate
				isOpen = false;
			}
		} else {
			// Gate closed. Will open only at rising edge
			if (gateInIsHigh && !lastGateInWasHigh) {
				// Make a decision!
				if (probability >= random::uniform() * 10.0f) {
					isOpen = true;
				}
			}
		}

		if (isOpen) {
			outputs[GATE_OUTPUT].setVoltage(10.0f);
		} else {
			outputs[GATE_OUTPUT].setVoltage(0.0f);
		}

		lastGateInWasHigh = gateInIsHigh;
	}
};

struct StumbleWidget : ModuleWidget
{
	struct DisplayWidget : TransparentWidget
	{
		Stumble *module;
		std::shared_ptr<Font> font;

        DisplayWidget()
		{
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/DSEG14Classic-RegularItalic.ttf"));
		}

		void draw(const DrawArgs &args) override
		{
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			Vec textPos = Vec(3, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, 30.f));
			nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);
			nvgFillColor(args.vg, textColor);
			nvgText(args.vg, textPos.x, textPos.y, "!1", NULL);
		}
	};

	StumbleWidget(Stumble *module)
	{
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Stumble.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInput<PJ301MPort>(Vec(11, 97), module, Stumble::PROBABILITY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(11, 237), module, Stumble::GATE_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(11, 293), module, Stumble::GATE_OUTPUT));

		DisplayWidget *displayNumber = new DisplayWidget();
		displayNumber->box.pos = Vec(5, 36);
		displayNumber->box.size = Vec(36, 30);// 3 characters
		displayNumber->module = module;
		addChild(displayNumber);
	}
};

// Specify the Module and ModuleWidget subclass plus human-readable module name
Model *modelStumble = createModel<Stumble, StumbleWidget>("Stumble");
