#include <iostream> 
#include <fstream>
#include <vector>
using namespace std;

#include <rsp-interface.hpp>
#include <PR/libaudio.h>


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

}

#define AUDIO_HEAP_SIZE    256 * 1024
#define AUDIO_MAX_VOICES   4
#define AUDIO_MAX_UPDATES  64
#define AUDIO_EVT_COUNT    32
#define AUDIO_FREQUENCY    32000

static __declspec(align(16)) u8 audioHeap[AUDIO_HEAP_SIZE];
static ALHeap audioHp;
static ALGlobals audio_globals;

int main() {
   load();
   power(false);

   u8* imem = static_cast<u8*>(getimem());
   u8* dmem = static_cast<u8*>(getdmem());
   vector<u8> seqFile;

   loadbin("ucode/audio_data.zbin", dmem, 0);
   loadbin("ucode/audio.zbin", imem, 0x80);
   loadfile("samples/seq/rainbow.bin", seqFile);
   swapbytes(17, seqFile.data());

   // TODO: http://en64.shoutwiki.com/wiki/Memory_map_detailed#Audio_Interface_.28AI.29_Registers
   // TODO: Audio Heap
   audioHp.base = audioHeap;
   audioHp.cur = audioHeap;
   audioHp.count = 0;
   audioHp.len = sizeof(audioHeap);

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

   ALCSeq seq = {};
   alCSeqNew(&seq, seqFile.data());
   run();
   unload();



   cout << "Hello World" << endl;
   return 0;
}

