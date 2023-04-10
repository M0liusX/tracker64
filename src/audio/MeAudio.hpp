#pragma once

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define DEVICE_FORMAT       ma_format_s16
#define DEVICE_CHANNELS     2
#define DEVICE_SAMPLE_RATE  32000
#define BUFFER_SIZE 320
#define BUFFER_COUNT 2
#include <iostream>
#include <array>
#include <atomic>

namespace me {

   /* Multiple Buffer Audio Implementation. */
   struct AudioBuffer {
      std::array<short, BUFFER_SIZE * DEVICE_CHANNELS> stream = { 0 };
      std::atomic<bool> ready = 0;
      //short samples[BUFFER_SIZE] = { 0 };
      //std::array<short, STREAM_SIZE> stream = { 0 };
      //std::atomic<uint32_t> count = 0; // updated by application in PushToStream
      //std::atomic<uint32_t> tail = 0; // updated by application in PushToStream
      //uint32_t head = 0;
   };

   struct AudioSwapChain {
      std::array<AudioBuffer, BUFFER_COUNT> buffers = {};
      int callbackBufferId = 0;
      int audioThreadyBufferId = 0;
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
         deviceConfig.pUserData = &swapChain;

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

      void AudioWait() {
         while (swapChain.buffers[swapChain.audioThreadyBufferId].ready) {}
      }

      short* GetAudioBuffer()
      {
         return swapChain.buffers[swapChain.audioThreadyBufferId].stream.data();
      }

      void PushToStream() {
         swapChain.buffers[swapChain.audioThreadyBufferId].ready = true;
         swapChain.audioThreadyBufferId = ++swapChain.audioThreadyBufferId % BUFFER_COUNT;

      }

   private:
      static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
      {
         AudioBuffer* pBuffer;
         AudioSwapChain* pSwapChain;
         MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);
         MA_ASSERT(frameCount == BUFFER_SIZE);

         pSwapChain = (AudioSwapChain*)pDevice->pUserData;
         pBuffer = &pSwapChain->buffers[pSwapChain->callbackBufferId];
         MA_ASSERT(pBuffer != nullptr);

         /* Process Audio Buffer! */
         ma_uint64 iFrame;
         ma_uint64 iChannel;
         MA_ASSERT(pOutput != NULL);
         ma_int16* pFramesOutS16 = (ma_int16*)pOutput;

         /* Play buffer! */
         // ma_uint64 unreadSamples = (pBuffer->count) >> 1;
         // ma_uint64 totalFrames = unreadSamples < frameCount ? unreadSamples : frameCount;
         ma_uint64 totalFrames = frameCount;

         //std::cout << "Callback: " << (totalFrames << 1) << std::endl;
         /* Too Slow, Missing Data! */
         if (!pBuffer->ready) {
            return;
         }

         for (iFrame = 0; iFrame < totalFrames; iFrame += 1) {
            for (iChannel = 0; iChannel < DEVICE_CHANNELS; iChannel += 1) {
               ma_int16 s = pBuffer->stream[iFrame * DEVICE_CHANNELS + iChannel];
               pFramesOutS16[iFrame * DEVICE_CHANNELS + iChannel] = s;
            }
         }
         pSwapChain->callbackBufferId = ++pSwapChain->callbackBufferId % BUFFER_COUNT;
         pBuffer->ready = false;
         (void)pInput;   /* Unused. */
      }

      ma_device device;
   public:
      AudioSwapChain swapChain;
   };

} // namespace me