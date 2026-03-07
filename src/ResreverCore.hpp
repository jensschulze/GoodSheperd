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
    if (!reverse)
    {
      // Bypass mode: output = input
      // Write input to ring buffer
      buffer[writePos] = input;
      writePos = (writePos + 1) % bufferSize;

      // Set read position to most recent sample (one behind write position)
      // so when reverse is engaged, we start from the most recent audio
      readPos = (writePos - 1 + bufferSize) % bufferSize;

      return input;
    }
    else
    {
      // Reverse mode: read buffer backwards
      float output = buffer[readPos];

      // Move read position backwards through the buffer
      readPos = (readPos - 1 + bufferSize) % bufferSize;

      return output;
    }
  }

  void reset()
  {
    writePos = 0;
    readPos = 0;
    std::fill(buffer.begin(), buffer.end(), 0.0f);
  }

private:
  std::vector<float> buffer;
  int writePos = 0;
  int readPos = 0;
  int bufferSize = 0;
  float bufferDuration;
  float sampleRate;

  void updateBufferSize()
  {
    bufferSize = (int)(bufferDuration * sampleRate);
    buffer.resize(bufferSize);
    writePos = 0;
    readPos = 0;
  }
};
