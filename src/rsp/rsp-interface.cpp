#include "rsp-interface.hpp"
#include <iostream>
using namespace ares::Nintendo64;

auto load() -> void {
    rdram.load();
    rsp.load();
}

auto unload() -> void {
    rsp.unload();
    rdram.unload();
}

auto power(bool reset) -> void {
    rdram.power(reset);
    rsp.power(reset);
}

auto run() -> void {
    rsp.clock = -256;
    while(rsp.clock < 0) rsp.main();
}

auto getimem() -> void* {
   return rsp.getimem();
}

auto getdmem() -> void* {
   return rsp.getdmem();
}