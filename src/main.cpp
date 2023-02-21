#include <iostream> 
#include <fstream>
using namespace std;

#include "rsp-interface.hpp"

void loadbin(string path, uint8_t* mem, uint16_t offset)
{
   ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw std::runtime_error("failed to open file: " + path);
   }
   size_t fileSize = static_cast<size_t>(file.tellg());

   file.seekg(0);
   file.read(reinterpret_cast<char*>(&mem[offset]), fileSize);
   file.close();
}

int main() {
   load();
   power(false);

   uint8_t* imem = static_cast<uint8_t*>(getimem());
   uint8_t* dmem = static_cast<uint8_t*>(getdmem());
   loadbin("ucode/audio_data.zbin", dmem, 0);
   loadbin("ucode/audio.zbin", imem, 0x80);

   run();
   unload();



   cout << "Hello World" << endl;
   return 0;
}

