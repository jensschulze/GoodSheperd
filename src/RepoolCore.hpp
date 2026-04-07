#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class LoopTrack {
public:
    enum class State { EMPTY, RECORDING, PLAYING, OVERDUBBING };

    static constexpr int   MAX_LAYERS    = 4;
    static constexpr int   CROSSFADE_LEN = 256;
    static constexpr float MAX_DURATION  = 60.0f;
    static constexpr float DC_R          = 0.995f;

    explicit LoopTrack(float sampleRate) : sampleRate(sampleRate) {
        allocate();
    }

    void setSampleRate(float sr) {
        sampleRate = sr;
        clear();
        allocate();
    }

    // ── State transitions ────────────────────────────────────────────────

    void startRecord() {
        if (state != State::EMPTY) return;
        writePos = 0;
        std::fill(baseBuffer.begin(), baseBuffer.end(), 0.0f);
        state = State::RECORDING;
    }

    void stopRecord() {
        if (state != State::RECORDING) return;
        // Require a minimum length to produce a usable loop
        if (writePos < 2 * CROSSFADE_LEN) {
            state = State::EMPTY;
            return;
        }
        loopLength = writePos;
        applyBoundaryFades(baseBuffer, loopLength);
        playhead = 0;
        state = State::PLAYING;
    }

    void startOverdub() {
        if (state != State::PLAYING) return;
        if (isFull()) return;
        currentLayer.assign(loopLength, 0.0f);
        state = State::OVERDUBBING;
    }

    // Commit current overdub layer and resume playback.
    void stopOverdub() {
        if (state != State::OVERDUBBING) return;
        applyBoundaryFades(currentLayer, loopLength);
        layerStack.push_back(std::move(currentLayer));
        currentLayer.clear();
        state = State::PLAYING;
    }

    // Reset playhead to 0. If overdubbing, commits the current layer first.
    // No-op during RECORDING or when EMPTY.
    void retrigger() {
        if (state == State::OVERDUBBING) stopOverdub();
        if (state == State::PLAYING) playhead = 0;
    }

    // Remove the last committed layer, or discard an in-progress overdub.
    void undo() {
        if (state == State::OVERDUBBING) {
            currentLayer.clear();
            state = State::PLAYING;
        } else if (state == State::PLAYING && !layerStack.empty()) {
            layerStack.pop_back();
        }
    }

    void clear() {
        state      = State::EMPTY;
        loopLength = 0;
        playhead   = 0;
        writePos   = 0;
        layerStack.clear();
        currentLayer.clear();
        dcBlockX = 0.0f;
        dcBlockY = 0.0f;
    }

    // ── DSP ──────────────────────────────────────────────────────────────

    // Returns loop audio only (dry signal handled by the host module).
    // During RECORDING/EMPTY, returns 0 so the mix knob behaves predictably.
    float process(float input) {
        switch (state) {
            case State::EMPTY:
                return 0.0f;

            case State::RECORDING: {
                int maxSamples = (int)baseBuffer.size();
                if (writePos < maxSamples) {
                    baseBuffer[writePos++] = input;
                } else {
                    // Buffer full — auto-commit
                    stopRecord();
                    if (state == State::PLAYING)
                        return dcBlock(sumBuffers(playhead));
                }
                return 0.0f;
            }

            case State::PLAYING:
            case State::OVERDUBBING: {
                float out = sumBuffers(playhead);
                if (state == State::OVERDUBBING) {
                    // Accumulate input into the current layer (additive overdub)
                    currentLayer[playhead] += input;
                }
                playhead = (playhead + 1) % loopLength;
                return dcBlock(out);
            }
        }
        return 0.0f;
    }

    // ── Accessors ────────────────────────────────────────────────────────

    State getState()      const { return state; }
    int   getLayerCount() const { return (int)layerStack.size(); }
    bool  isFull()        const { return (int)layerStack.size() >= MAX_LAYERS; }
    bool  hasContent()    const { return state != State::EMPTY; }
    int   getLoopLength() const { return loopLength; }

    // Normalized playhead position in [0, 1)
    float getPosition() const {
        return (loopLength > 0) ? (float)playhead / (float)loopLength : 0.0f;
    }

    // Current write position during RECORDING (samples written so far)
    int getWritePos() const { return writePos; }

    // Current playhead position (samples, 0-based)
    int getPlayhead() const { return playhead; }

    // Truncate the record buffer to `pos` samples (only valid during RECORDING).
    // Used by clock-quantization to snap a loop end to a beat boundary.
    void setWritePos(int pos) {
        if (state != State::RECORDING) return;
        int maxSamples = static_cast<int>(baseBuffer.size());
        writePos = std::max(0, std::min(pos, maxSamples));
    }

private:
    float sampleRate;
    State state = State::EMPTY;

    std::vector<float>              baseBuffer;
    std::vector<std::vector<float>> layerStack;
    std::vector<float>              currentLayer;

    int loopLength = 0;
    int playhead   = 0;
    int writePos   = 0;

    float dcBlockX = 0.0f;
    float dcBlockY = 0.0f;

    void allocate() {
        int maxSamples = static_cast<int>(MAX_DURATION * sampleRate);
        baseBuffer.assign(maxSamples, 0.0f);
    }

    // Sum base + all committed layers + in-progress layer (if any)
    float sumBuffers(int pos) const {
        float out = baseBuffer[pos];
        for (const auto& layer : layerStack)
            out += layer[pos];
        if (!currentLayer.empty())
            out += currentLayer[pos];
        return out;
    }

    // Cosine fade-in over the first N samples and fade-out over the last N samples.
    // Prevents clicks at the loop splice point.
    void applyBoundaryFades(std::vector<float>& buf, int len) {
        static constexpr float PI = 3.14159265358979323846f;
        int fadeLen = std::min(CROSSFADE_LEN, len / 4);
        if (fadeLen < 2) return;
        for (int i = 0; i < fadeLen; i++) {
            float env = 0.5f * (1.0f - cosf(PI * (float)i / (float)fadeLen));
            buf[i]           *= env;   // fade in at loop start
            buf[len - 1 - i] *= env;   // fade out at loop end
        }
    }

    // Simple one-pole DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
    float dcBlock(float x) {
        float y  = x - dcBlockX + DC_R * dcBlockY;
        dcBlockX = x;
        dcBlockY = y;
        return y;
    }
};
