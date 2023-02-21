#ifndef RSP_INTERFACE_HPP
#define RSP_INTERFACE_HPP

auto load() -> void;
auto unload() -> void;
auto power(bool reset) -> void;
auto run() -> void;

auto getimem() -> void*;
auto getdmem() -> void*;

#endif