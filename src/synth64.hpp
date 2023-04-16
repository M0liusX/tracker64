#ifndef SYNTH64_HPP
#define SYNTH64_HPP

#include <string>

void startaudiothread();
void init(std::string seqPath, int bank);
int mainloop();


void play();
void pause();
void stop();
#endif
