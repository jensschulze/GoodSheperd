#include "MillerPuckettePitchShifter.h"

MillerPuckettePitchShifter::MillerPuckettePitchShifter(size_t windowLength,
                                                       float speedChange,
                                                       float sampleRate,
                                                       int numCopies)
    : windowLength_(windowLength),
      speedChange_(speedChange),
      sampleRate_(sampleRate),
      numCopies_(numCopies),
      writePos_(0),
      phase_(0.0f)
{
    updateRotationFreq();
    reset();
}

void MillerPuckettePitchShifter::reset()
{
    // Clear delay buffer
    delayBuffer_.fill(0.0f);
    writePos_ = 0;
    phase_ = 0.0f;
}

void MillerPuckettePitchShifter::reinitializeBuffer()
{
    // Reset the buffer and update rotation frequency
    reset();
    updateRotationFreq();
}

bool MillerPuckettePitchShifter::updateWindowLength(size_t newWindowLength)
{
    // Validate and round window length
    if (newWindowLength < MIN_WINDOW_LENGTH)
        newWindowLength = MIN_WINDOW_LENGTH;
    if (newWindowLength > MAX_WINDOW_LENGTH)
        newWindowLength = MAX_WINDOW_LENGTH;

    // Round to nearest 256
    newWindowLength = (newWindowLength + 128) & ~0xFF;

    // Only update if changed
    if (newWindowLength != windowLength_)
    {
        windowLength_ = newWindowLength;
        reinitializeBuffer();
        return true;
    }
    return false;
}

bool MillerPuckettePitchShifter::updateSpeedChange(float newSpeedChange)
{
    // Validate speed change
    if (newSpeedChange < MIN_SPEED_CHANGE)
        newSpeedChange = MIN_SPEED_CHANGE;
    if (newSpeedChange > MAX_SPEED_CHANGE)
        newSpeedChange = MAX_SPEED_CHANGE;

    // Only update if changed significantly
    if (std::abs(newSpeedChange - speedChange_) > SPEED_CHANGE_THRESHOLD)
    {
        speedChange_ = newSpeedChange;
        updateRotationFreq();
        return true;
    }
    return false;
}

void MillerPuckettePitchShifter::updateRotationFreq()
{
    // Calculate tape head rotation frequency (ω) according to Miller-Puckette:
    // ω = (α - 1) * (fs/L) * β
    // where:
    // α = pitch shift ratio (2.0 for one octave up)
    // fs = sample rate
    // L = window length
    // β = speed change factor
    rotationFreq_ = (PITCH_SHIFT_RATIO - 1.0f) * (sampleRate_ / windowLength_) * speedChange_;
}

float MillerPuckettePitchShifter::hannWindow(float x) const
{
    // Hann window: 0.5 * (1 - cos(2πx))
    // x should be in range [0, 1]
    return 0.5f * (1.0f - std::cos(2.0f * PI * x));
}

float MillerPuckettePitchShifter::getDelayTime(float phase) const
{
    // Calculate delay time based on phase
    // For pitch shifting up by one octave, we use L/2 as the maximum delay
    return (windowLength_ / 2.0f) * (1.0f + std::sin(phase));
}

float MillerPuckettePitchShifter::cubicInterpolate(float x0,
                                                   float x1,
                                                   float x2,
                                                   float x3,
                                                   float t) const
{
    // Cubic interpolation using Catmull-Rom spline
    // t should be in range [0, 1]
    float t2 = t * t;
    float t3 = t2 * t;

    float a0 = -0.5f * x0 + 1.5f * x1 - 1.5f * x2 + 0.5f * x3;
    float a1 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    float a2 = -0.5f * x0 + 0.5f * x2;
    float a3 = x1;

    return a0 * t3 + a1 * t2 + a2 * t + a3;
}

float MillerPuckettePitchShifter::process(float input)
{
    // Store input in delay buffer
    delayBuffer_[writePos_] = input;

    // Calculate output by summing weighted delayed copies
    float output = 0.0f;

    // Calculate phase increment per sample in radians
    const float phaseIncrement = 2.0f * PI * rotationFreq_ / sampleRate_;

    for (int i = 0; i < numCopies_; ++i)
    {
        // Calculate phase for this copy
        float currentPhase = phase_ + (2.0f * PI * i / numCopies_);

        // Calculate delay time for this phase
        float delay = getDelayTime(currentPhase);

        // Calculate read position (with wrap-around)
        float readPos = writePos_ - delay;
        if (readPos < 0)
        {
            readPos += BUFFER_SIZE;
        }

        // Cubic interpolation between samples
        int readPosInt = static_cast<int>(readPos);
        float frac = readPos - readPosInt;

        // Get four samples for cubic interpolation
        float x0 = delayBuffer_[(readPosInt - 1 + BUFFER_SIZE) % BUFFER_SIZE];
        float x1 = delayBuffer_[readPosInt % BUFFER_SIZE];
        float x2 = delayBuffer_[(readPosInt + 1) % BUFFER_SIZE];
        float x3 = delayBuffer_[(readPosInt + 2) % BUFFER_SIZE];

        float interpolated = cubicInterpolate(x0, x1, x2, x3, frac);

        // Apply Hann window
        float windowPos = static_cast<float>(i) / numCopies_;
        float weight = hannWindow(windowPos);

        // Add weighted sample to output
        output += interpolated * weight;
    }

    // Update phase and write position
    phase_ += phaseIncrement;
    if (phase_ >= 2.0f * PI)
    {
        phase_ -= 2.0f * PI;
    }

    writePos_ = (writePos_ + 1) % BUFFER_SIZE;

    return output;
}
