#ifndef PIANOROLL_HPP
#define PIANOROLL_HPP

#include "..\seqparse.hpp"
#include "imgui.h"

void RenderPianoRoll(Midi64& midi, ImGuiIO& io);
void SetCurrentTrack(int newTrackID);
#endif
