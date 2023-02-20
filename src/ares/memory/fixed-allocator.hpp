#ifndef FIXED_ALLOCTOR_HPP
#define FIXED_ALLOCTOR_HPP

#include <nall/stdint.hpp>
#include <nall/string.hpp>
#include <nall/float-env.hpp>
#include <nall/hashset.hpp>
#include <nall/bump-allocator.hpp>
#include <nall/recompiler/generic/generic.hpp>
#include <nall/serializer.hpp>
#include <nall/literals.hpp>
#include <nall/types.hpp>
#include <nall/priority-queue.hpp>

using namespace nall;
using namespace nall::primitives;

namespace ares::Memory {
   struct FixedAllocator {
   static auto get() -> bump_allocator&;

   private:
   FixedAllocator();

   bump_allocator _allocator;
   };
}

#endif
