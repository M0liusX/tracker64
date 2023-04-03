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


class track64:
   def __init__(self):
      self.commands = []
      self.size = 0
      self.loop = 0
      self.lstatus = 0
   def delay_size(self, delay):
      if (delay == 0): 
         return 1
      else:
         return math.floor(math.log(delay, 128)) + 1
   def add_command(self, command):
      self.commands.append(command)
      mesg = command[0]
      extra = 0
      if mesg == AL_MIDI_Meta:
         self.lstatus = 0
         extra = 1
      elif self.lstatus != mesg:
         self.lstatus = mesg
         extra = 1
      self.size += extra + len(command[1]) + len(encode_delta(command[2]))
   def get_size(self):
      return self.size
   def init_loop(self):
      self.loop = self.size
   def get_loop_offset(self):
      return self.size - self.loop
   def save(self, file):
      lstatus = 0
      for command in self.commands:
         # write delay
         for byte in encode_delta(command[2]):
            file.write(byte.to_bytes(1, 'big'))
         
         # write mesg
         mesg = command[0]
         if mesg == AL_MIDI_Meta:
            file.write(mesg.to_bytes(1, 'big'))
            lstatus = 0
         elif lstatus != mesg:
            file.write(mesg.to_bytes(1, 'big'))
            lstatus = mesg

         # write mesg data
         for byte in command[1]:
            file.write(byte.to_bytes(1, 'big'))


class midi64:
   def __init__(self):
      self.tracks = [track64() for i in range(16)]
      self.division = 0
   def set_division(self, div):
      self.division = div
   def save(self, dir):
      offset = 68
      with open(dir, "wb") as f:
         for t in self.tracks:
            if t.get_size() == 0:
               f.write(int(0).to_bytes(4, 'big'))
            else:
               assert(offset <= 4,294,967,295 ) # seq can't be this big
               f.write(offset.to_bytes(4, 'big'))
               offset += t.get_size()
         f.write(self.division.to_bytes(4, 'big'))
         for t in self.tracks:
            if t.get_size() == 0:
               continue
            t.save(f)
   def add_command(self, track, command):
         self.tracks[track].add_command(command)
   def init_loop(self, track):
         self.tracks[track].init_loop()
   def get_loop_offset(self, track):
         return self.tracks[track].get_loop_offset()


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
   midi.set_division(division)
   for i in range(0, 64, 4):
      curLoc[i >> 2] = get_long(i)
   
   print("Division: " + str(division))
   for i in range(16):
      parse_track(i, division)

def parse_track(track, division):
   if curLoc[track] == 0: return

   lstatus = 0
   # ticks = 0
   while True:
      # delta = parse_delta(track)
      # ticks += delta
      # print(ticks/division, end=": ")
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

def encode_delta(delta):
   bytes = []
   c = 0x00
   while delta >= 0x7F:
      byte = (delta & 0x7F) | c
      bytes.append(byte)
      delta = delta >> 7
      c = 0x80
   bytes.append(delta | c)
   bytes.reverse()
   return bytes

NOTES = ['C','C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#',' A', 'A#', 'B']
def parse_note(value):
   key = value % 12
   pitch = -1 + math.floor(value/12)
   return NOTES[key] + str(pitch)

def parse_command(track, lstatus):
   delta = parse_delta(track)
   status = get_byte(track)

   if status == AL_MIDI_Meta:
      etype = get_byte(track)
      status = 0

      if etype == AL_MIDI_META_TEMPO:

         b1 = get_byte(track); b2 = get_byte(track); b3 = get_byte(track);
         tempo = (b1 << 16) + (b2 << 8) + b3
         midi.add_command(track, (AL_MIDI_Meta, [AL_MIDI_META_TEMPO, b1, b2, b3], delta))
         print("Tempo: " + str(tempo))
      elif etype == AL_CMIDI_LOOPSTART_CODE:
         b1 = get_byte(track); b2 = get_byte(track) # Two unused bytes
         midi.add_command(track, (AL_MIDI_Meta, [AL_CMIDI_LOOPSTART_CODE, b1, b2], delta))
         midi.init_loop(track)
         print("Loop Start")
      elif etype == AL_CMIDI_LOOPEND_CODE:
         # Skip 6 Bytes Used For Looping
         # 1   LoopCt
         # 2   curLpCt: 0xFF means loop forever
         # 3-6 offset (big endian)
         b1 = get_byte(track); b2 = get_byte(track); b3 = get_byte(track);
         b4 = get_byte(track); b5 = get_byte(track); b6 = get_byte(track);
         offset = midi.get_loop_offset(track) + len(encode_delta(delta)) + 8
         b3 = (0xFF000000 & offset) >> 24
         b4 = (0x00FF0000 & offset) >> 16
         b5 = (0x0000FF00 & offset) >> 8
         b6 = (0x000000FF & offset)
         midi.add_command(track, (AL_MIDI_Meta, [AL_CMIDI_LOOPEND_CODE, b1,b2,b3,b4,b5,b6], delta))
         print("Loop END")
      elif etype == AL_MIDI_META_EOT:
         midi.add_command(track, (AL_MIDI_Meta, [AL_MIDI_META_EOT, 0], delta))
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
      channel = (status & 0x0f) + 1
      print("Channel " + str(channel), end= ":: ")
      assert( (byte   & 0x80) == 0) # first data byte always less than 0x80

      if mesg != AL_MIDI_ProgramChange and mesg != AL_MIDI_ChannelPressure:
         byte2 = get_byte(track)

      if mesg == AL_MIDI_NoteOn:
            duration = parse_delta(track)
            midi.add_command(track, (status, [byte, byte2] + encode_delta(duration), delta))
            print("Note On: "+ parse_note(byte) + " V:" + str(byte2) + " D:" + str(duration))
      elif mesg == AL_MIDI_ProgramChange:
            midi.add_command(track, (status, [byte], delta))
            print("Program Change: " + str(byte))
      elif mesg == AL_MIDI_ControlChange:
         midi.add_command(track, (status, [byte, byte2], delta))
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

midi = midi64()
seq = read_bin("../samples/seq/rainbow.bin")
parse_midi()
midi.save("../samples/seq/rainbowexport.bin")