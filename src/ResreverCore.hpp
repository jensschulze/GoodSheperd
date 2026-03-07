#pragma once
#include <vector>
#include <algorithm>

class ResreverCore
{
public:
  ResreverCore(float bufferDuration, float sampleRate)
      : bufferDuration(bufferDuration), sampleRate(sampleRate)
  {
    updateBufferSize();
  }

  void setSampleRate(float newSampleRate)
  {
    sampleRate = newSampleRate;
    updateBufferSize();
  }

  float process(float input, bool reverse)
  {
    // Perform incremental buffer copying if in progress
    if (isCopying)
    {
      // Copy a chunk of samples per audio callback
      int samplesToCopy = std::min(COPY_CHUNK_SIZE, bufferSize - copyPos);
      for (int i = 0; i < samplesToCopy; i++)
      {
        readBuffer[copyPos + i] = writeBuffer[copyPos + i];
      }
      copyPos += samplesToCopy;

      // Check if copying is complete
      if (copyPos >= bufferSize)
      {
        isCopying = false;
        copyPos = 0;
      }

      // During copying, force bypass mode behavior (ignore reverse)
      reverse = false;
    }

    if (!reverse)
    {
      // If transitioning from reverse to bypass mode, start incremental copy
      if (wasReversing)
      {
        isCopying = true;
        copyPos = 0;
        wasReversing = false;
      }

      // Bypass mode: output = input
      // Write input to both ring buffers
      readBuffer[writePos] = input;
      writeBuffer[writePos] = input;
      writePos = (writePos + 1) % bufferSize;

      // Set read position to most recent sample (one behind write position)
      // so when reverse is engaged, we start from the most recent audio
      readPos = (writePos - 1 + bufferSize) % bufferSize;

      return input;
    }
    else
    {
      // Mark that we're in reverse mode
      wasReversing = true;

      // Reverse mode: read from read buffer backwards
      float output = readBuffer[readPos];

      // Move read position backwards through the buffer
      readPos = (readPos - 1 + bufferSize) % bufferSize;

      // Continue writing input to write buffer
      writeBuffer[writePos] = input;
      writePos = (writePos + 1) % bufferSize;

      return output;
    }
  }

  void reset()
  {
    writePos = 0;
    readPos = 0;
    wasReversing = false;
    isCopying = false;
    copyPos = 0;
    std::fill(readBuffer.begin(), readBuffer.end(), 0.0f);
    std::fill(writeBuffer.begin(), writeBuffer.end(), 0.0f);
  }

private:
  // Number of samples to copy per process() call (spread copying over multiple callbacks)
  static constexpr int COPY_CHUNK_SIZE = 256;

  std::vector<float> readBuffer;  // Buffer used for reverse playback
  std::vector<float> writeBuffer; // Buffer continuously written to
  int writePos = 0;
  int readPos = 0;
  int bufferSize = 0;
  float bufferDuration;
  float sampleRate;
  bool wasReversing = false; // Track if we were in reverse mode
  bool isCopying = false;    // Track if incremental copy is in progress
  int copyPos = 0;           // Current position in incremental copy

  void updateBufferSize()
  {
    bufferSize = (int)(bufferDuration * sampleRate);
    readBuffer.resize(bufferSize);
    writeBuffer.resize(bufferSize);
    writePos = 0;
    readPos = 0;
    wasReversing = false;
    isCopying = false;
    copyPos = 0;
  }
};
