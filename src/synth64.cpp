#include <iostream> 
#include <fstream>
#include <vector>
#include <cassert>
#include <ctime>

#include <rsp-interface.hpp>
#include <libaudio.h>

#include "audio/MeAudio.hpp"
#define FRAME_TIME (CLOCKS_PER_SEC / (60.0f))

#include "synth64.hpp"


/* TODO: Refactor these essentially duplicate functions. */
void loadbin(std::string path, uint8_t* mem, uint16_t offset)
{
   std::ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw std::runtime_error("failed to open file: " + path);
   }
   size_t fileSize = static_cast<size_t>(file.tellg());

   file.seekg(0);
   file.read(reinterpret_cast<char*>(&mem[offset]), fileSize);
   file.close();
}

void loadfile(std::string path, std::vector<uint8_t>& vec) {
   std::ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw std::runtime_error("failed to open file: " + path);
   }
   size_t fileSize = static_cast<size_t>(file.tellg());

   file.seekg(0);
   vec.resize(fileSize);
   file.read(reinterpret_cast<char*>(vec.data()), fileSize);
   file.close();
}

/* Fix seq midi header. */
void swapbytes(u8 count, u8* bytes) {
   for (u8 i = 0; i < count*4; i+=4) {
      u8 temp;

      temp = bytes[i];
      bytes[i] = bytes[i + 3];
      bytes[i + 3] = temp;

      temp = bytes[i + 1];
      bytes[i + 1] = bytes[i + 2];
      bytes[i + 2] = temp;
   }
}

#define AUDIO_HEAP_SIZE    256 * 1024
#define AUDIO_MAX_VOICES   64
#define AUDIO_MAX_UPDATES  128
#define AUDIO_MAX_CHANNELS 32
#define AUDIO_EVT_COUNT    64
#define AUDIO_FREQUENCY    32000

#define AUDIO_BUFSZ        320
#define AUDIO_CLIST_SIZE   32 * 1024

static me::MeAudio* audio; // audio playback backend
static bool initialized = false;

static ALHeap audioHp;
static ALGlobals audioGlobals;
static ALCSPlayer cseqPlayer;
static Acmd audioCmdList[AUDIO_CLIST_SIZE];

static u8* imem;
static u8* dmem;
static u8* rdram;
static s32 seqFileAddress, ctlFileAddress, tblFileAddress, commandListAddress, audioBufferAddress, audioHeapAddress;
static ALCSeq cseq = {};
static ALSeqpConfig cscfg = {};
static ALSynConfig scfg = {};
static ALCSeqMarker start = {};
static ALCSeqMarker end = {};


/* Dummy audio dma callback stubs. */
static s32 audioDmaCallback(s32 addr, s32 len, void* state) {
   return addr;
}
static ALDMAproc audioNewDma(void* arg) {
   return audioDmaCallback;
}

static s32 lastLocTicks = 0;
static u32 overflowAccumulator = 0;
void stop() {
   if (!initialized) { return; }
   alCSPStop(&cseqPlayer);
   alCSeqSetLoc(&cseq, &start);
   lastLocTicks = 0;
   overflowAccumulator = 0;
}

void play() {
   if (!initialized) { return; }
   alCSPPlay(&cseqPlayer);
}

void pause() {
   if (!initialized) { return; }
   alCSPStop(&cseqPlayer);
}

void scrub(float position) {
   ALCSeqMarker scrubMarker;
   s32 newTick = end.lastTicks * position;
   alCSeqNewMarker(&cseq, &scrubMarker, newTick);
   alCSeqSetLoc(&cseq, &scrubMarker);
   lastLocTicks = 0;
   overflowAccumulator = 0;
}

void getloc(float& position) {
   ALCSeqMarker currMarker;
   alCSeqGetLoc(&cseq, &currMarker);

   /* We overflowed, lets do some math. */
   u32 currTicks, lastTicks;
   memcpy(&currTicks, &currMarker.lastTicks, 4);
   memcpy(&lastTicks, &lastLocTicks, 4);
   if (lastTicks > currTicks) {
      u32 overflowStep = (u64) 0x100000000 % (u32) end.lastTicks;
      overflowAccumulator = (overflowAccumulator + overflowStep) % end.lastTicks;
   }

   position = (float) ((currMarker.lastTicks + overflowAccumulator) % end.lastTicks) / (float) end.lastTicks;
   lastLocTicks = currMarker.lastTicks;
}

void startaudiothread() {
   load();
   power(false);
   audio = new me::MeAudio;
}

void init(std::string seqPath, std::string bankPath, std::string wavetablePath, int bank) {
   if (initialized) { 
      stop();
      alClose(&audioGlobals);
   }
   initialized = false;
   lastLocTicks = 0;
   overflowAccumulator = 0;

   imem = static_cast<u8*>(getimem());
   dmem = static_cast<u8*>(getdmem());
   rdram = static_cast<u8*>(getrdram());

   memset(imem,  0, 4  * 1024);        // clear imem
   memset(dmem,  0, 4  * 1024);        // clear dmem
   memset(rdram, 0, 16 * 1024 * 1024); // clear rdram

   std::vector<u8> seqFile;
   std::vector<u8> ctlFile;
   std::vector<u8> tblFile;

   loadbin("ucode/audio_data.zbin", dmem, 0); // rsp udata
   loadbin("ucode/audio.zbin", imem, 0x80); // rsp ucode
   loadfile(seqPath, seqFile); // seq (midi) file
   loadfile(bankPath, ctlFile); // soundbank file
   loadfile(wavetablePath, tblFile); // wavetable file
   swapbytes(17, seqFile.data()); // swap header for endianess compatibility

   /* Reserve and initialize these memory regions in our virtual RDRAM. */
   seqFileAddress = 0;
   ctlFileAddress = (seqFile.size() + 0xF) & (~0xF);
   tblFileAddress = ((ctlFileAddress + ctlFile.size()) + 0xF) & (~0xF);
   commandListAddress = (tblFileAddress + tblFile.size() + 0xF) & (~0xF);
   audioBufferAddress = ((commandListAddress + AUDIO_CLIST_SIZE * sizeof(Acmd)) + 0xF) & (~0xF);
   audioHeapAddress = ((audioBufferAddress + 4 * AUDIO_BUFSZ) + 0xF) & (~0xF);
   assert((AUDIO_HEAP_SIZE + audioHeapAddress) < 0x1000000); // do not exceed 16MB ram size for now.
   memcpy(rdram + seqFileAddress, seqFile.data(), seqFile.size());
   memcpy(rdram + ctlFileAddress, ctlFile.data(), ctlFile.size());
   memcpy(rdram + tblFileAddress, tblFile.data(), tblFile.size());

   /* Initialize Audio Heap */
   audioHp = {};
   audioHp.base = rdram + audioHeapAddress;
   audioHp.cur = rdram + audioHeapAddress;
   audioHp.count = 0;
   audioHp.len = AUDIO_HEAP_SIZE;
   s32 audioRate = 0x02E6D354 / (s32)(0x02E6D354 / (f32)AUDIO_FREQUENCY - .5f);
   scfg = {
       .maxVVoices = AUDIO_MAX_VOICES,
       .maxPVoices = AUDIO_MAX_VOICES,
       .maxUpdates = AUDIO_MAX_UPDATES,
       .dmaproc = (void*)audioNewDma, // explained later
       .heap = &audioHp,
       .outputRate = audioRate,
       .fxType = AL_FX_BIGROOM,
   };
   audioGlobals = {};
   alInit(&audioGlobals, &scfg);
   audioGlobals.rdram = rdram;

   /* Initialize CSeqPlayer and CSequence. */
   cscfg = {
      .maxVoices = AUDIO_MAX_VOICES,
      .maxEvents = AUDIO_EVT_COUNT,
      .maxChannels = AUDIO_MAX_CHANNELS,
      .debugFlags = 0, // disabled
      .heap = &audioHp,
      .initOsc = nullptr,
      .updateOsc = nullptr,
      .stopOsc = nullptr,
   };
   cseqPlayer = {};
   alCSPNew(&cseqPlayer, &cscfg);

   alCSeqNew(&cseq, rdram + seqFileAddress);
   alCSPSetSeq(&cseqPlayer, &cseq);

   ALBankFile* bankFile = (ALBankFile*)getRamObject(ctlFileAddress);
   alBnkfNew(ctlFileAddress, tblFileAddress);
   alCSPSetBank(&cseqPlayer, bankFile->getBank(bank));
   alCSeqGetLoc(&cseq, &start);
   alCSeqGetFinalMarker(&cseq, &end);
   initialized = true;
}

int mainloop(float volume) {
   /* Skip loop if uninitialized synth */
   if (!initialized) { return 0; }

   /* Generate required audio */
   while (audio->NeedsAudio()) {
      /* Generate and write to DRAM the audio command list */
      s32 cmdLen = 0;
      alAudioFrame(audioCmdList, &cmdLen, (s16*)audioBufferAddress, AUDIO_BUFSZ);
      memcpy(rdram + commandListAddress, audioCmdList, cmdLen * sizeof(Acmd));

      /* Write to DMEM address 0xFF0 the 32 bit address of rdram location of cmdList */
      s32 data = 0;
      data = commandListAddress;
      memcpy(dmem + 0xFF0, &data, 4);

      /* Write to 0xFF4 length of rdram cmdList (cmdLen * sizeof(Acmd)) */
      data = cmdLen * sizeof(Acmd);
      memcpy(dmem + 0xFF4, &data, 4);

      /* Loop through rsp ucode until halt is reached. */
      while (!halted()) { run(); }
      unhalt();

      /* Push generated samples to stream */
      u8* output = (rdram + audioBufferAddress);
      if (volume != 1.0f) {
         for (u32 i = 0; i < AUDIO_BUFSZ * DEVICE_CHANNELS; i++) {
            ((s16*)output)[i] = (s16) ((s16*)output)[i] * volume;
         }
      }
      memcpy(audio->GetAudioBuffer(), output, AUDIO_BUFSZ * sizeof(s16) * DEVICE_CHANNELS);
      audio->PushToStream();

      /* TODO: Missing unloading rsp. */
      // unload();
   }
   return 0;
}

