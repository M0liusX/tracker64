#include "bank64.hpp"
#include <cassert>

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

void SaveBank(std::vector<unsigned char>& dataChunk,
              Bank64* bank)
{
   push16(dataChunk, bank->instruments.size());
   push8(dataChunk, 0); // flag
   push8(dataChunk, bank->pad);
   push32(dataChunk, bank->sampleRate);
   push32(dataChunk, 0); // TODO: percussion
   //s32                 instArray[1];  /* ARRAY of instruments            */
   for (auto& inst : bank->instruments) {
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
      //   s32         soundArray[1];
      for (auto& sound : inst.sounds) {

         //         s32         envelope;
         push32(dataChunk, sound.envelope.attackTime);
         push32(dataChunk, sound.envelope.decayTime);
         push32(dataChunk, sound.envelope.releaseTime);
         push8(dataChunk, sound.envelope.attackVol);
         push8(dataChunk, sound.envelope.decayVol);
         push16(dataChunk, 0);

         //         s32         keyMap;
         push8(dataChunk, sound.keymap.velocityMin);
         push8(dataChunk, sound.keymap.velocityMax);
         push8(dataChunk, sound.keymap.keyMin);
         push8(dataChunk, sound.keymap.keyMax);
         push8(dataChunk, sound.keymap.keyBase);
         push8(dataChunk, sound.keymap.detune);
         push16(dataChunk, 0);

         //         s32         wavetable;
         push32(dataChunk, sound.wave.raw);
         push32(dataChunk, sound.wave.len);
         push32(dataChunk, 0); // type:8, flag:8, padding:16
         // s32 loop
         // s32 book


         // book
         push32(dataChunk, sound.wave.book.order);
         push32(dataChunk, sound.wave.book.predictors);
         int* bookData = (int*)sound.wave.book.pages;
         for (int i = 0; i < sound.wave.book.order * sound.wave.book.predictors * 4; i++) {
            push32(dataChunk, *bookData);
            bookData++;
         }

         // loop
         if (sound.wave.loop.valid) {
            push32(dataChunk, sound.wave.loop.start);
            push32(dataChunk, sound.wave.loop.end);
            push32(dataChunk, sound.wave.loop.count);
            int* loopData = (int*)sound.wave.loop.state;
            for (int i = 0; i < 8; i++) {
               push32(dataChunk, *loopData);
               loopData++;
            }
            push32(dataChunk, 0); // padding
         }

         push8(dataChunk, sound.samplePan);
         push8(dataChunk, sound.sampleVolume);
         push8(dataChunk, 0); // flags
         push8(dataChunk, 0); // padding
      }
    }
}

void SaveBankFile(std::string filename, 
                  BankFile64* bankfile)
{
    assert(false);
    return;
}
