#include <iostream> 
#include <fstream>
#include <vector>
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

void audioDmaNew() {
   cout << "Hello DMA!" << endl;
}

#define AUDIO_HEAP_SIZE    256 * 1024
#define AUDIO_MAX_VOICES   4
#define AUDIO_MAX_UPDATES  64
#define AUDIO_MAX_CHANNELS 16
#define AUDIO_EVT_COUNT    32
#define AUDIO_FREQUENCY    32000

#define AUDIO_BUFSZ        1024
#define AUDIO_CLIST_SIZE   4 * 1024

static __declspec(align(16)) u8 audioHeap[AUDIO_HEAP_SIZE];
static ALHeap audioHp;
static ALGlobals audioGlobals;
static ALCSPlayer cseqPlayer;

static __declspec(align(16)) s16 audioBuffers[2][AUDIO_BUFSZ];
static Acmd audioCmdList[AUDIO_CLIST_SIZE];

int main() {
   load();
   power(false);

   u8* imem = static_cast<u8*>(getimem());
   u8* dmem = static_cast<u8*>(getdmem());
   u8* rdram = static_cast<u8*>(getrdram());
   vector<u8> seqFile;
   vector<u8> ctlFile;
   vector<u8> tblFile;

   loadbin("ucode/audio_data.zhbin", dmem, 0); // rsp udata
   loadbin("ucode/audio.zbin", imem, 0x80); // rsp ucode
   loadfile("samples/seq/rainbow.bin", seqFile); // mp1 mario board seq
   loadfile("samples/ctl/soundbank1.ctl", ctlFile); // mp1 soundbank file
   loadfile("samples/tbl/wavetable1.tbl", tblFile); // mp1 wavetable file
   swapbytes(17, seqFile.data());
   s32 seqFileAddress = 0;
   s32 ctlFileAddress = seqFile.size();
   s32 tblFileAddress = ctlFileAddress + ctlFile.size();
   s32 commandListAddress = tblFileAddress + tblFile.size();
   memcpy(rdram + seqFileAddress, seqFile.data(), seqFile.size());
   memcpy(rdram + ctlFileAddress, ctlFile.data(), ctlFile.size());
   memcpy(rdram + tblFileAddress, tblFile.data(), tblFile.size());

   // TODO: http://en64.shoutwiki.com/wiki/Memory_map_detailed#Audio_Interface_.28AI.29_Registers
   // TODO: Audio Heap
   audioHp.base = audioHeap;
   audioHp.cur = audioHeap;
   audioHp.count = 0;
   audioHp.len = sizeof(audioHeap);
   // alHeapInit(&audioHp, audioHeap, sizeof(audioHeap));

   s32 audioRate = 0x02E6D354 / (s32) (0x02E6D354 / (float)32000 - .5f);
   ALSynConfig scfg = {
       .maxVVoices = AUDIO_MAX_VOICES,
       .maxPVoices = AUDIO_MAX_VOICES,
       .maxUpdates = AUDIO_MAX_UPDATES,
       .dmaproc = (void*) audioDmaNew, // explained later
       .heap = &audioHp,
       .outputRate = audioRate,
       .fxType = AL_FX_SMALLROOM,
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
   alCSPSetBank(&cseqPlayer, bankFile->getBank(0x20));
   alCSPPlay(&cseqPlayer);

   // TODO: Loop? (till break status?)
   s32 cmdLen = 0;
   alAudioFrame(audioCmdList, &cmdLen, audioBuffers[0], AUDIO_BUFSZ);
   memcpy(rdram + commandListAddress, audioCmdList, cmdLen * sizeof(Acmd));

   s32 data = 0;
   data = commandListAddress;
   memcpy(dmem + 0xFF0, &data, 4); // Write to DMEM address 0xFF0 the 32 bit address of rdram location of cmdList

   data = cmdLen * sizeof(Acmd);
   memcpy(dmem + 0xFF4, &data, 4); // Write to 0xFF4 a 64 length of rdram cmdList (cmdLen * sizeof(Acmd))

   while (!halted()) {
      run();
   }


   unload();
   cout << "Hello World" << endl;
   return 0;
}

