#pragma once

#include "element.hpp"
#include <stock.hpp>

namespace aux_lfo {

// Audio role. Both consumers use the existing AUX depth and destination route.
inline opxy::i32 gyro_q31_from_stock() {
    return gyro_q31(opxy::platform::stock<opxy::i16()>(0x8d4db5bc)());
}

opxy::i32 mic_q31_from_stock();

} // namespace aux_lfo

extern "C" void aux_lfo_observe_mic(opxy::u32 source, const opxy::i32* pairs)
    __asm__("aux_lfo_observe_mic");

extern "C" opxy::u32 aux_lfo_mic_input_lane() __asm__("aux_lfo_mic_input_lane");
