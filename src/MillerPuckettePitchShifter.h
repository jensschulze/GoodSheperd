#pragma once

#include <array>
#include <cmath>

class MillerPuckettePitchShifter
{
  public:
    // Parameter ranges
    static constexpr size_t MIN_WINDOW_LENGTH
        = 512; // Minimum window length (samples)
    static constexpr size_t MAX_WINDOW_LENGTH
        = 2048; // Maximum window length (samples)
    static constexpr float MIN_SPEED_CHANGE
        = 0.25f; // Minimum speed (quarter speed)
    static constexpr float MAX_SPEED_CHANGE = 4.0f; // Maximum speed (4x speed)

    // Constructor takes window length, speed change, sample rate, and number of copies as parameters
    // windowLength should be a power of 2 for efficient processing
    // speedChange determines how fast the phase changes (1.0 = normal speed)
    // sampleRate is the system sample rate in Hz (e.g., 48000 for Daisy)
    // numCopies determines the number of overlapping copies (more copies = better quality but more CPU)
    MillerPuckettePitchShifter(size_t windowLength,
                               float  speedChange,
                               float  sampleRate,
                               int    numCopies = 4);

    // Process a single sample
    float process(float input);

    // Reset the internal state
    void reset();

    // Parameter update methods
    bool updateWindowLength(size_t newWindowLength);
    bool updateSpeedChange(float newSpeedChange);

    // Get current parameter values
    size_t getWindowLength() const { return windowLength_; }
    float  getSpeedChange() const { return speedChange_; }
    float  getSampleRate() const { return sampleRate_; }
    int    getNumCopies() const { return numCopies_; }

  private:
    // Constants
    static constexpr float PITCH_SHIFT_RATIO
        = 2.0f; // One octave up (12 semitones)
    static constexpr float  PI          = 3.14159265358979323846f;
    static constexpr size_t BUFFER_SIZE = 4096; // Size of the delay buffer
    static constexpr float  SPEED_CHANGE_THRESHOLD
        = 0.01f; // Threshold for speed change updates

    // Parameters (can be updated during runtime)
    size_t      windowLength_;
    float       speedChange_; // Speed change factor (β)
    const float sampleRate_;
    const int   numCopies_;

    // Calculated parameters
    float rotationFreq_; // Calculated tape head rotation frequency (ω)

    // Internal state
    std::array<float, BUFFER_SIZE>
           delayBuffer_; // Circular buffer for delayed samples
    size_t writePos_;    // Current write position in buffer
    float  phase_;       // Current phase of rotation

    // Helper methods
    float hannWindow(float x) const; // Hann window function
    float
    getDelayTime(float phase) const; // Calculate delay time for given phase
    float cubicInterpolate(float x0,
                           float x1,
                           float x2,
                           float x3,
                           float t) const; // Cubic interpolation
    void  updateRotationFreq(); // Update rotation frequency based on parameters
    void reinitializeBuffer(); // Reinitialize the buffer when parameters change
};
