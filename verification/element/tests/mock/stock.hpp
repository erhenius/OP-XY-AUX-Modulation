#pragma once
#include <opxy/types.hpp>
namespace opxy { namespace platform {
u32 word(u32 address);
template<class T> T* stock(u32 address);
template<> i16 (*stock<i16()>(u32))();
template<> u32 (*stock<u32()>(u32))();
template<> void (*stock<void(u32, const i32*)>(u32))(u32, const i32*);
}}
