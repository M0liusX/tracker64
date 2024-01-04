#ifndef VIRTUALKEYBOARD_HPP
#define VIRTUALKEYBOARD_HPP

#include "..\bank64.hpp"

void RenderVirtualKeyboard(Bank64* bank);

bool GetInstrumentChanged();
int GetInstrument();
bool GetKeyHit(int key);
bool GetKeyReleased(int key);
int GetVolume();

int IsMapped(Bank64*, int key);

#endif