#include "bank64.hpp"

#include <iostream>
#include <fstream>
#include <cassert>
#include <map>

typedef std::vector<unsigned char> BYTES;

void push32(BYTES& bytes, int v) {
   bytes.push_back((v >> 24) & 0x000000FF);
   bytes.push_back((v >> 16) & 0x000000FF);
   bytes.push_back((v >> 8)  & 0x000000FF);
   bytes.push_back( v        & 0x000000FF);
}

void push16(BYTES& bytes, short s) {
   bytes.push_back((s >> 8) & 0x00FF);
   bytes.push_back( s       & 0x00FF);
}

void push8(BYTES& bytes, unsigned char c) {
   bytes.push_back(c);
}

void SaveFile(std::string filename, BYTES& bytes, bool append = false) {
   // Open a file for writing (truncating the existing file or creating a new one)
   int flags = std::ios::binary;
   if (append) {
      flags |= std::ios::app;
   }
   std::ofstream outFile(filename, flags);

   if (!outFile.is_open()) {
      std::cerr << "Error opening the file for writing." << std::endl;
      assert(false);
      return;
   }

   outFile.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
   outFile.close();
}


// Sound Data with Optional Wave Data
std::map<unsigned int, unsigned int> waveMap;
void SaveBank(BankChunk* bankChunk,
              Bank64* bank,
              unsigned int pos)
{
   BYTES& dataChunk = bankChunk->dataChunk;
   dataChunk.clear();
   // Envelopes
   unsigned int offset = pos;
   unsigned int envelopeStart = offset;
   for (auto& inst : bank->instruments) {
      for (auto& sound : inst.sounds) {
         push32(dataChunk, sound.envelope.attackTime);
         push32(dataChunk, sound.envelope.decayTime);
         push32(dataChunk, sound.envelope.releaseTime);
         push8(dataChunk, sound.envelope.attackVol);
         push8(dataChunk, sound.envelope.decayVol);
         push16(dataChunk, 0);
         offset += 16;
      }
   }

   // Keymaps
   unsigned int keymapStart = offset;
   for (auto& inst : bank->instruments) {
      for (auto& sound : inst.sounds) {
         push8(dataChunk, sound.keymap.velocityMin);
         push8(dataChunk, sound.keymap.velocityMax);
         push8(dataChunk, sound.keymap.keyMin);
         push8(dataChunk, sound.keymap.keyMax);
         push8(dataChunk, sound.keymap.keyBase);
         push8(dataChunk, sound.keymap.detune);
         push16(dataChunk, 0);
         offset += 8;
      }
   }

   std::vector<unsigned int> soundStarts;
   unsigned int soundIdx = 0;
   for (auto& inst : bank->instruments) {
      for (auto& sound : inst.sounds) {
         // New wave to add
         if (waveMap.find(sound.wave.id) == waveMap.end()) {
            // Wave Book
            unsigned int bookStart = offset;
            push32(dataChunk, sound.wave.book.order);
            push32(dataChunk, sound.wave.book.predictors);
            int* bookData = (int*)sound.wave.book.pages;
            for (int i = 0; i < sound.wave.book.order * sound.wave.book.predictors * 4; i++) {
               push32(dataChunk, *bookData);
               bookData++;
            }
            offset += 8 + 4 * (sound.wave.book.order * sound.wave.book.predictors * 4);

            // Wave Loop
            unsigned int loopStart = 0;
            if (sound.wave.loop.valid) {
               loopStart = offset;
               push32(dataChunk, sound.wave.loop.start);
               push32(dataChunk, sound.wave.loop.end);
               push32(dataChunk, sound.wave.loop.count);
               int* loopData = (int*)sound.wave.loop.state;
               for (int i = 0; i < 8; i++) {
                  push32(dataChunk, *loopData);
                  loopData++;
               }
               push32(dataChunk, 0); // padding
               offset += 48;
            }

            // Wave Data Start
            waveMap[sound.wave.id] = offset;
            push32(dataChunk, sound.wave.raw);
            push32(dataChunk, sound.wave.len);
            push32(dataChunk, 0); // type:8, flag:8, padding:16
            push32(dataChunk, loopStart);
            push32(dataChunk, bookStart);
            push32(dataChunk, 0); // padding
            offset += 24;
         }

         // Sound Data Start
         soundStarts.push_back(offset);
         push32(dataChunk, envelopeStart + (16 * soundIdx));
         push32(dataChunk, keymapStart + (8 * soundIdx));
         push32(dataChunk, waveMap[sound.wave.id]);
         push8(dataChunk, sound.samplePan);
         push8(dataChunk, sound.sampleVolume);
         push8(dataChunk, 0); // flags
         push8(dataChunk, 0); // padding
         offset += 16;
         soundIdx++;
      }
   }

   // Instrument Data
   std::vector<unsigned int> instStarts;
   soundIdx = 0;
   for (auto& inst : bank->instruments) {
      instStarts.push_back(offset);
      push8(dataChunk, inst.volume);
      push8(dataChunk, inst.pan);
      push8(dataChunk, inst.priority);
      push8(dataChunk, 0); // flags
      push8(dataChunk, inst.tremType);
      push8(dataChunk, inst.tremRate);
      push8(dataChunk, inst.tremDepth);
      push8(dataChunk, inst.tremDelay);
      push8(dataChunk, inst.vibType);
      push8(dataChunk, inst.vibRate);
      push8(dataChunk, inst.vibDepth);
      push8(dataChunk, inst.vibDelay);
      push16(dataChunk, inst.bendRange);
      push16(dataChunk, inst.sounds.size());
      for (int i = 0; i < inst.sounds.size(); i++) {
         push32(dataChunk, soundStarts[soundIdx]);
         soundIdx++;
      }
      offset += 16 + 4 * inst.sounds.size();
      if ((inst.sounds.size() % 2) != 0) {
         push32(dataChunk, 0); // padding
         offset += 4;
      }
   }

   // Bank Data Start
   bankChunk->dataStart = offset;
   push16(dataChunk, bank->instruments.size());
   push8(dataChunk, 0); // flag
   push8(dataChunk, bank->pad);
   push32(dataChunk, bank->sampleRate);
   push32(dataChunk, 0); // TODO: percussion
   for (int i = 0; i < bank->instruments.size(); i++) {
      push32(dataChunk, instStarts[i]);
   }
   offset += 12 + 4 * bank->instruments.size();
   if ((bank->instruments.size() % 2) == 0) {
      push32(dataChunk, 0); // padding
      offset += 4;
   }
   
   SaveFile("temp.bank", dataChunk);
   //std::cout << offset << std::endl;
}

void SaveBankFile(std::string filename, 
                  BankFile64* bankfile)
{
   // Generate Header
   int bankCount = bankfile->bankDataChunks.size();
   int usePadding = (bankCount + 1) % 2;
   BYTES header;
   push16(header, 0x4231); // 0x4231 big endian
   push16(header, bankCount);
   for (int i = 0; i < bankCount; i++) {
      int start = bankfile->bankDataChunks[i].dataStart;
      push32(header, start);
   }
   if (usePadding) {
      push32(header, 0);
   }

   // Save header and append bankChunks
   SaveFile(filename, header);
   for (int i = 0; i < bankCount; i++) {
      SaveFile(filename, bankfile->bankDataChunks[i].dataChunk, true);
   }
   waveMap.clear();

}
