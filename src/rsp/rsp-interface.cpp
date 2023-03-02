#include "rsp-interface.hpp"
#include <iostream>
#include <cassert>
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

    /* TODO: Investigate need for this? */
    queue.step(256, [](u32 event) {
       switch (event) {
       case Queue::RSP_DMA:       return rsp.dmaTransferStep();
       default: assert(false);    return;
       }
    });
}

auto halted() -> bool {
   return rsp.status.halted == 1;
}

auto reset() -> void {

}

auto getimem() -> void* {
   return rsp.getimem();
}

auto getdmem() -> void* {
   return rsp.getdmem();
}

auto getrdram() -> void* {
   return rdram.ram.data;
}

auto unhalt() -> void {
   rsp.unhalt();
}