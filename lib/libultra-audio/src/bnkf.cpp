/*====================================================================
 * bnkf.c
 *
 * Copyright 1993, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics,
 * Inc.; the contents of this file may not be disclosed to third
 * parties, copied or duplicated in any form, in whole or in part,
 * without the prior written permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to
 * restrictions as set forth in subdivision (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS
 * 252.227-7013, and/or in similar or successor clauses in the FAR,
 * DOD or NASA FAR Supplement. Unpublished - rights reserved under the
 * Copyright Laws of the United States.
 *====================================================================*/

#include <libaudio.h>
#include <cassert>
//#include <os_internal.h>
//#include <ultraerror.h>

/*
 * ### when the file format settles down a little, I'll remove these
 * ### for efficiency.
 */
static  void _bnkfPatchBank(ALBank *bank, s32 offset, s32 table);
static  void _bnkfPatchInst(ALInstrument *i, s32 offset, s32 table);
static  void _bnkfPatchSound(ALSound *s, s32 offset, s32 table);
static  void _bnkfPatchWaveTable(ALWaveTable *w, s32 offset, s32 table);


void swap32(u8* bytes)
{
      u8 temp = bytes[0];
      bytes[0] = bytes[3];
      bytes[3] = temp;

      temp = bytes[1];
      bytes[1] = bytes[2];
      bytes[2] = temp;
}

void swap16(u8* bytes)
{
   u8 temp = bytes[0];
   bytes[0] = bytes[1];
   bytes[1] = temp;
}

void alSeqFileNew(ALSeqFile *file, u8 *base)
{
    s32 offset = (s32) base;
    s32 i;
    
    /*
     * patch the file so that offsets are pointers
     */
    for (i = 0; i < file->seqCount; i++) {
        file->seqArray[i].offset = (u8 *)((u8 *)file->seqArray[i].offset + offset);
    }
}

void alBnkfNew(s32 ctlAddress, s32 tblAddress)
{   
    s32 i;
    
    /*
     * check the file format revision in debug libraries
     */
    //ALFailIf(file->revision != AL_BANK_VERSION, ERR_ALBNKFNEW);
    
    /*
     * patch the file so that offsets are pointers
     */
    ALBankFile* file = (ALBankFile*) getRamObject(ctlAddress);
    swap16((u8*) &file->revision);
    swap16((u8*) &file->bankCount);
    for (i = 0; i < file->bankCount; i++) {
        //file->bankArray[i] = (ALBank *)((u8 *)file->bankArray[i] + offset);
       s32* offset = file->bankAddress(i);
       swap32((u8*) offset);
       if (*offset) {
          *offset += ctlAddress;
          _bnkfPatchBank(file->getBank(i), ctlAddress, tblAddress);
       }
    }
}

void _bnkfPatchBank(ALBank *bank, s32 offset, s32 table) 
{
    s32 i;
    
    if (bank->flags)
        return;

    bank->flags = 1;
    swap16((u8*) &bank->instCount);
    swap32((u8*) &bank->sampleRate);
    s32* percussionOffset = bank->percussionAddress();
    if (*percussionOffset) {
        swap32((u8*)percussionOffset);
        *percussionOffset += offset;
        _bnkfPatchInst(bank->getPercussion(), offset, table);
    }
    
    for (i = 0; i < bank->instCount; i++) {
        s32* instOffset = bank->instrumentAddress(i);
        if(*instOffset)
            swap32((u8*)instOffset);
            *instOffset += offset;
            _bnkfPatchInst(bank->getInstrument(i), offset, table);
    }
}

void _bnkfPatchInst(ALInstrument *inst, s32 offset, s32 table)
{
    s32 i;

    if (inst->flags)
        return;

    inst->flags = 1;
    
    swap16((u8*) &inst->bendRange);
    swap16((u8*) &inst->soundCount);
    for (i = 0; i < inst->soundCount; i++) {
        s32* soundOffset = inst->soundAddress(i);
        swap32((u8*)soundOffset);
        *soundOffset += offset;
        _bnkfPatchSound(inst->getSound(i), offset, table);

    }
}

void _bnkfPatchSound(ALSound *s, s32 offset, s32 table)
{
    if (s->flags)
        return;

    s->flags = 1;
    
    s32 *envelopeOffset, *keyMapOffset, *wavetableOffset;
    envelopeOffset = s->envelopeAddress();
    keyMapOffset = s->keymapAddress();
    wavetableOffset = s->wavetableAddress();
    swap32((u8*)envelopeOffset);
    swap32((u8*)keyMapOffset);
    swap32((u8*)wavetableOffset);
    *envelopeOffset += offset;
    *keyMapOffset += offset;
    *wavetableOffset += offset;

    ALEnvelope* envelope = s->getEnvelope();
    swap32((u8*) &envelope->attackTime);
    swap32((u8*) &envelope->decayTime);
    swap32((u8*) &envelope->releaseTime);

    _bnkfPatchWaveTable(s->getWaveTable(), offset, table);
}

void _bnkfPatchWaveTable(ALWaveTable *w, s32 offset, s32 table)
{
    if (w->flags)
        return;

    w->flags = 1;
    
    swap32((u8*) &w->base);
    w->base += table;

    /* sct 2/14/96 - patch wavetable loop info based on type. */
    if (w->type == AL_ADPCM_WAVE)
    {
       s32 *bookOffset = w->waveInfo.adpcmWave.bookAddress();
       s32 *loopOffset = w->waveInfo.adpcmWave.loopAddress();
       swap32((u8*)bookOffset);
       swap32((u8*)loopOffset);
       *bookOffset += offset;

       ALADPCMBook* book = w->waveInfo.adpcmWave.getBook();
       swap32((u8*)&book->order);
       swap32((u8*)&book->npredictors);
       for (u32 i = 0; i < book->order * book->npredictors * 8; i++) {
          swap16((u8*)&book->book[i]);
       }
       if (*loopOffset) {
          *loopOffset += offset;
          ALADPCMloop* loop = w->waveInfo.adpcmWave.getLoop();
          swap32((u8*)&loop->start);
          swap32((u8*)&loop->end);
          swap32((u8*)&loop->count);
          for (u32 i = 0; i < 16; i++) {
             swap16((u8*)&loop->state[i]);
          }
       }
 
    }
    else if (w->type == AL_RAW16_WAVE)
    {
       assert(false);
    }
}
