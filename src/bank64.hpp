#ifndef BANK64_HPP
#define BANK64_HPP

#include <vector>
#include <string>

typedef struct {
   int keyMin;
   int keyMax;
   int velocityMin;
   int velocityMax;
   int keyBase;
   int detune;
} KeyMap64;

typedef struct {
   int attackTime;
   int decayTime;
   int releaseTime;
   int attackVol;
   int decayVol;
} Envelope64;

typedef struct {
   int order;
   int predictors;
   void* pages;
} Book64;

typedef struct {
   int start;
   int end;
   int count;
   void* state;
   bool valid;
} Loop64;

typedef struct {
   int    raw;
   int    len;
   Book64 book;
   Loop64 loop;
   unsigned int id;
} Wave64;

typedef struct {
   KeyMap64 keymap;
   Envelope64 envelope;
   Wave64 wave;
   int samplePan;
   int sampleVolume;
} Sound64;

typedef struct {
   int volume;         /* overall volume for this instrument   */
   int pan;            /* 0 = hard left, 127 = hard right      */
   int priority;       /* voice priority for this instrument   */
   int tremType;       /* the type of tremelo osc. to use      */
   int tremRate;       /* the rate of the tremelo osc.         */
   int tremDepth;      /* the depth of the tremelo osc         */
   int tremDelay;      /* the delay for the tremelo osc        */
   int vibType;        /* the type of tremelo osc. to use      */
   int vibRate;        /* the rate of the tremelo osc.         */
   int vibDepth;       /* the depth of the tremelo osc         */
   int vibDelay;       /* the delay for the tremelo osc        */
   int bendRange;      /* pitch bend range in cents            */
   std::vector<Sound64> sounds;
} Inst64;

typedef struct {
   int pad;
   int sampleRate;     /* e.g. 44100, 22050, etc...       */
   Inst64* percussion;
   std::vector<Inst64> instruments;
   int tblFileAddress;
} Bank64;

typedef struct {
   std::vector<unsigned char> dataChunk;
   int dataStart;
} BankChunk;

typedef struct {
   std::vector<BankChunk> bankDataChunks;
} BankFile64;

void SaveBank(BankChunk* bankChunk, Bank64* bank);
void SaveBankFile(std::string filename, BankFile64* bankfile);

#endif