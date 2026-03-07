#include "plugin.hpp"
#include <vector>

struct Resrever : Module
{
	enum ParamId
	{
		REVERSE_BUTTON_PARAM,
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		AUDIO_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		LIGHTS_LEN
	};

	// Ring buffer for 4 seconds of audio
	static constexpr float BUFFER_DURATION = 4.0f;
	std::vector<float> buffer;
	int writePos = 0;
	int readPos = 0;
	int bufferSize = 0;

	Resrever()
	{
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(REVERSE_BUTTON_PARAM, 0.f, 1.f, 0.f, "Reverse");
		configInput(AUDIO_INPUT, "Audio Input");
		configOutput(AUDIO_OUTPUT, "Audio Output");

		// Initialize buffer with default sample rate
		onSampleRateChange();
	}

	void onSampleRateChange() override
	{
		// Calculate buffer size for 4 seconds at current sample rate
		bufferSize = (int)(BUFFER_DURATION * APP->engine->getSampleRate());
		buffer.resize(bufferSize, 0.f);
		writePos = 0;
		readPos = 0;
	}

	void process(const ProcessArgs &args) override
	{
		bool reversePressed = params[REVERSE_BUTTON_PARAM].getValue() > 0.5f;
		float input = inputs[AUDIO_INPUT].getVoltage();

		if (!reversePressed)
		{
			// Bypass mode: output = input
			outputs[AUDIO_OUTPUT].setVoltage(input);

			// Write input to ring buffer
			buffer[writePos] = input;
			writePos = (writePos + 1) % bufferSize;

			// Set read position to most recent sample (one behind write position)
			// so when reverse is engaged, we start from the most recent audio
			readPos = (writePos - 1 + bufferSize) % bufferSize;
		}
		else
		{
			// Reverse mode: read buffer backwards
			outputs[AUDIO_OUTPUT].setVoltage(buffer[readPos]);

			// Move read position backwards through the buffer
			readPos = (readPos - 1 + bufferSize) % bufferSize;
		}
	}
};

struct ResreverWidget : ModuleWidget
{
	ResreverWidget(Resrever *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Resrever.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(10.16, 36.0)), module, Resrever::REVERSE_BUTTON_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 69.0)), module, Resrever::AUDIO_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.16, 102.0)), module, Resrever::AUDIO_OUTPUT));
	}
};

Model *modelResrever = createModel<Resrever, ResreverWidget>("Resrever");
