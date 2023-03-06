#include <iostream> 
#include <fstream>
#include <vector>
#include <cassert>
using namespace std;

#include <rsp-interface.hpp>
#include <libaudio.h>


void loadbin(string path, uint8_t* mem, uint16_t offset)
{
   ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw runtime_error("failed to open file: " + path);
   }
   size_t fileSize = static_cast<size_t>(file.tellg());

   file.seekg(0);
   file.read(reinterpret_cast<char*>(&mem[offset]), fileSize);
   file.close();
}
void loadfile(string path, vector<uint8_t>& vec) {
   ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw runtime_error("failed to open file: " + path);
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

#define AUDIO_BUFSZ        1024
#define AUDIO_CLIST_SIZE   32 * 1024

static ALHeap audioHp;
static ALGlobals audioGlobals;
static ALCSPlayer cseqPlayer;
static u8* rdram;

// static __declspec(align(16)) s16 audioBuffers[2][AUDIO_BUFSZ];
static Acmd audioCmdList[AUDIO_CLIST_SIZE];

static s32 audioDmaCallback(s32 addr, s32 len, void* state) {
   // cout << "Hello DMA!" << endl;
   return addr;
}

static ALDMAproc audioNewDma(void* arg) {
   return audioDmaCallback;
}

int main() {
   load();
   power(false);

   u8* imem = static_cast<u8*>(getimem());
   u8* dmem = static_cast<u8*>(getdmem());
   rdram = static_cast<u8*>(getrdram());

   vector<u8> seqFile;
   vector<u8> ctlFile;
   vector<u8> tblFile;

   loadbin("ucode/audio_data.zbin", dmem, 0); // rsp udata
   loadbin("ucode/audio.zbin", imem, 0x80); // rsp ucode
   loadfile("samples/seq/twobeeps.bin", seqFile); // mp1 mario board seq
   loadfile("samples/ctl/soundbank1.ctl", ctlFile); // mp1 soundbank file
   loadfile("samples/tbl/wavetable1.tbl", tblFile); // mp1 wavetable file
   swapbytes(17, seqFile.data());

   s32 seqFileAddress = 0;
   s32 ctlFileAddress = seqFile.size();
   s32 tblFileAddress = ((ctlFileAddress + ctlFile.size()) + 0xF) & (~0xF);
   s32 commandListAddress = tblFileAddress + tblFile.size();
   s32 audioBufferAddress = ((commandListAddress + AUDIO_CLIST_SIZE * sizeof(Acmd)) + 0xF) & (~0xF);
   s32 audioHeapAddress = ((audioBufferAddress + 4 * AUDIO_BUFSZ) + 0xF) & (~0xF);
   assert((AUDIO_HEAP_SIZE + audioHeapAddress) < 0x800000); // do not exceed 8MB ram size for now.
   memcpy(rdram + seqFileAddress, seqFile.data(), seqFile.size());
   memcpy(rdram + ctlFileAddress, ctlFile.data(), ctlFile.size());
   memcpy(rdram + tblFileAddress, tblFile.data(), tblFile.size());

   // TODO: http://en64.shoutwiki.com/wiki/Memory_map_detailed#Audio_Interface_.28AI.29_Registers
   // TODO: Audio Heap
   audioHp.base = rdram + audioHeapAddress;
   audioHp.cur = rdram + audioHeapAddress;
   audioHp.count = 0;
   audioHp.len = AUDIO_HEAP_SIZE;
   // alHeapInit(&audioHp, audioHeap, sizeof(audioHeap));

   s32 audioRate = 0x02E6D354 / (s32) (0x02E6D354 / (f32)32000.0 - .5f);
   ALSynConfig scfg = {
       .maxVVoices = AUDIO_MAX_VOICES,
       .maxPVoices = AUDIO_MAX_VOICES,
       .maxUpdates = AUDIO_MAX_UPDATES,
       .dmaproc = (void*) audioNewDma, // explained later
       .heap = &audioHp,
       .outputRate = audioRate,
       .fxType = AL_FX_NONE,
   };
   alInit(&audioGlobals, &scfg);
   audioGlobals.rdram = rdram;

   ALSeqpConfig cscfg = {
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

   ALCSeq cseq = {};
   alCSeqNew(&cseq, seqFile.data());
   alCSPSetSeq(&cseqPlayer, &cseq);


   ALBankFile* bankFile = (ALBankFile*)getRamObject(ctlFileAddress);
   alBnkfNew(ctlFileAddress, tblFileAddress);
   alCSPSetBank(&cseqPlayer, bankFile->getBank(0x0));
   alCSPPlay(&cseqPlayer);

   ofstream outfile("mono.sw", ofstream::binary);

   while (true) {
      // Clean DMEM?
      memset(dmem + 0x2E0, 0, 0x1000 - 0x2E0);

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
      
      /* Deinterleave and mono. */
      u8 intermediate[AUDIO_BUFSZ * 2];
      u8* output = (rdram + audioBufferAddress);
      for (u32 i = 0; i < AUDIO_BUFSZ; i++) {
         intermediate[i*2] = output[i * 4];
         intermediate[i*2 + 1] = output[i * 4 + 1];
      }
      outfile.write((char*)intermediate, AUDIO_BUFSZ * 2);
   }
   outfile.close();
   unload();
   cout << "Hello World" << endl;
   return 0;
}

