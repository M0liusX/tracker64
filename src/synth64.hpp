/*    This is an interface for a N64 libaudio synth. Init initialized memory 
      while mainloop should be called every audio frame to generate samples.

      The interactable actions allow implementation of basic playback control schemes.
 */
#ifndef SYNTH64_HPP
#define SYNTH64_HPP

#include <string>

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

/* State change actions! */
void setEnabledTracks(int enabledTracks);
int getValidTracks();
void resetBank(int bank);
#endif
