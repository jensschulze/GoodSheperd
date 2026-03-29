#include "plugin.hpp"
#include <cmath>


// 2-pole State Variable Filter (ZDF, Andy Simper / Cytomic formulation).
// Unconditionally stable for any Q and any fc < sr/2.
// BP output has unity peak gain at fc regardless of Q.
struct SVFBandpass {
	float ic1eq = 0.f;  // integrator 1 state
	float ic2eq = 0.f;  // integrator 2 state
	float g = 0.f;      // tan(π·fc/sr)
	float k = 0.f;      // damping = 1/Q

	void setParams(float fc, float sampleRate, float Q) {
		fc = clamp(fc, 10.f, sampleRate * 0.49f);
		g = std::tan(M_PI * fc / sampleRate);
		k = 1.f / std::max(Q, 0.5f);
	}

	float process(float v0) {
		float v1 = (ic1eq + g * (v0 - ic2eq)) / (1.f + g * (g + k));
		float v2 = ic2eq + g * v1;
		ic1eq = 2.f * v1 - ic1eq;
		ic2eq = 2.f * v2 - ic2eq;
		return v1;  // bandpass output, peak gain = 1 at fc
	}

	void reset() {
		ic1eq = ic2eq = 0.f;
	}
};


struct Spectral : Module {
	enum ParamId {
		Q_PARAM,
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

	static constexpr int MAX_HARMONICS = 16;

	SVFBandpass filters[MAX_HARMONICS];
	int numHarmonics = 8;

	// Cached values to detect changes and avoid redundant recalculation
	float lastFreq = -1.f;
	float lastQ    = -1.f;

	Spectral() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(Q_PARAM, 1.f, 40.f, 10.f, "Resonance (Q)");
		configInput(FREQ_INPUT, "Frequency (1V/oct)");
		configInput(AUDIO_INPUT, "Audio");
		configOutput(AUDIO_OUTPUT, "Audio");
	}

	void process(const ProcessArgs& args) override {
		float cv = inputs[FREQ_INPUT].getVoltage();
		float f0 = dsp::FREQ_C4 * std::pow(2.f, cv);
		float Q  = params[Q_PARAM].getValue();

		// Recompute filter parameters only when inputs change
		if (f0 != lastFreq || Q != lastQ) {
			for (int n = 0; n < numHarmonics; n++) {
				float fc = f0 * (n + 1);
				if (fc < args.sampleRate * 0.45f) {
					filters[n].setParams(fc, args.sampleRate, Q);
				}
			}
			lastFreq = f0;
			lastQ    = Q;
		}

		float in  = inputs[AUDIO_INPUT].getVoltage();
		float out = 0.f;
		int active = 0;
		for (int n = 0; n < numHarmonics; n++) {
			float fc = f0 * (n + 1);
			if (fc < args.sampleRate * 0.45f) {
				out += filters[n].process(in);
				active++;
			}
		}
		if (active > 0)
			out /= active;

		outputs[AUDIO_OUTPUT].setVoltage(out);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		lastFreq = -1.f;
		lastQ    = -1.f;
		for (int n = 0; n < MAX_HARMONICS; n++)
			filters[n].reset();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "numHarmonics", json_integer(numHarmonics));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* v = json_object_get(rootJ, "numHarmonics");
		if (v) numHarmonics = clamp((int)json_integer_value(v), 1, MAX_HARMONICS);
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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 36.0)),   module, Spectral::FREQ_INPUT));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(10.16, 52.5)), module, Spectral::Q_PARAM));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)),   module, Spectral::AUDIO_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Spectral::AUDIO_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Spectral* module = getModule<Spectral>();
		if (!module) return;

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Harmonics"));

		for (int n : {8, 16}) {
			menu->addChild(createCheckMenuItem(
				std::to_string(n) + " harmonics", "",
				[=]() { return module->numHarmonics == n; },
				[=]() {
					module->numHarmonics = n;
					module->lastFreq = -1.f;  // force filter recalculation
				}
			));
		}
	}
};


Model* modelSpectral = createModel<Spectral, SpectralWidget>("Spectral");