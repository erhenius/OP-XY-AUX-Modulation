#include "element_stock.hpp"
#include <cassert>
#include <climits>
#include <cstdio>
#include <initializer_list>

using namespace opxy;
namespace {
u32 params[16];
u32 tick = 1, tick_high = 1, headset = 0;
u32 route_count, headset_count;
u32 last_source;
const i32* last_buffer;

void original(u32 source, const i32* buffer) {
    ++route_count;
    last_source = source;
    last_buffer = buffer;
}
u32 read_headset() { ++headset_count; return headset; }
i16 read_motion() { return -123; }
}

namespace opxy { namespace platform {
u32 word(u32 adr) {
    if (adr == 0x08034f68) return tick;
    if (adr == 0x08034f6c) return tick_high;
    for (unsigned track : {11u, 14u, 15u})
        if (adr == 0x84eab39cu + (58 * track + 57) * 4) return params[track];
    assert(false); return 0;
}
template<> i16 (*stock<i16()>(u32 addr))() {
    assert(addr == 0x8d4db5bc); return read_motion;
}
template<> u32 (*stock<u32()>(u32 addr))() {
    assert(addr == 0x8d4db364); return read_headset;
}
template<> void (*stock<void(u32, const i32*)>(u32 addr))(u32, const i32*) {
    assert(addr == 0x8d51a3c8); return original;
}
}}

int main() {
    constexpr u32 mode = 0x4af00002;
    for (i32 x = -32768; x <= 32767; ++x)
        assert(aux_lfo::gyro_q31(static_cast<i16>(x)) ==
               (x == -32768 ? 32767 : -x) * 65536);
    assert(aux_lfo::gyro_q31_from_stock() == 123 * 65536);

    i32 samples[128];
    for (i32& sample : samples) sample = INT_MAX;
    auto observe = [&](u32 source, const i32* ptr) {
        const u32 count = route_count;
        assert(aux_lfo_mic_input_lane() == headset);
        aux_lfo_observe_mic(source, ptr);
        assert(route_count == count + 1 && last_source == source && last_buffer == ptr);
    };
    assert(aux_lfo::mic_q31_from_stock() == 0);
    for (unsigned track : {11u, 14u, 15u}) {
        params[track] = mode - 1;
        const u32 reads = headset_count;
        observe(14, samples);
        assert(!aux_lfo::mic_q31_from_stock() && headset_count == reads + 1);
        params[track] = mode | (17u << 4);
        observe(14, samples);
        const i32 high = aux_lfo::mic_q31_from_stock();
        assert(high > 0 && high < INT_MAX && aux_lfo::mic_q31_from_stock() == high);
        observe(14, samples);
        assert(aux_lfo::mic_q31_from_stock() > high);
        // Stock advances its stamp even when no input buffer was supplied.
        ++tick_high;
        assert(!aux_lfo::mic_q31_from_stock());
        observe(14, samples);
        ++tick;
        assert(!aux_lfo::mic_q31_from_stock());
        observe(14, samples);
        headset = 1;
        observe(14, samples);
        assert(!aux_lfo::mic_q31_from_stock());
        headset = 0;
        observe(15, samples);
        assert(!aux_lfo::mic_q31_from_stock());
        observe(14, nullptr);
        assert(!aux_lfo::mic_q31_from_stock());
        observe(14, samples);
        params[track] = 0;
        observe(14, samples);
        assert(!aux_lfo::mic_q31_from_stock());
    }
    // A hotplug after the selector call must never relabel this buffer.
    params[11] = mode; headset = 7;
    assert(aux_lfo_mic_input_lane() == 7);
    headset = 0;
    aux_lfo_observe_mic(14, samples);
    assert(!aux_lfo::mic_q31_from_stock());
    // An observer fired without a matching lane event must also fail closed.
    aux_lfo_observe_mic(14, samples);
    assert(!aux_lfo::mic_q31_from_stock());
    params[11] = 0;
    // Endpoints, both channels, and the final pair. Saturated ABS(INT_MIN).
    params[14] = params[15] = mode;
    for (i32& sample : samples) sample = 0;
    samples[126] = INT_MIN; samples[127] = INT_MIN;
    observe(14, samples);
    assert(aux_lfo::mic_q31_from_stock() > 0);
    params[14] = params[15] = 0; observe(14, samples);
    params[11] = mode; samples[127] = 0;
    observe(14, samples);
    assert(!aux_lfo::mic_q31_from_stock());
    // A full-level sustained stimulus rises; prolonged silence releases to zero.
    for (i32& sample : samples) sample = INT_MAX;
    i32 last = 0;
    for (int i = 0; i < 200; ++i) {
        observe(14, samples);
        const i32 level = aux_lfo::mic_q31_from_stock();
        assert(level >= last); last = level;
    }
    for (i32& sample : samples) sample = 0;
    for (int i = 0; i < 2100; ++i) {
        observe(14, samples);
        const i32 level = aux_lfo::mic_q31_from_stock();
        assert(level >= 0 && level <= last); last = level;
    }
    assert(last == 0);
    std::puts("PASS gyro domain, mic capture identity/freshness, all targets, envelope and original routing");
}
