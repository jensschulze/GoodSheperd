#pragma once
#include <vector>
#include <algorithm>
#include <cstring>

class ResreverCore
{
public:
  // 0 = fully reversed (wet), 1 = fully dry — matches Daisy formula
  float mix = 0.0f;

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
    // Detect rising/falling edges; lock transitions during crossfades and copy
    if (!isCrossfadingIn && !isCrossfadingOut && !isCopying)
    {
      if (reverse && !prevReverse)
      {
        isCrossfadingIn = true;
        crossfadePos    = 0;
      }
      else if (!reverse && prevReverse)
      {
        isCrossfadingOut = true;
        crossfadePos     = 0;
      }
    }
    prevReverse = reverse;

    // Crossfade into reverse mode
    if (isCrossfadingIn)
    {
      float fadeOut = 1.0f - (float)crossfadePos / (float)CROSSFADE_LENGTH;
      float fadeIn  = (float)crossfadePos / (float)CROSSFADE_LENGTH;

      writeBuffer[writePos] = input;
      readBuffer[writePos]  = input;

      readPos        = (bufferSize + readPos - 1) % bufferSize;
      float output   = input * fadeOut
                     + (input * mix + readBuffer[readPos] * (1.0f - mix)) * fadeIn;

      writePos = (writePos + 1) % bufferSize;
      crossfadePos++;

      if (crossfadePos >= CROSSFADE_LENGTH)
      {
        isCrossfadingIn = false;
        reverseActive   = true;
        crossfadePos    = 0;
      }

      return dcBlock(output);
    }

    // Crossfade out of reverse mode
    if (isCrossfadingOut)
    {
      float fadeOut = 1.0f - (float)crossfadePos / (float)CROSSFADE_LENGTH;
      float fadeIn  = (float)crossfadePos / (float)CROSSFADE_LENGTH;

      writeBuffer[writePos] = input;
      readBuffer[writePos]  = input;

      readPos      = (bufferSize + readPos - 1) % bufferSize;
      float output = (input * mix + readBuffer[readPos] * (1.0f - mix)) * fadeOut
                   + input * fadeIn;

      writePos = (writePos + 1) % bufferSize;
      crossfadePos++;

      if (crossfadePos >= CROSSFADE_LENGTH)
      {
        isCrossfadingOut = false;
        reverseActive    = false;
        crossfadePos     = 0;
        isCopying        = true;
        copyPos          = 0;
      }

      return dcBlock(output);
    }

    // Incrementally resync readBuffer with writeBuffer after exiting reverse
    if (isCopying)
    {
      int samplesToCopy = std::min(COPY_CHUNK_SIZE, bufferSize - copyPos);
      std::memcpy(&readBuffer[copyPos], &writeBuffer[copyPos],
                  samplesToCopy * sizeof(float));
      copyPos += samplesToCopy;

      if (copyPos >= bufferSize)
      {
        isCopying = false;
        copyPos   = 0;
      }
    }

    if (!reverseActive)
    {
      readPos               = writePos;
      writeBuffer[writePos] = input;
      readBuffer[writePos]  = input;
      writePos = (writePos + 1) % bufferSize;
      return dcBlock(input);
    }
    else
    {
      writeBuffer[writePos] = input;
      writePos = (writePos + 1) % bufferSize;

      float outputR = readBuffer[readPos];
      readPos       = (bufferSize + readPos - 1) % bufferSize;

      return dcBlock(input * mix + outputR * (1.0f - mix));
    }
  }

  bool isReversing()   const { return reverseActive; }
  bool isCrossfading() const { return isCrossfadingIn || isCrossfadingOut; }
  bool isSyncing()     const { return isCopying; }

  void reset()
  {
    writePos         = 0;
    readPos          = 0;
    isCopying        = false;
    copyPos          = 0;
    reverseActive    = false;
    isCrossfadingIn  = false;
    isCrossfadingOut = false;
    crossfadePos     = 0;
    prevReverse      = false;
    dcBlockX         = 0.0f;
    dcBlockY         = 0.0f;
    std::fill(readBuffer.begin(), readBuffer.end(), 0.0f);
    std::fill(writeBuffer.begin(), writeBuffer.end(), 0.0f);
  }

private:
  static constexpr int   COPY_CHUNK_SIZE  = 256;
  static constexpr int   CROSSFADE_LENGTH = 256;
  static constexpr float DC_R             = 0.995f;

  std::vector<float> readBuffer;
  std::vector<float> writeBuffer;
  int   writePos       = 0;
  int   readPos        = 0;
  int   bufferSize     = 0;
  float bufferDuration;
  float sampleRate;

  bool  isCopying        = false;
  int   copyPos          = 0;
  bool  reverseActive    = false;
  bool  isCrossfadingIn  = false;
  bool  isCrossfadingOut = false;
  int   crossfadePos     = 0;
  bool  prevReverse      = false;

  // Simple one-pole DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]
  float dcBlockX = 0.0f;
  float dcBlockY = 0.0f;

  float dcBlock(float x)
  {
    float y  = x - dcBlockX + DC_R * dcBlockY;
    dcBlockX = x;
    dcBlockY = y;
    return y;
  }

  void updateBufferSize()
  {
    bufferSize = (int)(bufferDuration * sampleRate);
    readBuffer.assign(bufferSize, 0.0f);
    writeBuffer.assign(bufferSize, 0.0f);
    reset();
  }
};
