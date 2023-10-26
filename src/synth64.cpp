#include <iostream> 
#include <fstream>
#include <vector>
#include <cassert>
#include <ctime>

#include <rsp-interface.hpp>
#include <libaudio.h>

#include "audio/MeAudio.hpp"
#define FRAME_TIME (CLOCKS_PER_SEC / (60.0f))

#include "synth64.hpp"

/* WAV Header */
typedef struct WAV_HEADER {
   /* RIFF Chunk Descriptor */
   uint8_t RIFF[4] = { 'R', 'I', 'F', 'F' }; // RIFF Header Magic header
   uint32_t ChunkSize;                     // RIFF Chunk Size
   uint8_t WAVE[4] = { 'W', 'A', 'V', 'E' }; // WAVE Header
   /* "fmt" sub-chunk */
   uint8_t fmt[4] = { 'f', 'm', 't', ' ' }; // FMT header
   uint32_t Subchunk1Size = 16;           // Size of the fmt chunk
   uint16_t AudioFormat = 1; // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM
   // Mu-Law, 258=IBM A-Law, 259=ADPCM
   uint16_t NumOfChan = 2;   // Number of channels 1=Mono 2=Sterio
   uint32_t SamplesPerSec = 32000;   // Sampling Frequency in Hz
   uint32_t bytesPerSec = 32000 * 4; // bytes per second
   uint16_t blockAlign = 4;          // 2=16-bit mono, 4=16-bit stereo
   uint16_t bitsPerSample = 16;      // Number of bits per sample
   /* "data" sub-chunk */
   uint8_t Subchunk2ID[4] = { 'd', 'a', 't', 'a' }; // "data"  string
   uint32_t Subchunk2Size;                        // Sampled data length
} wav_hdr;

/* TODO: Refactor these essentially duplicate functions. */
void loadbin(std::string path, uint8_t* mem, uint16_t offset)
{
   std::ifstream file(path, std::ios::ate | std::ios::binary);
   if (!file.is_open()) {
      throw std::runtime_error("failed to open file: " + path);
   }
   size_t fileSize = static_cast<size_t>(file.tellg());

   file.seekg(0);
   file.read(reinterpret_cast<char*>(&mem[offset]), fileSize);
   file.close();
}

void loadfile(std::string path, std::vector<uint8_t>& vec) {
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

/* Fix seq midi header. */
void swapbytes(u8 count, u8* bytes) {
   for (u8 i = 0; i < count*4; i+=4) {
      u8 temp;

      temp = bytes[i];
      bytes[i] = bytes[i + 3];
      bytes[i + 3] = temp;

      temp = bytes[i + 1];
      bytes[i + 1] = bytes[i + 2];
      bytes[i + 2] = temp;
   }
}

#define AUDIO_HEAP_SIZE    256 * 1024
#define AUDIO_MAX_VOICES   64
#define AUDIO_MAX_UPDATES  128
#define AUDIO_MAX_CHANNELS 32
#define AUDIO_EVT_COUNT    64
#define AUDIO_FREQUENCY    32000

#define AUDIO_BUFSZ        320
#define AUDIO_CLIST_SIZE   32 * 1024

static me::MeAudio* audio; // audio playback backend
static bool initialized = false;

static ALHeap audioHp;
static ALGlobals audioGlobals;
static ALCSPlayer cseqPlayer;
static Acmd audioCmdList[AUDIO_CLIST_SIZE];

static u8* imem;
static u8* dmem;
static u8* rdram;
static s32 seqFileAddress, ctlFileAddress, tblFileAddress, commandListAddress, audioBufferAddress, audioHeapAddress;
static u32 validTracks = 0;
static u32 currentEnabledTracks = 0xFFFFFFFF;
static ALCSeq cseq = {};
static ALSeqpConfig cscfg = {};
static ALSynConfig scfg = {};
static ALCSeqMarker start = {};
static ALCSeqMarker end = {};


/* Dummy audio dma callback stubs. */
static s32 audioDmaCallback(s32 addr, s32 len, void* state) {
   return addr;
}
static ALDMAproc audioNewDma(void* arg) {
   return audioDmaCallback;
}

static s32 lastLocTicks = 0;
static u32 overflowAccumulator = 0;
void stop() {
   if (!initialized) { return; }
   alCSPStop(&cseqPlayer);
   alCSeqSetLoc(&cseq, &start);
   lastLocTicks = 0;
   overflowAccumulator = 0;
}

void play() {
   if (!initialized) { return; }
   cseq.validTracks &= currentEnabledTracks;
   alCSPPlay(&cseqPlayer);
}

void pause() {
   if (!initialized) { return; }
   alCSPStop(&cseqPlayer);
}

void setEnabledTracks(int enabledTracks) {
   currentEnabledTracks = enabledTracks;
   cseq.validTracks &= currentEnabledTracks;
}

int getValidTracks() {
   return validTracks;
}

void scrub(float position) {
   ALCSeqMarker scrubMarker;
   s32 newTick = end.lastTicks * position;
   alCSeqNewMarker(&cseq, &scrubMarker, newTick);
   alCSeqSetLoc(&cseq, &scrubMarker);
   lastLocTicks = 0;
   overflowAccumulator = 0;
}

void getloc(float& position) {
   ALCSeqMarker currMarker;
   alCSeqGetLoc(&cseq, &currMarker);

   /* We overflowed, lets do some math. */
   u32 currTicks, lastTicks;
   memcpy(&currTicks, &currMarker.lastTicks, 4);
   memcpy(&lastTicks, &lastLocTicks, 4);
   if (lastTicks > currTicks) {
      u32 overflowStep = (u64) 0x100000000 % (u32) end.lastTicks;
      overflowAccumulator = (overflowAccumulator + overflowStep) % end.lastTicks;
   }

   position = (float) ((currMarker.lastTicks + overflowAccumulator) % end.lastTicks) / (float) end.lastTicks;
   lastLocTicks = currMarker.lastTicks;
}

void startaudiothread() {
   load();
   power(false);
   audio = new me::MeAudio;
}

void resetBank(int bank) {
   ALBankFile* bankFile = (ALBankFile*)getRamObject(ctlFileAddress);
   alCSPSetBank(&cseqPlayer, bankFile->getBank(bank));
}

void init(std::string seqPath, std::string bankPath, std::string wavetablePath, int bank) {
   if (initialized) { 
      stop();
      alClose(&audioGlobals);
   }
   initialized = false;
   lastLocTicks = 0;
   overflowAccumulator = 0;

   imem = static_cast<u8*>(getimem());
   dmem = static_cast<u8*>(getdmem());
   rdram = static_cast<u8*>(getrdram());

   memset(imem,  0, 4  * 1024);        // clear imem
   memset(dmem,  0, 4  * 1024);        // clear dmem
   memset(rdram, 0, 16 * 1024 * 1024); // clear rdram

   std::vector<u8> seqFile;
   std::vector<u8> ctlFile;
   std::vector<u8> tblFile;

   loadbin("ucode/audio_data.zbin", dmem, 0); // rsp udata
   loadbin("ucode/audio.zbin", imem, 0x80); // rsp ucode
   loadfile(seqPath, seqFile); // seq (midi) file
   loadfile(bankPath, ctlFile); // soundbank file
   loadfile(wavetablePath, tblFile); // wavetable file
   swapbytes(17, seqFile.data()); // swap header for endianess compatibility

   /* Reserve and initialize these memory regions in our virtual RDRAM. */
   seqFileAddress = 0;
   ctlFileAddress = (seqFile.size() + 0xF) & (~0xF);
   tblFileAddress = ((ctlFileAddress + ctlFile.size()) + 0xF) & (~0xF);
   commandListAddress = (tblFileAddress + tblFile.size() + 0xF) & (~0xF);
   audioBufferAddress = ((commandListAddress + AUDIO_CLIST_SIZE * sizeof(Acmd)) + 0xF) & (~0xF);
   audioHeapAddress = ((audioBufferAddress + 4 * AUDIO_BUFSZ) + 0xF) & (~0xF);
   assert((AUDIO_HEAP_SIZE + audioHeapAddress) < 0x1000000); // do not exceed 16MB ram size for now.
   memcpy(rdram + seqFileAddress, seqFile.data(), seqFile.size());
   memcpy(rdram + ctlFileAddress, ctlFile.data(), ctlFile.size());
   memcpy(rdram + tblFileAddress, tblFile.data(), tblFile.size());

   /* Initialize Audio Heap */
   audioHp = {};
   audioHp.base = rdram + audioHeapAddress;
   audioHp.cur = rdram + audioHeapAddress;
   audioHp.count = 0;
   audioHp.len = AUDIO_HEAP_SIZE;
   s32 audioRate = 0x02E6D354 / (s32)(0x02E6D354 / (f32)AUDIO_FREQUENCY - .5f);
   scfg = {
       .maxVVoices = AUDIO_MAX_VOICES,
       .maxPVoices = AUDIO_MAX_VOICES,
       .maxUpdates = AUDIO_MAX_UPDATES,
       .dmaproc = (void*)audioNewDma, // explained later
       .heap = &audioHp,
       .outputRate = audioRate,
       .fxType = AL_FX_BIGROOM,
   };
   audioGlobals = {};
   alInit(&audioGlobals, &scfg);
   audioGlobals.rdram = rdram;

   /* Initialize CSeqPlayer and CSequence. */
   cscfg = {
      .maxVoices = AUDIO_MAX_VOICES,
      .maxEvents = AUDIO_EVT_COUNT,
      .maxChannels = AUDIO_MAX_CHANNELS,
      .debugFlags = 0, // disabled
      .heap = &audioHp,
      .initOsc = nullptr,
      .updateOsc = nullptr,
      .stopOsc = nullptr,
   };
   cseqPlayer = {};
   alCSPNew(&cseqPlayer, &cscfg);

   alCSeqNew(&cseq, rdram + seqFileAddress);
   validTracks = cseq.validTracks;
   alCSPSetSeq(&cseqPlayer, &cseq);

   ALBankFile* bankFile = (ALBankFile*)getRamObject(ctlFileAddress);
   alBnkfNew(ctlFileAddress, tblFileAddress);
   alCSPSetBank(&cseqPlayer, bankFile->getBank(bank));
   alCSeqGetLoc(&cseq, &start);
   alCSeqGetFinalMarker(&cseq, &end);
   initialized = true;
}

void audioFrame() {
   /* Generate and write to DRAM the audio command list */
   s32 cmdLen = 0;
   alAudioFrame(audioCmdList, &cmdLen, (s16*)audioBufferAddress, AUDIO_BUFSZ);
   memcpy(rdram + commandListAddress, audioCmdList, cmdLen * sizeof(Acmd));

   /* Write to DMEM address 0xFF0 the 32 bit address of rdram location of cmdList */
   s32 data = 0;
   data = commandListAddress;
   memcpy(dmem + 0xFF0, &data, 4);

   /* Write to 0xFF4 length of rdram cmdList (cmdLen * sizeof(Acmd)) */
   data = cmdLen * sizeof(Acmd);
   memcpy(dmem + 0xFF4, &data, 4);

   /* Loop through rsp ucode until halt is reached. */
   while (!halted()) { run(); }
   unhalt();
}

int mainloop(float volume) {
   /* Skip loop if uninitialized synth */
   if (!initialized) { return 0; }

   /* Generate required audio */
   while (audio->NeedsAudio()) {
      audioFrame();

      /* Push generated samples to stream */
      u8* output = (rdram + audioBufferAddress);
      if (volume != 1.0f) {
         for (u32 i = 0; i < AUDIO_BUFSZ * DEVICE_CHANNELS; i++) {
            ((s16*)output)[i] = (s16) ((s16*)output)[i] * volume;
         }
      }
      memcpy(audio->GetAudioBuffer(), output, AUDIO_BUFSZ * sizeof(s16) * DEVICE_CHANNELS);
      audio->PushToStream();

      /* TODO: Missing unloading rsp. */
      // unload();
   }
   return 0;
}

void record(std::string name) {
   std::ofstream binfile("temp.sw", std::ofstream::binary);

   ALCSeqMarker currMarker;
   alCSeqGetLoc(&cseq, &currMarker);
   cseq.validTracks &= currentEnabledTracks;
   alCSPPlay(&cseqPlayer);

   float p, lp = 0.0f;
   getloc(p); lp = p;
   while (p >= lp) {
      lp = p;
      audioFrame();
      u8* output = (rdram + audioBufferAddress);
      binfile.write((char*)output, AUDIO_BUFSZ * 4);
      getloc(p);
      std::cout << p << std::endl;
   }
   alCSPStop(&cseqPlayer);
   alCSeqSetLoc(&cseq, &currMarker);

   binfile.close();

   std::string filename = name + ".wav";
   std::ifstream in("temp.sw", std::ifstream::binary);
   std::ofstream out(filename.c_str(), std::ofstream::binary);

   uint32_t fsize = in.tellg();
   in.seekg(0, std::ios::end);
   fsize = (uint32_t)in.tellg() - fsize;
   in.seekg(0, std::ios::beg);

   wav_hdr wav;
   wav.ChunkSize = fsize + sizeof(wav_hdr) - 8;
   wav.Subchunk2Size = fsize + sizeof(wav_hdr) - 44;
   out.write(reinterpret_cast<const char*>(&wav), sizeof(wav));

   int16_t d;
   for (int i = 0; i < fsize; ++i) {
      // TODO: read/write in blocks
      in.read(reinterpret_cast<char*>(&d), sizeof(int16_t));
      out.write(reinterpret_cast<char*>(&d), sizeof(int16_t));
   }

   in.close();
   out.close();
}
