#pragma once

#include "aux_value_random.hpp"

namespace aux_lfo {

// Audio CPU, OS 1.2.1 only. Callers must authenticate the OS profile before
// installing the binding. These aliases use read-only native data.
inline opxy::u32 value_random_stock_increment(opxy::i32 rate_word) {
    const volatile opxy::u32 *tempo = reinterpret_cast<const volatile opxy::u32 *>(0x840426f4u);
    return value_random_increment(rate_word, *tempo);
}

inline opxy::i32 value_wave_stock(ValueRandomState &state, opxy::i32 rate_word, opxy::u32 shape) {
    const volatile opxy::i16 *sine = reinterpret_cast<const volatile opxy::i16 *>(0x84048aa4u);
    return value_wave(state, value_random_stock_increment(rate_word), shape, sine);
}

inline opxy::i32 random_wave_stock(ValueRandomState &state, opxy::i32 rate_word) {
    return random_wave(state, value_random_stock_increment(rate_word));
}

}  // namespace aux_lfo
