#include "plugin.hpp"
#include "RepoolCore.hpp"

struct Repool : Module
{
  enum ParamId
  {
    MIX_PARAM,
    ENUMS(SELECT_PARAM, 4),
    ENUMS(ACTIVE_PARAM, 4),
    REC_PARAM,
    OVD_PARAM,
    UND_PARAM,
    CLR_PARAM,
    PARAMS_LEN
  };
  enum InputId
  {
    AUDIO_INPUT,
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
    ENUMS(SELECTED_LIGHT, 4),
    ENUMS(STATE_LIGHT, 4 * 3), // 4 columns × RGB
    ENUMS(ACTIVE_LIGHT, 4),
    POS_LIGHT,
    LIGHTS_LEN
  };

  enum class Mode
  {
    STACK,
    SPREAD
  };

  std::vector<LoopTrack> tracks;

  dsp::BooleanTrigger selEdge[4];
  dsp::BooleanTrigger recEdge, ovdEdge, undEdge, clrEdge;
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

  // Currently selected track — all actions target this track
  int selectedTrack = 0;

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
      configButton(SELECT_PARAM + i, string::f("Select track %d", i + 1));
      configSwitch(ACTIVE_PARAM + i, 0.f, 1.f, 1.f,
                   string::f("Track %d active", i + 1), {"Muted", "Active"});
    }
    configButton(REC_PARAM, "Record");
    configButton(OVD_PARAM, "Overdub");
    configButton(UND_PARAM, "Undo");
    configButton(CLR_PARAM, "Clear");
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
    {
      pendingStop[i] = false;
      params[ACTIVE_PARAM + i].setValue(1.0f);
    }
    clockPeriod = 0.0f;
    clockTimer = 0;
    selectedTrack = 0;
  }

  json_t *dataToJson() override
  {
    json_t *root = json_object();
    json_object_set_new(root, "mode", json_integer(static_cast<int>(mode)));
    json_object_set_new(root, "linked", json_boolean(linked));
    json_object_set_new(root, "selectedTrack", json_integer(selectedTrack));
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
    v = json_object_get(root, "selectedTrack");
    if (v)
      selectedTrack = clamp((int)json_integer_value(v), 0, 3);
  }

  // ── Helpers ──────────────────────────────────────────────────────────

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

  // Track 0 is always selectable. Track i (i>0) is selectable when at least
  // i tracks have content — this unlocks them sequentially as you record.
  bool canSelect(int i) const
  {
    if (i == 0)
      return true;
    int nonEmpty = 0;
    for (const auto &t : tracks)
      if (t.getLoopLength() > 0)
        nonEmpty++;
    return nonEmpty >= i;
  }

  // Selection is blocked while the selected track is recording or overdubbing.
  bool selectionBlocked() const
  {
    auto s = tracks[selectedTrack].getState();
    return s == LoopTrack::State::RECORDING || s == LoopTrack::State::OVERDUBBING;
  }

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
    // ── Clock measurement ─────────────────────────────────────────
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
      processSpread();
    else
      processStack();

    // ── Audio mix with per-track muting ───────────────────────────
    float dryIn = inputs[AUDIO_INPUT].getVoltage();
    float loopSum = 0.0f;

    int trackCount = (mode == Mode::SPREAD) ? 1 : 4;

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
    {
      float trackOut = tracks[i].process(dryIn);
      if (params[ACTIVE_PARAM + i].getValue() > 0.5f)
        loopSum += trackOut;
    }

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
      for (int i = 0; i < 4; i++)
        lights[SELECTED_LIGHT + i].setBrightness(i == selectedTrack ? 1.0f : 0.0f);

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

      for (int i = 0; i < 4; i++)
        lights[ACTIVE_LIGHT + i].setBrightness(params[ACTIVE_PARAM + i].getValue());

      float pos = (masterIdx >= 0) ? tracks[masterIdx].getPosition() : 0.0f;
      lights[POS_LIGHT].setBrightness(pos);
    }
  }

private:
  // ── Stack mode: 4 independent tracks, one selected at a time ─────────
  void processStack()
  {
    // Handle select buttons (blocked while recording/overdubbing)
    if (!selectionBlocked())
    {
      for (int i = 0; i < 4; i++)
      {
        if (selEdge[i].process(params[SELECT_PARAM + i].getValue() > 0.5f) && canSelect(i))
          selectedTrack = i;
      }
    }

    auto &st = tracks[selectedTrack];

    // REC (hold-to-record)
    auto recEvent = recEdge.processEvent(params[REC_PARAM].getValue() > 0.5f);
    if (recEvent == dsp::BooleanTrigger::TRIGGERED)
    {
      st.startRecord();
      pendingStop[selectedTrack] = false;
      if (linked)
      {
        int master = masterLoopLength();
        if (master > 0)
        {
          pendingStop[selectedTrack] = true;
          targetStop[selectedTrack] = master;
        }
      }
    }
    if (recEvent == dsp::BooleanTrigger::UNTRIGGERED)
    {
      if (!pendingStop[selectedTrack])
        stopRecordQuantized(selectedTrack);
    }

    // OVD (hold-to-overdub)
    auto ovdEvent = ovdEdge.processEvent(params[OVD_PARAM].getValue() > 0.5f);
    if (ovdEvent == dsp::BooleanTrigger::TRIGGERED)
      st.startOverdub();
    if (ovdEvent == dsp::BooleanTrigger::UNTRIGGERED)
      st.stopOverdub();

    // UND / CLR (momentary)
    if (undEdge.process(params[UND_PARAM].getValue() > 0.5f))
      st.undo();
    if (clrEdge.process(params[CLR_PARAM].getValue() > 0.5f))
    {
      st.clear();
      pendingStop[selectedTrack] = false;
      // Fall back to track 0 if this track is no longer selectable
      if (!canSelect(selectedTrack))
        selectedTrack = 0;
    }

    // Deferred / linked stop
    for (int i = 0; i < 4; i++)
    {
      if (pendingStop[i] &&
          tracks[i].getState() == LoopTrack::State::RECORDING &&
          tracks[i].getWritePos() >= targetStop[i])
      {
        tracks[i].setWritePos(targetStop[i]);
        tracks[i].stopRecord();
        pendingStop[i] = false;
      }
    }
  }

  // ── Spread mode: one loop, columns = layer versions ───────────────────
  // SEL buttons rewind to that layer version. REC/OVD act on tracks[0].
  void processSpread()
  {
    auto &t0 = tracks[0];
    static constexpr int SPREAD_MAX_LAYERS = 3;

    // SEL buttons: rewind to layer i (blocked during rec/ovd)
    bool isActive = (t0.getState() == LoopTrack::State::RECORDING ||
                     t0.getState() == LoopTrack::State::OVERDUBBING);
    if (!isActive)
    {
      for (int i = 0; i < 4; i++)
      {
        if (selEdge[i].process(params[SELECT_PARAM + i].getValue() > 0.5f) &&
            t0.getState() != LoopTrack::State::EMPTY)
        {
          while (t0.getLayerCount() > i)
            t0.undo();
          selectedTrack = i;
        }
      }
    }

    // REC (hold to record base)
    auto recEvent = recEdge.processEvent(params[REC_PARAM].getValue() > 0.5f);
    if (recEvent == dsp::BooleanTrigger::TRIGGERED)
    {
      if (t0.getState() == LoopTrack::State::EMPTY)
      {
        t0.startRecord();
        pendingStop[0] = false;
      }
    }
    if (recEvent == dsp::BooleanTrigger::UNTRIGGERED)
    {
      if (t0.getState() == LoopTrack::State::RECORDING)
        stopRecordQuantized(0);
    }

    // OVD (hold to add next layer)
    auto ovdEvent = ovdEdge.processEvent(params[OVD_PARAM].getValue() > 0.5f);
    if (ovdEvent == dsp::BooleanTrigger::TRIGGERED)
    {
      if (t0.getLayerCount() < SPREAD_MAX_LAYERS &&
          t0.getState() == LoopTrack::State::PLAYING)
        t0.startOverdub();
    }
    if (ovdEvent == dsp::BooleanTrigger::UNTRIGGERED)
    {
      if (t0.getState() == LoopTrack::State::OVERDUBBING)
        t0.stopOverdub();
    }

    // Deferred quantized stop
    if (pendingStop[0] &&
        t0.getState() == LoopTrack::State::RECORDING &&
        t0.getWritePos() >= targetStop[0])
    {
      t0.setWritePos(targetStop[0]);
      t0.stopRecord();
      pendingStop[0] = false;
    }

    // UND / CLR
    if (undEdge.process(params[UND_PARAM].getValue() > 0.5f))
      t0.undo();
    if (clrEdge.process(params[CLR_PARAM].getValue() > 0.5f))
    {
      t0.clear();
      pendingStop[0] = false;
      selectedTrack = 0;
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
    // Column centers (mm): x = 20 / 40 / 60 / 80
    // Row y (mm):  18=sel-LED  28=status-LED  40=sel-btn  52=act-btn
    const float trackX[4] = {20.f, 40.f, 60.f, 80.f};

    for (int i = 0; i < 4; i++)
    {
      float x = trackX[i];

      // Selected indicator LED
      addChild(createLightCentered<MediumLight<RedLight>>(
          mm2px(Vec(x, 20.f)), module, Repool::SELECTED_LIGHT + i));

      // Status LED (R=rec, G=play, B=ovd)
      addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(
          mm2px(Vec(x, 28.f)), module, Repool::STATE_LIGHT + i * 3));

      // Select button
      addParam(createParamCentered<VCVButton>(
          mm2px(Vec(x, 40.f)), module, Repool::SELECT_PARAM + i));

      // Active / mute toggle (latch button with red LED)
      addParam(createLightParamCentered<VCVLightBezelLatch<MediumLight<RedLight>>>(
          mm2px(Vec(x, 52.f)), module, Repool::ACTIVE_PARAM + i, Repool::ACTIVE_LIGHT + i));
    }

    // ── Action row (y=64): REC / OVD / UND / CLR ─────────────────
    addParam(createParamCentered<VCVButton>(
        mm2px(Vec(trackX[0], 72.f)), module, Repool::REC_PARAM));
    addParam(createParamCentered<VCVButton>(
        mm2px(Vec(trackX[1], 72.f)), module, Repool::OVD_PARAM));
    addParam(createParamCentered<VCVButton>(
        mm2px(Vec(trackX[2], 72.f)), module, Repool::UND_PARAM));
    addParam(createParamCentered<VCVButton>(
        mm2px(Vec(trackX[3], 72.f)), module, Repool::CLR_PARAM));

    // ── Global controls ───────────────────────────────────────────
    addParam(createParamCentered<RoundBlackKnob>(
        mm2px(Vec(33.f, 108.f)), module, Repool::MIX_PARAM));
    addInput(createInputCentered<PJ301MPort>(
        mm2px(Vec(74.f, 80.f)), module, Repool::CLOCK_INPUT));

    // ── Audio I/O + sync outputs ──────────────────────────────────
    addInput(createInputCentered<PJ301MPort>(
        mm2px(Vec(13.f, 88.f)), module, Repool::AUDIO_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(
        mm2px(Vec(13.f, 108.f)), module, Repool::AUDIO_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(
        mm2px(Vec(68.f, 96.f)), module, Repool::POS_OUTPUT));
    addChild(createLightCentered<MediumLight<RedLight>>(
        mm2px(Vec(68.f, 88.f)), module, Repool::POS_LIGHT));
    addOutput(createOutputCentered<PJ301MPort>(
        mm2px(Vec(88.f, 96.f)), module, Repool::LOOP_START_OUTPUT));
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
