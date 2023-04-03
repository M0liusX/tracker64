#pragma once

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define DEVICE_FORMAT       ma_format_s16
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  32000
#define BUFFER_SIZE 2048
#define STREAM_SIZE 32000
#define DELAY 320
#include <iostream>
#include <array>
#include <atomic>

namespace me {

/* Circular Buffer Audio Implementation. */
struct AudioBuffer {
   short samples[BUFFER_SIZE] = { 0 };
   std::array<short, STREAM_SIZE> stream = { 0 };
   std::atomic<uint32_t> count = 0; // updated by application in PushToStream
   std::atomic<uint32_t> tail = 0; // updated by application in PushToStream
   uint32_t head = 0;
};

class MeAudio {
public:
   MeAudio() {
      ma_device_config deviceConfig;

      deviceConfig = ma_device_config_init(ma_device_type_playback);
      deviceConfig.playback.format = DEVICE_FORMAT;
      deviceConfig.playback.channels = DEVICE_CHANNELS;
      deviceConfig.sampleRate = DEVICE_SAMPLE_RATE;
      deviceConfig.dataCallback = AudioCallback;
      deviceConfig.pUserData = &buffer;

      if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
         assert(false && "Failed to open playback device.\n");
      }
      printf("Device Name: %s\n", device.playback.name);

      if (ma_device_start(&device) != MA_SUCCESS) {
         ma_device_uninit(&device);
         assert(false && "Failed to start playback device.\n");
      }
   }
   ~MeAudio()
   {
      ma_device_uninit(&device);
   }
   short* GetAudioBuffer()
   {
      return buffer.samples;
   }
   int GetAudioBufferSpace()
   {
      return BUFFER_SIZE;
   }
   int GetAudioBufferCount()
   {
      return buffer.count >> 1;
   }
   void SetDelayBuffer() {
      if (buffer.count < (DELAY)) {
         buffer.tail = (buffer.tail + DELAY) % STREAM_SIZE;
         buffer.count += DELAY;
      }
   }
   bool PushToStream(int newSampleCount) {
      if (newSampleCount + buffer.count > STREAM_SIZE) {
         return false;
      }
      for (uint32_t i = 0; i < newSampleCount; i++) {
         buffer.stream[(buffer.tail + i) % STREAM_SIZE] = buffer.samples[i];
      }
      buffer.tail = (buffer.tail + newSampleCount) % STREAM_SIZE;
      buffer.count += newSampleCount;
      return true;
   }
private:
   static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
   {
      AudioBuffer* pBuffer;

      MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);

      pBuffer = (AudioBuffer*)pDevice->pUserData;
      MA_ASSERT(pBuffer != nullptr);

      /* Process Audio Buffer! */
      ma_uint64 iFrame;
      ma_uint64 iChannel;
      MA_ASSERT(pOutput != NULL);
      ma_int16* pFramesOutS16 = (ma_int16*)pOutput;

      /* Play buffer! */
      ma_uint64 unreadSamples = (pBuffer->count) >> 1;
      ma_uint64 totalFrames = unreadSamples < frameCount ? unreadSamples : frameCount;
      //std::cout << "Callback: " << (totalFrames << 1) << std::endl;
      for (iFrame = 0; iFrame < totalFrames; iFrame += 1) {
         for (iChannel = 0; iChannel < DEVICE_CHANNELS; iChannel += 1) {
            ma_int16 s = pBuffer->stream[pBuffer->head % STREAM_SIZE];
            pFramesOutS16[iFrame * DEVICE_CHANNELS + iChannel] = s;
            pBuffer->head++;
         }
      }
      pBuffer->head %= STREAM_SIZE;
      pBuffer->count -= (totalFrames << 1);
      (void)pInput;   /* Unused. */
   }

   ma_device device;
public:
   AudioBuffer buffer = {};
};

} // namespace me