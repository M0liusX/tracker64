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
#define AUDIO_MAX_VOICES   32
#define AUDIO_MAX_UPDATES  64
#define AUDIO_MAX_CHANNELS 16
#define AUDIO_EVT_COUNT    32
#define AUDIO_FREQUENCY    32000

#define AUDIO_BUFSZ        320
#define AUDIO_CLIST_SIZE   32 * 1024

static ALHeap audioHp;
static ALGlobals audioGlobals;
static ALCSPlayer cseqPlayer;
static u8* rdram;
static Acmd audioCmdList[AUDIO_CLIST_SIZE];

static s32 audioDmaCallback(s32 addr, s32 len, void* state) {
   return addr;
}

static ALDMAproc audioNewDma(void* arg) {
   return audioDmaCallback;
}

static me::MeAudio* audio;
static u8* imem;
static u8* dmem;
static s32 seqFileAddress, ctlFileAddress, tblFileAddress, commandListAddress, audioBufferAddress, audioHeapAddress;
static ALCSeq cseq = {};
static ALSeqpConfig cscfg;
static ALSynConfig scfg;
static ALCSeqMarker start = {};

void stop() {
   alCSPStop(&cseqPlayer);
   alCSeqSetLoc(&cseq, &start);
}

void play() {
   alCSPPlay(&cseqPlayer);
}

void pause() {
   alCSPStop(&cseqPlayer);
}

void init() {
   audio = new me::MeAudio;
   /*audio->SetDelayBuffer();*/

   load();
   power(false);

   imem = static_cast<u8*>(getimem());
   dmem = static_cast<u8*>(getdmem());
   rdram = static_cast<u8*>(getrdram());

   std::vector<u8> seqFile;
   std::vector<u8> ctlFile;
   std::vector<u8> tblFile;

   // Pause
   //int m;
   //std::cin >> m;
   //std::cout << m;

   loadbin("ucode/audio_data.zbin", dmem, 0); // rsp udata
   loadbin("ucode/audio.zbin", imem, 0x80); // rsp ucode
   loadfile("samples/seq/intopipe.bin", seqFile); // mp1 mario board seq
   loadfile("samples/ctl/mp2.ctl", ctlFile); // mp1 soundbank file
   loadfile("samples/tbl/mp2.ztbl", tblFile); // mp1 wavetable file
   swapbytes(17, seqFile.data());

   seqFileAddress = 0;
   ctlFileAddress = (seqFile.size() + 0xF) & (~0xF);
   tblFileAddress = ((ctlFileAddress + ctlFile.size()) + 0xF) & (~0xF);
   commandListAddress = (tblFileAddress + tblFile.size() + 0xF) & (~0xF);
   audioBufferAddress = ((commandListAddress + AUDIO_CLIST_SIZE * sizeof(Acmd)) + 0xF) & (~0xF);
   audioHeapAddress = ((audioBufferAddress + 4 * AUDIO_BUFSZ) + 0xF) & (~0xF);
   assert((AUDIO_HEAP_SIZE + audioHeapAddress) < 0x1000000); // do not exceed 8MB ram size for now.
   memcpy(rdram + seqFileAddress, seqFile.data(), seqFile.size());
   memcpy(rdram + ctlFileAddress, ctlFile.data(), ctlFile.size());
   memcpy(rdram + tblFileAddress, tblFile.data(), tblFile.size());

   audioHp.base = rdram + audioHeapAddress;
   audioHp.cur = rdram + audioHeapAddress;
   audioHp.count = 0;
   audioHp.len = AUDIO_HEAP_SIZE;

   s32 audioRate = 0x02E6D354 / (s32)(0x02E6D354 / (f32)32000.0 - .5f);
   scfg = {
       .maxVVoices = AUDIO_MAX_VOICES,
       .maxPVoices = AUDIO_MAX_VOICES,
       .maxUpdates = AUDIO_MAX_UPDATES,
       .dmaproc = (void*)audioNewDma, // explained later
       .heap = &audioHp,
       .outputRate = audioRate,
       .fxType = AL_FX_SMALLROOM,
   };
   alInit(&audioGlobals, &scfg);
   audioGlobals.rdram = rdram;

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
   alCSPNew(&cseqPlayer, &cscfg);

   alCSeqNew(&cseq, rdram + seqFileAddress);
   alCSPSetSeq(&cseqPlayer, &cseq);

   ALBankFile* bankFile = (ALBankFile*)getRamObject(ctlFileAddress);
   alBnkfNew(ctlFileAddress, tblFileAddress);
   alCSPSetBank(&cseqPlayer, bankFile->getBank(0x1B));
   alCSeqGetLoc(&cseq, &start);
}

int mainloop() {
   // std::ofstream outfile("rainbow.sw", std::ofstream::binary);
   // while (true) {
      /* Generate required audio */
      if (!audio->NeedsAudio()) {
         return 0;
      }
      // audio->AudioWait();

      clock_t startTime = clock(); // Start timer
      clock_t timePassed = startTime;

      s32 cmdLen = 0;
      alAudioFrame(audioCmdList, &cmdLen, (s16*) audioBufferAddress, AUDIO_BUFSZ);
      memcpy(rdram + commandListAddress, audioCmdList, cmdLen * sizeof(Acmd)); // Write to DRAM the audio command list

      s32 data = 0;
      data = commandListAddress;
      memcpy(dmem + 0xFF0, &data, 4); // Write to DMEM address 0xFF0 the 32 bit address of rdram location of cmdList

      data = cmdLen * sizeof(Acmd);
      memcpy(dmem + 0xFF4, &data, 4); // Write to 0xFF4 a 64 length of rdram cmdList (cmdLen * sizeof(Acmd))

      while (!halted()) {
         run();
      }
      unhalt();

      /* Push Stream */
      u8* output = (rdram + audioBufferAddress);
      memcpy(audio->GetAudioBuffer(), output, AUDIO_BUFSZ * sizeof(s16) * DEVICE_CHANNELS);
      audio->PushToStream();

      timePassed = clock() - startTime;
      std::cout << timePassed << std::endl;

      //int count = audio->GetAudioBufferCount();
      //while (count >= 320) { // ~ 10ms second delay.
      //   // audio->SetDelayBuffer(); // not needed in this strategy?
      //   count = audio->GetAudioBufferCount();
      //}

      /* Deinterleave and mono. */
      //u8 intermediate[AUDIO_BUFSZ * 2];
      //if (x > 2) {
      //   u8* output = (rdram + audioBufferAddress);
      //   //for (u32 i = 0; i < AUDIO_BUFSZ; i++) {
      //   //   intermediate[i*2] = output[i * 4];
      //   //   intermediate[i*2 + 1] = output[i * 4 + 1];
      //   //}
      //   outfile.write((char*)output, AUDIO_BUFSZ * 4);
      //}
   // }

   // outfile.close();
   // unload();
   return 0;
}

