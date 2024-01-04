#ifndef VIRTUALKEYBOARD_HPP
#define VIRTUALKEYBOARD_HPP

void RenderVirtualKeyboard();

bool GetInstrumentChanged();
int GetInstrument();
bool GetKeyHit(int key);
bool GetKeyReleased(int key);

#endif