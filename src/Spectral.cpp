#include "plugin.hpp"


struct Spectral : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		FREQ_INPUT,
		AUDIO_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Spectral() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(FREQ_INPUT, "");
		configInput(AUDIO_INPUT, "");
		configOutput(AUDIO_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
	}
};


struct SpectralWidget : ModuleWidget {
	SpectralWidget(Spectral* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Spectral.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 36.0)), module, Spectral::FREQ_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)), module, Spectral::AUDIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Spectral::AUDIO_OUTPUT));
	}
};


Model* modelSpectral = createModel<Spectral, SpectralWidget>("Spectral");