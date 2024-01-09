#include "seqparse.hpp"
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
         Command64* command = new Command64();
         command->status = AL_MIDI_Meta;
         command->bytes.push_back(AL_CMIDI_LOOPSTART_CODE);
         command->bytes.push_back(b1);
         command->bytes.push_back(b2);
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (etype == AL_CMIDI_LOOPEND_CODE) {
         u8 b1 = GetByte(track); u8 b2 = GetByte(track); u8 b3 = GetByte(track);
         u8 b4 = GetByte(track); u8 b5 = GetByte(track); u8 b6 = GetByte(track);

         /* Generate Uncompressed Offset 
          * This is required because we are saving
          * commands without utilizing cseq blocks
          */
         u32 offset = GetLoopOffset(track) + Track64::EncodeDelta(delta).size() + 8;
         b3 = (0xFF000000 & offset) >> 24;
         b4 = (0x00FF0000 & offset) >> 16;
         b5 = (0x0000FF00 & offset) >> 8;
         b6 = (0x000000FF & offset);

         Command64* command = new Command64();
         command->status = AL_MIDI_Meta;
         command->bytes.push_back(AL_CMIDI_LOOPEND_CODE);
         command->bytes.push_back(b1);
         command->bytes.push_back(b2);
         command->bytes.push_back(b3);
         command->bytes.push_back(b4);
         command->bytes.push_back(b5);
         command->bytes.push_back(b6);
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (etype == AL_MIDI_META_EOT) {
         lstatus = 0;
         Command64* command = new Command64();
         command->status = AL_MIDI_Meta;
         command->bytes.push_back(AL_MIDI_META_EOT);
         command->bytes.push_back(0);
         command->delta = delta;
         AddCommand(track, command);
         return true;
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
         Command64* command = new Command64();
         command->status = status;
         command->bytes.push_back(byte1);
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (mesg == AL_MIDI_ControlChange) {
         Command64* command = new Command64();
         command->status = status;
         command->bytes.push_back(byte1);
         command->bytes.push_back(byte2);
         command->delta = delta;
         AddCommand(track, command);
      }
      else if (mesg == AL_MIDI_PitchBendChange) {
         Command64* command = new Command64();
         command->status = status;
         command->bytes.push_back(byte1);
         command->bytes.push_back(byte2);
         command->delta = delta;
         AddCommand(track, command);
      }
      else {
         assert(false);
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

void Track64::Save(std::ofstream& outFile) {
   u8 lstatus = 0;
   for (Command64* command : commands) {
      std::vector<u8> deltaBytes = EncodeDelta(command->delta);
      outFile.write(reinterpret_cast<char*>(deltaBytes.data()), deltaBytes.size());

      u8 mesg = command->status;
      if (mesg == AL_MIDI_Meta) {
         outFile.write(reinterpret_cast<const char*>(&mesg), sizeof(mesg));
         lstatus = 0;
      }
      else if (lstatus != mesg) {
         outFile.write(reinterpret_cast<const char*>(&mesg), sizeof(mesg));
         lstatus = mesg;
      }
      outFile.write(reinterpret_cast<char*>(command->bytes.data()), command->bytes.size());
   }

}

void Midi64::Save(std::string file) {
   // Open a file for writing (truncating the existing file or creating a new one)
   std::ofstream outFile(file, std::ios::binary);

   if (!outFile.is_open()) {
      std::cerr << "Error opening the file for writing." << std::endl;
      assert(false);
      return;
   }

   u32 offset = 68;
   for (Track64* track : tracks) {
      u32 value = 0;
      u32 size = track->GetSize();
      if (size != 0) {
         assert(offset <= 4294967295);
         value = offset;
         offset += size;
      }

      // Convert the uint32_t value to big-endian format
      u8 bytes[4];
      bytes[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
      bytes[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
      bytes[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
      bytes[3] = static_cast<uint8_t>(value & 0xFF);
      // Write the bytes to the file
      outFile.write(reinterpret_cast<char*>(bytes), sizeof(bytes));
   }

   // Write division
   u8 bytes[4];
   bytes[0] = static_cast<uint8_t>((division >> 24) & 0xFF);
   bytes[1] = static_cast<uint8_t>((division >> 16) & 0xFF);
   bytes[2] = static_cast<uint8_t>((division >> 8) & 0xFF);
   bytes[3] = static_cast<uint8_t>(division & 0xFF);
   // Write the bytes to the file
   outFile.write(reinterpret_cast<char*>(bytes), sizeof(bytes));

   for (Track64* track : tracks) {
      if (track->GetSize() > 0) {
         track->Save(outFile);
      }
   }
   // Close the file
   outFile.close();
   std::cout << "New sequence successfully written!" << std::endl;
}

u32 Midi64::GetAveragePitch(u32 track) {
   return tracks[track]->GetAveragePitch();
}

u32 Track64::GetAveragePitch() {
   u32 totalNotes = 0;
   u32 sum = 0;
   for (Command64* command : commands) {
      if ((command->status & 0xf0) == AL_MIDI_NoteOn) {
         sum += command->bytes[0];
         totalNotes += 1;
      }
   }
   if (totalNotes == 0) {
      return 0;
   }
   return sum / totalNotes;
}