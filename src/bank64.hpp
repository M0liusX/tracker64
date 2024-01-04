#ifndef BANK64_HPP
#define BANK64_HPP

#include <vector>

typedef struct {
   int keyMin;
   int keyMax;
   int velocityMin;
   int velocityMax;
} KeyMap64;

typedef struct {
   KeyMap64 keymap;
} Sound64;

typedef struct {
   std::vector<Sound64> sounds;
} Inst64;

typedef struct {
   std::vector<Inst64> instruments;
} Bank64;

#endif