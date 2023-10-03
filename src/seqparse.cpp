#include "seqparse.hpp"
#include <iostream>
#include <fstream>
#include <cassert>

#define PRINT(S) std::cout << "SEQPARSE: " + (S) << std::endl;

void loadmidifile(std::string path, std::vector<u8>& vec) {
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

#define GETLONG(A) ((bytes[A] << 24) + (bytes[A + 1] << 16) + (bytes[A + 2] << 8) + bytes[A + 3])

// Nintendo64 Sequence File
Midi64::Midi64() {
   for (int i = 0; i < 16; i++) {
      Track64* track = new Track64();
      tracks.push_back(track);
   }
   memset(BUPos, 0, sizeof(BUPos));
   memset(BULen, 0, sizeof(BULen));
   memset(curLoc, 0, sizeof(curLoc));
}

u8 Midi64::GetByte(u32 track) {
   u8 byte = 0;
   if (BULen[track]) {
      byte = bytes[BUPos[track]++]; BULen[track]--;
   } else {
      byte = bytes[curLoc[track]++];
      if (byte == AL_CMIDI_BLOCK_CODE) {
         /* if here, then got a backup section. Get the amount of
          * backup, and the len of the section. Subtract the amount of
          * backup from the curLoc ptr, and subtract four more, since
          * curLoc has been advanced by four while reading the codes.
          */
         u8 nextByte = bytes[curLoc[track]++];
         if (nextByte != AL_CMIDI_BLOCK_CODE) {
            u8 hiBackUp = nextByte;
            u8 loBackUp = bytes[curLoc[track]++];
            u8 len = bytes[curLoc[track]++];
            
            u16 backup = (hiBackUp << 8) | loBackUp;
            BUPos[track] = curLoc[track] - (backup + 4);
            BULen[track] = len;

            byte = bytes[BUPos[track]++]; BULen[track]--;
         } else {
            assert(false && "Invalid byte!");
         }
      }
   }
   return byte;
}

void Midi64::Parse(std::string file) {
   PRINT(file);
   loadmidifile(file, bytes);

   // Get Division
   u32 division = GETLONG(64);
   SetDivision(division);
   PRINT("DIVISION = " + std::to_string(division));

   // Get Track Offsets
   for (int i = 0; i < 64; i += 4) {
      curLoc[i >> 2] = GETLONG(i);
   }

   for (int i = 0; i < 16; i++) {
      ParseTrack(i);
   }
   // No more need for the bytes
   bytes.clear();
}

void Midi64::ParseTrack(u32 track) {
   if (curLoc[track] == 0) { return; }
   
   lstatus = 0;
   while (!ParseCommand(track)) {}
}

bool Midi64::ParseCommand(u32 track) {
   u64 delta = ParseDelta(track);
   u8 status = GetByte(track);
   
   if (status == AL_MIDI_Meta) {
      u8 etype = GetByte(track);
      status = 0;
      
      if (etype == AL_MIDI_META_TEMPO) {
         Command64* command = new Command64();
         u8 b1 = GetByte(track); u8 b2 = GetByte(track); u8 b3 = GetByte(track);
         // u32 tempo = (b1 << 16) + (b2 << 8) + b3;
         // PRINT("Tempo: " + std::to_string(tempo))
         command->status = AL_MIDI_Meta;
         command->bytes.push_back(AL_MIDI_META_TEMPO);
         command->bytes.push_back(b1);
         command->bytes.push_back(b2);
         command->bytes.push_back(b3);
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (etype == AL_CMIDI_LOOPSTART_CODE) {
         u8 b1 = GetByte(track); u8 b2 = GetByte(track);
         // assert(false);
      }
      else if (etype == AL_CMIDI_LOOPEND_CODE) {
         u8 b1 = GetByte(track); u8 b2 = GetByte(track); u8 b3 = GetByte(track);
         u8 b4 = GetByte(track); u8 b5 = GetByte(track); u8 b6 = GetByte(track);
         // assert(false);
      }
      else if (etype == AL_MIDI_META_EOT) {
         lstatus = 0;
         return true;
         //assert(false);
      }
      else {
         assert(false);
      }
   }
   else {
      u8 byte1 = 0; u8 byte2 = 0;
      if ((status & 0x80) == 0x80) {
         byte1 = GetByte(track);
      }
      else {
         byte1 = status;
         status = lstatus;
      }
      
      u8 mesg = status & 0xF0;
      // u8 channel = (status & 0x0F) + 1;
      assert((byte1 & 0x80) == 0x00);

      if ((mesg != AL_MIDI_ProgramChange) && (mesg != AL_MIDI_ChannelPressure)) {
         byte2 = GetByte(track);
      }

      if (mesg == AL_MIDI_NoteOn) {
         Command64* command = new Command64();

         u64 duration = ParseDelta(track);
         //PRINT(" NOTE DURATION = " + std::to_string(duration));

         std::vector<u8> encDuration = Track64::EncodeDelta(duration);

         command->status = status;
         command->bytes.push_back(byte1);
         command->bytes.push_back(byte2);
         command->bytes.insert(command->bytes.end(), encDuration.begin(), encDuration.end());
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (mesg == AL_MIDI_ProgramChange) {
         // assert(false);
      }
      else if (mesg == AL_MIDI_ControlChange) {
         // assert(false);
      }
   }

   lstatus = status;
   return false;
}

u64 Midi64::ParseDelta(u32 track) {
   u64 delta = GetByte(track);
   if (delta & 0x80) {
      delta &= 0x7F;
      while (1) {
         u8 c = GetByte(track);
         delta = (delta << 7) + (c & 0x7F);
         if ((c & 0x80) == 0) {
            break;
         }
      }
   }
   return delta;
}

// TRACK
u64 Track64::DecodeDelta(std::vector<u8>& bytes) {
   int i = 0; assert(bytes.size() > i);
   u64 delta = bytes[i];

   if (delta & 0x80) {
      delta &= 0x7F;
      while (1) {
         i++; assert(bytes.size() > i);
         u8 c = bytes[i];
         delta = (delta << 7) + (c & 0x7F);
         if ((c & 0x80) == 0) {
            break;
         }
      }
   }
   return delta;
}

std::vector<u8> Track64::EncodeDelta(u64 delta) {
   std::vector<u8> reverseBytes, bytes;
   u8 c = 0x00;
   while (delta >= 0x7F) {
      u8 byte = (delta & 0x7F) | c;
      bytes.push_back(byte);
      delta = delta >> 7;
      c = 0x80;
   }
   bytes.push_back(delta | c);

   /* reverse bytes */
   for (int i = bytes.size() - 1; i >= 0; i--) {
      reverseBytes.push_back(bytes[i]);
   }
   return reverseBytes;
}

void Track64::AddCommand(Command64* command) {
   commands.push_back(command);
   u32 mesg = command->status;
   u32 extra = 0;
   if (mesg == AL_MIDI_Meta) {
      lstatus = 0;
      extra = 1;
   }
   else if (lstatus != mesg) {
      lstatus = mesg;
      extra = 1;
   }
   size += extra + command->bytes.size() + EncodeDelta(command->delta).size();
}