#ifndef SEQ_PARSE_HPP
#define SEQ_PARSE_HPP

#include <vector>
#include <string>
#include <PR/ultratypes.h>

/* AL_MIDIstatus */
// For distinguishing channel number from status
#define AL_MIDI_ChannelMask  0x0F
#define AL_MIDI_StatusMask   0xF0

/* Channel voice messages */
#define AL_MIDI_ChannelVoice       0x80
#define AL_MIDI_NoteOff            0x80
#define AL_MIDI_NoteOn             0x90
#define AL_MIDI_PolyKeyPressure    0xA0
#define AL_MIDI_ControlChange      0xB0
#define AL_MIDI_ChannelModeSelect  0xB0
#define AL_MIDI_ProgramChange      0xC0
#define AL_MIDI_ChannelPressure    0xD0
#define AL_MIDI_PitchBendChange    0xE0

/* System messages */
#define AL_MIDI_SysEx 0xF0 // System Exclusive

/* System common */
#define AL_MIDI_SystemCommon          0xF1
#define AL_MIDI_TimeCodeQuarterFrame  0xF1
#define AL_MIDI_SongPositionPointer   0xF2
#define AL_MIDI_SongSelect            0xF3
#define AL_MIDI_Undefined1            0xF4
#define AL_MIDI_Undefined2            0xF5
#define AL_MIDI_TuneRequest           0xF6
#define AL_MIDI_EOX                   0xF7 // System Exlusive end

/* System real time */
#define AL_MIDI_SystemRealTime  0xF8
#define AL_MIDI_TimingClock     0xF8
#define AL_MIDI_Undefined3      0xF9
#define AL_MIDI_Start           0xFA
#define AL_MIDI_Continue        0xFB
#define AL_MIDI_Stop            0xFC
#define AL_MIDI_Undefined4      0xFD
#define AL_MIDI_ActiveSensing   0xFE
#define AL_MIDI_SystemReset     0xFF
#define AL_MIDI_Meta            0xFF // MIDI Files only

/* AL_MIDIctrl */
#define AL_MIDI_VOLUME_CTRL    0x07
#define AL_MIDI_PAN_CTRL       0x0A
#define AL_MIDI_PRIORITY_CTRL  0x10 // use general purpose controller for priority
#define AL_MIDI_FX_CTRL_0      0x14
#define AL_MIDI_FX_CTRL_1      0x15
#define AL_MIDI_FX_CTRL_2      0x16
#define AL_MIDI_FX_CTRL_3      0x17
#define AL_MIDI_FX_CTRL_4      0x18
#define AL_MIDI_FX_CTRL_5      0x19
#define AL_MIDI_FX_CTRL_6      0x1A
#define AL_MIDI_FX_CTRL_7      0x1B
#define AL_MIDI_FX_CTRL_8      0x1C
#define AL_MIDI_FX_CTRL_9      0x1D
#define AL_MIDI_SUSTAIN_CTRL   0x40
#define AL_MIDI_FX1_CTRL       0x5B
#define AL_MIDI_FX3_CTRL       0x5D

/* AL_MIDImeta */
#define AL_MIDI_META_TEMPO  0x51
#define AL_MIDI_META_EOT    0x2f

#define AL_CMIDI_BLOCK_CODE           0xFE
#define AL_CMIDI_LOOPSTART_CODE       0x2E
#define AL_CMIDI_LOOPEND_CODE         0x2D
#define AL_CMIDI_CNTRL_LOOPSTART      102
#define AL_CMIDI_CNTRL_LOOPEND        103
#define AL_CMIDI_CNTRL_LOOPCOUNT_SM   104
#define AL_CMIDI_CNTRL_LOOPCOUNT_BIG  105


struct Command64 {
   u32 status;
   std::vector<u8> bytes;
   u64 delta;
};

class Track64 {
public:
   //Track64()
   static std::vector<u8> EncodeDelta(u64 delta);
   static u64 DecodeDelta(std::vector<u8>& bytes);


public:
   void AddCommand(Command64* command);
   std::vector<Command64*>& GetCommands() { return commands; }

   u32  GetAveragePitch();

   u32 GetSize()                { return size; }
   void InitLoop()              { loop = size; }
   u32 GetLoopOffset()          { return size - loop; }

   // Save()
private:
   std::vector<Command64*> commands;
   u32 size = 0;
   u32 loop = 0;
   u32 lstatus = 0;
};


class Midi64 {
public:
   Midi64();
   void Parse(std::string file);
   std::vector<Command64*>& GetCommands(u32 track) {
      return tracks[track]->GetCommands();
   }
   u32  GetDivision() { return division; }
   u32  GetAveragePitch(u32 track);
private:
   void ParseTrack(u32 track);
   bool ParseCommand(u32 track);
   u64  ParseDelta(u32 track);

   void AddCommand(u32 track, Command64* command) {
      tracks[track]->AddCommand(command);
   }

   void SetDivision(u32 div) { division = div; }
   void InitLoop(u32 track)      { tracks[track]->InitLoop(); }
   u32  GetLoopOffset(u32 track) { return tracks[track]->GetLoopOffset(); }
   
   u8   GetByte(u32 track);
   // Save()
private:
   std::vector<Track64*> tracks;
   std::vector<u8> bytes;

   u32 BULen[16];
   u32 BUPos[16];
   u32 curLoc[16];

   u32 division = 0;
   u8 lstatus;
};


#endif // SEQ_PARSE