#include "plugin.hpp"
#include "RepoolCore.hpp"

struct Repool : Module
{
	enum ParamId
	{
		MIX_PARAM,
		ENUMS(REC_PARAM, 4),
		ENUMS(OVERDUB_PARAM, 4),
		ENUMS(UNDO_PARAM, 4),
		ENUMS(CLEAR_PARAM, 4),
		PARAMS_LEN
	};
	enum InputId
	{
		AUDIO_INPUT,
		ENUMS(REC_INPUT, 4),
		ENUMS(OVERDUB_INPUT, 4),
		ENUMS(UNDO_INPUT, 4),
		ENUMS(CLEAR_INPUT, 4),
		CLOCK_INPUT,
		INPUTS_LEN
	};
	enum OutputId
	{
		AUDIO_OUTPUT,
		POS_OUTPUT,
		LOOP_START_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId
	{
		ENUMS(STATE_LIGHT, 4 * 3), // 4 columns × RGB
		LIGHTS_LEN
	};

	enum class Mode
	{
		STACK,
		SPREAD
	};

	std::vector<LoopTrack> tracks;

	dsp::SchmittTrigger recGate[4];
	dsp::SchmittTrigger overdubGate[4];
	dsp::SchmittTrigger undoTrig[4];
	dsp::SchmittTrigger clearTrig[4];
	dsp::BooleanTrigger recEdge[4];
	dsp::BooleanTrigger overdubEdge[4];
	dsp::ClockDivider lightDivider;

	// Clock quantization
	dsp::SchmittTrigger clockTrig;
	int clockTimer = 0;
	float clockPeriod = 0.0f;
	bool pendingStop[4] = {};
	int targetStop[4] = {};

	// Sync outputs
	dsp::PulseGenerator loopStartPulse;

	// Operating mode
	Mode mode = Mode::STACK;

	// Stack-mode option: lock all track lengths to the first committed loop
	bool linked = false;

	Repool()
	{
		const float sr = APP->engine->getSampleRate();
		tracks.reserve(4);
		for (int i = 0; i < 4; i++)
			tracks.emplace_back(sr);

		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(MIX_PARAM, 0.0f, 1.0f, 0.5f, "Mix");
		for (int i = 0; i < 4; i++)
		{
			configButton(REC_PARAM + i, string::f("Track %d Record", i + 1));
			configButton(OVERDUB_PARAM + i, string::f("Track %d Overdub", i + 1));
			configButton(UNDO_PARAM + i, string::f("Track %d Undo", i + 1));
			configButton(CLEAR_PARAM + i, string::f("Track %d Clear", i + 1));
			configInput(REC_INPUT + i, string::f("Track %d Record gate", i + 1));
			configInput(OVERDUB_INPUT + i, string::f("Track %d Overdub gate", i + 1));
			configInput(UNDO_INPUT + i, string::f("Track %d Undo trigger", i + 1));
			configInput(CLEAR_INPUT + i, string::f("Track %d Clear trigger", i + 1));
		}
		configInput(AUDIO_INPUT, "Audio");
		configInput(CLOCK_INPUT, "Clock");
		configOutput(AUDIO_OUTPUT, "Audio");
		configOutput(POS_OUTPUT, "Position CV");
		configOutput(LOOP_START_OUTPUT, "Loop start trigger");

		lightDivider.setDivision(512);
	}

	void onSampleRateChange() override
	{
		const float sr = APP->engine->getSampleRate();
		for (auto &t : tracks)
			t.setSampleRate(sr);
		clockPeriod = 0.0f;
		clockTimer = 0;
		for (int i = 0; i < 4; i++)
			pendingStop[i] = false;
	}

	void onReset() override
	{
		for (auto &t : tracks)
			t.clear();
		for (int i = 0; i < 4; i++)
			pendingStop[i] = false;
		clockPeriod = 0.0f;
		clockTimer = 0;
	}

	json_t *dataToJson() override
	{
		json_t *root = json_object();
		json_object_set_new(root, "mode", json_integer(static_cast<int>(mode)));
		json_object_set_new(root, "linked", json_boolean(linked));
		return root;
	}

	void dataFromJson(json_t *root) override
	{
		json_t *v;
		v = json_object_get(root, "mode");
		if (v)
			mode = static_cast<Mode>(json_integer_value(v));
		v = json_object_get(root, "linked");
		if (v)
			linked = json_boolean_value(v);
	}

	// ── Helpers ──────────────────────────────────────────────────────────

	// Returns the loop length (samples) of the first playing/overdubbing track,
	// or 0 if no track has a committed loop yet.
	int masterLoopLength() const
	{
		for (const auto &t : tracks)
		{
			int len = t.getLoopLength();
			if (len > 0)
				return len;
		}
		return 0;
	}

	// Commit recording for track i, quantizing to nearest clock beat if available.
	// Schedules a deferred stop (pendingStop[i]) when rounding up.
	void stopRecordQuantized(int i)
	{
		if (tracks[i].getState() != LoopTrack::State::RECORDING)
			return;
		if (clockPeriod > 0.0f)
		{
			int wp = tracks[i].getWritePos();
			int beats = std::max(1, (int)std::round((float)wp / clockPeriod));
			int target = (int)(beats * clockPeriod);
			int maxSmp = (int)(LoopTrack::MAX_DURATION * APP->engine->getSampleRate());
			if (target <= wp || target > maxSmp)
			{
				if (target < wp)
					tracks[i].setWritePos(target);
				tracks[i].stopRecord();
			}
			else
			{
				pendingStop[i] = true;
				targetStop[i] = target;
			}
		}
		else
		{
			tracks[i].stopRecord();
			pendingStop[i] = false;
		}
	}

	// ── DSP ──────────────────────────────────────────────────────────────

	void process(const ProcessArgs &args) override
	{
		// ── Clock measurement (shared by both modes) ──────────────────
		if (inputs[CLOCK_INPUT].isConnected())
		{
			clockTimer++;
			if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.0f))
			{
				if (clockTimer > 0)
					clockPeriod = (float)clockTimer;
				clockTimer = 0;
			}
		}
		else
		{
			clockPeriod = 0.0f;
			clockTimer = 0;
		}

		if (mode == Mode::SPREAD)
		{
			processSpread();
		}
		else
		{
			processStack();
		}

		// ── Audio mix ─────────────────────────────────────────────────
		float dryIn = inputs[AUDIO_INPUT].getVoltage();
		float loopSum = 0.0f;

		// In spread mode only tracks[0] is active; others remain silent.
		int trackCount = (mode == Mode::SPREAD) ? 1 : 4;

		// Sample master playhead BEFORE advancing (detects wrap-around)
		int masterIdx = -1;
		int prevPlayhead = -1;
		for (int i = 0; i < trackCount; i++)
		{
			if (tracks[i].hasContent())
			{
				masterIdx = i;
				break;
			}
		}
		if (masterIdx >= 0)
			prevPlayhead = tracks[masterIdx].getPlayhead();

		for (int i = 0; i < trackCount; i++)
			loopSum += tracks[i].process(dryIn);

		float mix = params[MIX_PARAM].getValue();
		outputs[AUDIO_OUTPUT].setVoltage(dryIn * (1.0f - mix) + loopSum * mix);

		// ── Sync outputs ──────────────────────────────────────────────
		if (masterIdx >= 0)
		{
			outputs[POS_OUTPUT].setVoltage(tracks[masterIdx].getPosition() * 10.0f);
			int currPlayhead = tracks[masterIdx].getPlayhead();
			if (prevPlayhead > 0 && currPlayhead < prevPlayhead)
				loopStartPulse.trigger(1e-3f);
		}
		else
		{
			outputs[POS_OUTPUT].setVoltage(0.0f);
		}
		outputs[LOOP_START_OUTPUT].setVoltage(
				loopStartPulse.process(args.sampleTime) ? 10.0f : 0.0f);

		// ── Lights ────────────────────────────────────────────────────
		if (lightDivider.process())
		{
			if (mode == Mode::SPREAD)
			{
				auto s = tracks[0].getState();
				int lc = tracks[0].getLayerCount();
				for (int i = 0; i < 4; i++)
				{
					bool rec = (s == LoopTrack::State::RECORDING && i == 0);
					bool ovd = (s == LoopTrack::State::OVERDUBBING && i == lc);
					bool play = !rec && !ovd && ((i == 0 && s != LoopTrack::State::EMPTY) || (i > 0 && i <= lc));
					lights[STATE_LIGHT + i * 3 + 0].setBrightness(rec ? 1.0f : 0.0f);
					lights[STATE_LIGHT + i * 3 + 1].setBrightness(play ? 0.5f : 0.0f);
					lights[STATE_LIGHT + i * 3 + 2].setBrightness(ovd ? 1.0f : 0.0f);
				}
			}
			else
			{
				for (int i = 0; i < 4; i++)
				{
					auto s = tracks[i].getState();
					lights[STATE_LIGHT + i * 3 + 0].setBrightness(
							s == LoopTrack::State::RECORDING ? 1.0f : 0.0f);
					lights[STATE_LIGHT + i * 3 + 1].setBrightness(
							s == LoopTrack::State::PLAYING ? 0.5f : 0.0f);
					lights[STATE_LIGHT + i * 3 + 2].setBrightness(
							s == LoopTrack::State::OVERDUBBING ? 1.0f : 0.0f);
				}
			}
		}
	}

private:
	// ── Stack mode: 4 fully independent tracks ────────────────────────
	void processStack()
	{
		for (int i = 0; i < 4; i++)
		{
			// REC gate (hold-to-record, button OR CV)
			float recV = std::max(params[REC_PARAM + i].getValue() * 10.0f,
														inputs[REC_INPUT + i].getVoltage());
			recGate[i].process(recV, 0.1f, 2.0f);
			{
				auto e = recEdge[i].processEvent(recGate[i].isHigh());
				if (e == dsp::BooleanTrigger::TRIGGERED)
				{
					tracks[i].startRecord();
					pendingStop[i] = false;
					// If linked and a master length exists, auto-stop at that length
					if (linked)
					{
						int master = masterLoopLength();
						if (master > 0)
						{
							pendingStop[i] = true;
							targetStop[i] = master;
						}
					}
				}
				if (e == dsp::BooleanTrigger::UNTRIGGERED)
				{
					// If a linked pendingStop is scheduled, honour it (ignore early release)
					if (!pendingStop[i])
						stopRecordQuantized(i);
				}
			}

			// Fire deferred / linked stop
			if (pendingStop[i] && tracks[i].getState() == LoopTrack::State::RECORDING && tracks[i].getWritePos() >= targetStop[i])
			{
				tracks[i].setWritePos(targetStop[i]);
				tracks[i].stopRecord();
				pendingStop[i] = false;
			}

			// OVERDUB gate (hold-to-overdub, button OR CV)
			float ovdV = std::max(params[OVERDUB_PARAM + i].getValue() * 10.0f,
														inputs[OVERDUB_INPUT + i].getVoltage());
			overdubGate[i].process(ovdV, 0.1f, 2.0f);
			{
				auto e = overdubEdge[i].processEvent(overdubGate[i].isHigh());
				if (e == dsp::BooleanTrigger::TRIGGERED)
					tracks[i].startOverdub();
				if (e == dsp::BooleanTrigger::UNTRIGGERED)
					tracks[i].stopOverdub();
			}

			// UNDO / CLEAR (momentary, button OR CV)
			float undoV = std::max(params[UNDO_PARAM + i].getValue() * 10.0f,
														 inputs[UNDO_INPUT + i].getVoltage());
			float clrV = std::max(params[CLEAR_PARAM + i].getValue() * 10.0f,
														inputs[CLEAR_INPUT + i].getVoltage());
			if (undoTrig[i].process(undoV, 0.1f, 2.0f))
				tracks[i].undo();
			if (clearTrig[i].process(clrV, 0.1f, 2.0f))
			{
				tracks[i].clear();
				pendingStop[i] = false;
			}
		}
	}

	// ── Spread mode: one loop, 4 columns = layer versions ─────────────
	// Column 0 = base recording. Columns 1–3 = overdub layers 1–3.
	// Pressing an occupied column rewinds to that layer version.
	// Pressing the next empty column adds the next overdub.
	void processSpread()
	{
		auto &t0 = tracks[0];
		// max overdubs in spread = 3 (4 columns: base + 3 overdubs)
		static constexpr int SPREAD_MAX_LAYERS = 3;

		for (int i = 0; i < 4; i++)
		{
			float recV = std::max(params[REC_PARAM + i].getValue() * 10.0f,
														inputs[REC_INPUT + i].getVoltage());
			recGate[i].process(recV, 0.1f, 2.0f);
			auto e = recEdge[i].processEvent(recGate[i].isHigh());

			if (e == dsp::BooleanTrigger::TRIGGERED)
			{
				auto s = t0.getState();
				int lc = t0.getLayerCount();
				if (i == 0 && s == LoopTrack::State::EMPTY)
				{
					// Start the base recording
					t0.startRecord();
					pendingStop[0] = false;
				}
				else if (s == LoopTrack::State::PLAYING && i == lc && lc < SPREAD_MAX_LAYERS)
				{
					// Add the next overdub layer
					t0.startOverdub();
				}
				else if (s != LoopTrack::State::EMPTY)
				{
					// Rewind to version i: discard in-progress overdub first, then pop
					if (s == LoopTrack::State::OVERDUBBING)
						t0.undo();
					while (t0.getLayerCount() > i)
						t0.undo();
					pendingStop[0] = false;
				}
			}

			if (e == dsp::BooleanTrigger::UNTRIGGERED)
			{
				if (t0.getState() == LoopTrack::State::RECORDING)
					stopRecordQuantized(0);
				else if (t0.getState() == LoopTrack::State::OVERDUBBING)
					t0.stopOverdub();
			}
		}

		// Fire deferred quantized stop
		if (pendingStop[0] && t0.getState() == LoopTrack::State::RECORDING && t0.getWritePos() >= targetStop[0])
		{
			t0.setWritePos(targetStop[0]);
			t0.stopRecord();
			pendingStop[0] = false;
		}

		// UNDO / CLEAR: any column → tracks[0]
		for (int i = 0; i < 4; i++)
		{
			float undoV = std::max(params[UNDO_PARAM + i].getValue() * 10.0f,
														 inputs[UNDO_INPUT + i].getVoltage());
			float clrV = std::max(params[CLEAR_PARAM + i].getValue() * 10.0f,
														inputs[CLEAR_INPUT + i].getVoltage());
			if (undoTrig[i].process(undoV, 0.1f, 2.0f))
				t0.undo();
			if (clearTrig[i].process(clrV, 0.1f, 2.0f))
			{
				t0.clear();
				pendingStop[0] = false;
			}
		}
	}
};

struct RepoolWidget : ModuleWidget
{
	RepoolWidget(Repool *module)
	{
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Repool.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// ── 4-track controls ─────────────────────────────────────────
		// Column centers (x mm): 20, 40, 60, 80
		// Row centers (y mm):
		//   LED=20, REC btn=28, REC cv=36, OVD btn=44, OVD cv=52,
		//   UND btn=60, UND cv=68, CLR btn=76, CLR cv=84
		const float trackX[4] = {20.f, 40.f, 60.f, 80.f};

		for (int i = 0; i < 4; i++)
		{
			float x = trackX[i];

			addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(
					mm2px(Vec(x, 20.f)), module, Repool::STATE_LIGHT + i * 3));

			addParam(createParamCentered<VCVButton>(
					mm2px(Vec(x, 28.f)), module, Repool::REC_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(
					mm2px(Vec(x, 36.f)), module, Repool::REC_INPUT + i));

			addParam(createParamCentered<VCVButton>(
					mm2px(Vec(x, 44.f)), module, Repool::OVERDUB_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(
					mm2px(Vec(x, 52.f)), module, Repool::OVERDUB_INPUT + i));

			addParam(createParamCentered<VCVButton>(
					mm2px(Vec(x, 60.f)), module, Repool::UNDO_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(
					mm2px(Vec(x, 68.f)), module, Repool::UNDO_INPUT + i));

			addParam(createParamCentered<VCVButton>(
					mm2px(Vec(x, 76.f)), module, Repool::CLEAR_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(
					mm2px(Vec(x, 84.f)), module, Repool::CLEAR_INPUT + i));
		}

		// ── Global controls ───────────────────────────────────────────
		addParam(createParamCentered<RoundBlackKnob>(
				mm2px(Vec(28.f, 98.f)), module, Repool::MIX_PARAM));
		addInput(createInputCentered<PJ301MPort>(
				mm2px(Vec(74.f, 98.f)), module, Repool::CLOCK_INPUT));

		// ── Audio I/O + sync outputs ──────────────────────────────────
		addInput(createInputCentered<PJ301MPort>(
				mm2px(Vec(13.f, 114.f)), module, Repool::AUDIO_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(33.f, 114.f)), module, Repool::AUDIO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(68.f, 114.f)), module, Repool::POS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(
				mm2px(Vec(88.f, 114.f)), module, Repool::LOOP_START_OUTPUT));
	}

	void appendContextMenu(Menu *menu) override
	{
		auto *m = getModule<Repool>();

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Mode"));

		menu->addChild(createMenuItem("Stack — independent tracks",
																	CHECKMARK(m->mode == Repool::Mode::STACK),
																	[m]()
																	{ m->mode = Repool::Mode::STACK; }));

		menu->addChild(createMenuItem("Spread — undo layers",
																	CHECKMARK(m->mode == Repool::Mode::SPREAD),
																	[m]()
																	{ m->mode = Repool::Mode::SPREAD; }));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Stack options"));
		menu->addChild(createMenuItem("Link loop lengths",
																	CHECKMARK(m->linked),
																	[m]()
																	{ m->linked = !m->linked; }));
	}
};

Model *modelRepool = createModel<Repool, RepoolWidget>("Repool");
