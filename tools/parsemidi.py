import math

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
        return bytearray(f.read())

def get_long(addr):
    return (seq[addr] << 24) + (seq[addr + 1] << 16) + (seq[addr + 2] << 8) + seq[addr + 3]

BULen = [0] * 16
BUPos = [0] * 16
curLoc = [0] * 16
def get_byte(track):
   byte = 0
   if BULen[track] != 0:
      byte = seq[BUPos[track]]
      BUPos[track] += 1
      BULen[track] -= 1
   else:
      byte = seq[curLoc[track]]
      curLoc[track] += 1
      if byte == AL_CMIDI_BLOCK_CODE:
         nextByte = seq[curLoc[track]]
         curLoc[track] += 1
         if nextByte != AL_CMIDI_BLOCK_CODE:
            # if here, then got a backup section. get the amount of
            # backup, and the len of the section. Subtract the amount of
            # backup from the curLoc ptr, and subtract four more, since
            # curLoc has been advanced by four while reading the codes.
            hiBackUp = nextByte
            loBackUp = seq[curLoc[track]]
            curLoc[track] += 1
            theLen = seq[curLoc[track]]
            curLoc[track] += 1
            backup = hiBackUp
            backup = backup << 8
            backup += loBackUp
            # value = seq[curLoc[track]] - (backup + 4)
            BUPos[track] = curLoc[track] - (backup + 4)
            BULen[track] = theLen

            # now get the byte
            byte = seq[BUPos[track]]
            BUPos[track] +=1
            BULen[track] -=1
         else:
            assert(False)
      
   return byte

def parse_midi():
   division = get_long(64)
   for i in range(0, 64, 4):
      curLoc[i >> 2] = get_long(i)
   
   print("Division: " + str(division))
   parse_track(0, division)
   # for t in trackOffsets:
   #    parse_track(t)

def parse_track(track, division):
   if curLoc[track] == 0: return

   lstatus = 0
   ticks = 0
   while True:
      delta = parse_delta(track)
      ticks += delta
      print(ticks/division, end=": ")
      lstatus, done = parse_command(track, lstatus)
      if done: break

def parse_delta(track):
   delta = get_byte(track)
   if delta & 0x80:
      delta = delta & 0x7F
      while True:
         c = get_byte(track)
         delta = (delta << 7) + (c & 0x7f)
         if (c & 0x80) == 0:
            break
   return delta

NOTES = ['C','C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#',' A', 'A#', 'B']
def parse_note(value):
   key = value % 12
   pitch = -1 + math.floor(value/12)
   return NOTES[key] + str(pitch)

def parse_command(track, lstatus):
   status = get_byte(track)

   if status == AL_MIDI_Meta:
      etype = get_byte(track)

      if etype == AL_MIDI_META_TEMPO:
         status = 0
         tempo = (get_byte(track) << 16) + (get_byte(track) << 8) + get_byte(track)
         print("Tempo: " + str(tempo))
      elif etype == AL_CMIDI_LOOPSTART_CODE:
         get_byte(track); get_byte(track) # Two unused bytes
         status = 0
         print("Loop Start")
      elif etype == AL_CMIDI_LOOPEND_CODE:
         # Skip 6 Bytes Used For Looping
         # 1   LoopCt
         # 2   curLpCt: 0xFF means loop forever
         # 3-6 offset (big endian)
         get_byte(track); get_byte(track); get_byte(track);
         get_byte(track); get_byte(track); get_byte(track);
         status = 0
         print("Loop END")
      elif etype == AL_MIDI_META_EOT:
         print("TRACK END")
         return 0, True
      else:
         print(hex(etype))
         assert(False)
   else:
      byte = 0; byte2 = 0;
      if (status & 0x80) == 0x80:
         byte = get_byte(track)
      else:
         byte = status
         status = lstatus

      mesg = status & 0xf0
      if mesg != AL_MIDI_ProgramChange and mesg != AL_MIDI_ChannelPressure:
         byte2 = get_byte(track)

      if mesg == AL_MIDI_NoteOn:
            duration = parse_delta(track)
            print("Note On: "+ parse_note(byte) + " V:" + str(byte2) + " D:" + str(duration))
      elif mesg == AL_MIDI_ProgramChange:
            print("Program Change: " + str(byte))
      elif mesg == AL_MIDI_ControlChange:
         if byte == AL_MIDI_VOLUME_CTRL:
            print("Control Change (Vol): " + str(byte2))
         elif byte == AL_MIDI_PAN_CTRL:
            print("Control Change (Pan): " + str(byte2))
         elif byte == AL_MIDI_FX1_CTRL:
            print("Control Change (Reverb): " + str(byte2))
         else:
            print(hex(byte))
            assert(False)
      else:
         print("ERROR: " + hex(status) )
         assert(False)

   return status, False


### MAIN ###
seq = read_bin("../samples/seq/rainbow.bin")
parse_midi()