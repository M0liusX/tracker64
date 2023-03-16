### AL_MIDIstatus ###
# For distinguishing channel number from status #
AL_MIDI_ChannelMask         = 0x0F
AL_MIDI_StatusMask          = 0xF0

# Channel voice messages $
AL_MIDI_ChannelVoice        = 0x80
AL_MIDI_NoteOff             = 0x80
AL_MIDI_NoteOn              = 0x90
AL_MIDI_PolyKeyPressure     = 0xA0
AL_MIDI_ControlChange       = 0xB0
AL_MIDI_ChannelModeSelect   = 0xB0
AL_MIDI_ProgramChange       = 0xC0
AL_MIDI_ChannelPressure     = 0xD0
AL_MIDI_PitchBendChange     = 0xE0

# System messages #
AL_MIDI_SysEx               = 0xF0 # System Exclusive

#System common #
AL_MIDI_SystemCommon            = 0xF1
AL_MIDI_TimeCodeQuarterFrame    = 0xF1
AL_MIDI_SongPositionPointer     = 0xF2
AL_MIDI_SongSelect              = 0xF3
AL_MIDI_Undefined1              = 0xF4
AL_MIDI_Undefined2              = 0xF5
AL_MIDI_TuneRequest             = 0xF6
AL_MIDI_EOX                     = 0xF7 # End of System Exclusive

# System real time #
AL_MIDI_SystemRealTime  = 0xF8
AL_MIDI_TimingClock     = 0xF8
AL_MIDI_Undefined3      = 0xF9
AL_MIDI_Start           = 0xFA
AL_MIDI_Continue        = 0xFB
AL_MIDI_Stop            = 0xFC
AL_MIDI_Undefined4      = 0xFD
AL_MIDI_ActiveSensing   = 0xFE
AL_MIDI_SystemReset     = 0xFF
AL_MIDI_Meta            = 0xFF      # MIDI Files only

# enum AL_MIDIctrl
AL_MIDI_VOLUME_CTRL         = 0x07
AL_MIDI_PAN_CTRL            = 0x0A
AL_MIDI_PRIORITY_CTRL       = 0x10 #  use general purpose controller for priority
AL_MIDI_FX_CTRL_0           = 0x14
AL_MIDI_FX_CTRL_1           = 0x15
AL_MIDI_FX_CTRL_2           = 0x16
AL_MIDI_FX_CTRL_3           = 0x17
AL_MIDI_FX_CTRL_4           = 0x18
AL_MIDI_FX_CTRL_5           = 0x19
AL_MIDI_FX_CTRL_6           = 0x1A
AL_MIDI_FX_CTRL_7           = 0x1B
AL_MIDI_FX_CTRL_8           = 0x1C
AL_MIDI_FX_CTRL_9           = 0x1D
AL_MIDI_SUSTAIN_CTRL        = 0x40
AL_MIDI_FX1_CTRL            = 0x5B
AL_MIDI_FX3_CTRL            = 0x5D

### AL_MIDImeta ###
AL_MIDI_META_TEMPO          = 0x51
AL_MIDI_META_EOT            = 0x2f

AL_CMIDI_BLOCK_CODE           = 0xFE
AL_CMIDI_LOOPSTART_CODE       = 0x2E
AL_CMIDI_LOOPEND_CODE         = 0x2D
AL_CMIDI_CNTRL_LOOPSTART      = 102
AL_CMIDI_CNTRL_LOOPEND        = 103
AL_CMIDI_CNTRL_LOOPCOUNT_SM   = 104
AL_CMIDI_CNTRL_LOOPCOUNT_BIG  = 105

def read_bin(dir):
    with open(dir, "rb") as f:
        return f.read()

def get_long(addr):
    return (seq[addr] << 24) + (seq[addr + 1] << 16) + (seq[addr + 2] << 8) + seq[addr + 3]

def parse_midi():
   trackOffsets = []
   division = get_long(64)
   for i in range(0, 64, 4):
      trackOffsets.append(get_long(i))
   
   print("Division: " + str(division))
   parse_track(trackOffsets[0])
   # for t in trackOffsets:
   #    parse_track(t)

def parse_track(offset):
   if offset == 0: return
   lstatus = 0
   
   while True:
      offset, delta = parse_delta(offset)
      offset, lstatus = parse_command(offset, lstatus)

def parse_delta(offset):
   delta = seq[offset]; offset += 1
   if delta & 0x80:
      delta = delta & 0x79
      while True:
         c = seq[offset]; offset +=1
         delta = (delta << 7) + (c & 0x7f)
         if (c & 0x80) == 0:
            break
   return offset, delta

def parse_command(offset, lstatus):
   status = seq[offset]; offset += 1

   if status == AL_MIDI_Meta:
      etype = seq[offset]; offset += 1

      if etype == AL_MIDI_META_TEMPO:
         status = 0
         tempo = (seq[offset] << 16) + (seq[offset + 1] << 8) + seq[offset + 2]; offset += 3
         print("Tempo: " + str(tempo))
      elif etype == AL_CMIDI_LOOPSTART_CODE:
         offset += 2
         status = 0
         print("Loop Start")
      # elif etype == AL_CMIDI_LOOPEND_CODE:
      #    offset += 2
      #    status = 0
      #    print("Loop Start")
      else:
         print(hex(etype))
         assert(False)
   else:
      byte = 0; byte2 = 0;
      if (status & 0x80) == 0x80:
         byte = seq[offset]; offset += 1
      else:
         byte = status
         status = lstatus

      if status != AL_MIDI_ProgramChange and status != AL_MIDI_ChannelPressure:
         byte2 = seq[offset]; offset += 1
         if status == AL_MIDI_NoteOn:
            if (byte2 == 114):
               print("Here")
            offset, duration = parse_delta(offset)
            print("Note On: "+ hex(byte) + " " + str(byte2) + " " + str(duration))

      elif status == AL_MIDI_ProgramChange:
            print("Program Change: " + str(byte))
      elif status == AL_MIDI_ControlChange:
         if byte == AL_MIDI_VOLUME_CTRL:
            print("Control Change (Vol): " + str(byte2))
         elif byte == AL_MIDI_PAN_CTRL:
            print("Control Change (Pan): " + str(byte2))
         else:
            print(hex(byte))
            assert(False)
      else:
         print("ERROR: " + hex(status) )
         assert(False)

   return offset, status


### MAIN ###
seq = read_bin("../samples/seq/rainbow.bin")
parse_midi()