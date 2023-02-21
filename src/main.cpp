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
void swapbytes(uint8_t count, uint8_t* bytes) {
   for (uint8_t i = 0; i < count*4; i+=4) {
      uint8_t temp;

      temp = bytes[i];
      bytes[i] = bytes[i + 3];
      bytes[i + 3] = temp;

      temp = bytes[i + 1];
      bytes[i + 1] = bytes[i + 2];
      bytes[i + 2] = temp;
   }
}

int main() {
   load();
   power(false);

   uint8_t* imem = static_cast<uint8_t*>(getimem());
   uint8_t* dmem = static_cast<uint8_t*>(getdmem());
   vector<uint8_t> seqFile;

   loadbin("ucode/audio_data.zbin", dmem, 0);
   loadbin("ucode/audio.zbin", imem, 0x80);
   loadfile("samples/seq/rainbow.bin", seqFile);
   swapbytes(17, seqFile.data());

   ALCSeq seq = {};
   alCSeqNew(&seq, seqFile.data());
   run();
   unload();



   cout << "Hello World" << endl;
   return 0;
}

