/*    This is an interface for a N64 libaudio synth. Init initialized memory 
      while mainloop should be called every audio frame to generate samples.

      The interactable actions allow implementation of basic playback control schemes.
 */
#ifndef SYNTH64_HPP
#define SYNTH64_HPP

#include <string>
#include "bank64.hpp"

typedef struct {
   int type;
   int key;
   int velocity;
} Midi64Event;

//typedef struct {
//   int instrumentCount
//} Bank64;

/* TODO: Seperate audio callback mechanism from synth. */
void startaudiothread();

void init(std::string seqPath, std::string bankPath, std::string wavetablePath, int bank);
int mainloop(float volume = 1.0f);

/* Interactable actions. */
void play();
void pause();
void stop();
void record(std::string name);
void scrub(float pos);
void getloc(float& pos); // TODO: Maybe let application keep track of position? Non-trivial with a blackbox synth.
void sendevent(Midi64Event e);

/* State change actions! */
void setEnabledTracks(int enabledTracks);
int getValidTracks();
void resetBank(int bank);

void getBankData(int bankNum, Bank64* bank64);
int getBankCount();
#endif
