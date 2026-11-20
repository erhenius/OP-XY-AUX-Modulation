#pragma once

#include <opxy/q31.hpp>

namespace aux_lfo {

struct ValueRandomState {
    opxy::u32 phase;
    opxy::u32 sequence;
    opxy::i32 held;
    opxy::i32 smoothed;
};

inline void reset_value_random(ValueRandomState &state, opxy::u32 seed) {
    state.phase = 0;
    state.sequence = seed;
    state.held = 0;
    state.smoothed = 0;
}

// OS 1.2.1 instrument Value and Random interpret their rate word identically.
inline opxy::u32 value_random_increment(opxy::i32 rate_word, opxy::u32 stock_tempo_increment) {
    const opxy::u32 rate = rate_word > 0 ? static_cast<opxy::u32>(rate_word) >> 17 : 0;
    if (rate < 8192u) return stock_tempo_increment >> (rate >> 10);
    const opxy::i32 distance = static_cast<opxy::i32>(rate - 8192u) * 65536;
    return static_cast<opxy::u32>(opxy::q31::multiply(distance, distance));
}

// One control sample at the start of the stock 64-sample Value block.
// The instrument divides its phase step by 64 before advancing waveform samples.
inline opxy::i32 value_wave(ValueRandomState &state, opxy::u32 increment, opxy::u32 shape,
                            const volatile opxy::i16 *sine_table) {
    const opxy::u32 phase = state.phase;
    state.phase += increment & ~63u;
    switch (shape) {
        case 0: return static_cast<opxy::i32>(sine_table[phase >> 19]) * 65536;
        case 1: return (phase & 0x80000000u) ? (-2147483647 - 1) : 2147418112;
        case 2: return static_cast<opxy::i32>((65535u - (phase >> 16)) >> 1) * 65536;
        case 3: return static_cast<opxy::i32>((phase >> 16) >> 1) * 65536;
        default: return 0;
    }
}

inline opxy::i32 random_sample_blend(opxy::u32 increment) {
    const opxy::i32 base = 16777216;
    const opxy::u32 threshold = 21474836;
    if (increment <= threshold) return base;
    // The native evaluator supports signed positive increments. A caller may use
    // the entire unsigned range to fast-forward the clock; clamp for the blend.
    const opxy::i32 limited = increment > 2147483647u ? 2147483647 : static_cast<opxy::i32>(increment);
    const opxy::i32 distance = limited - static_cast<opxy::i32>(threshold);
    return base + opxy::q31::multiply(opxy::q31::shift_left(distance, 5), 268435456);
}

inline opxy::i32 random_wave(ValueRandomState &state, opxy::u32 increment) {
    const opxy::u32 previous = state.phase;
    state.phase += increment;
    if (state.phase < previous) {
        state.sequence = state.sequence * 1664525u + 1013904223u;
        const opxy::i32 high = static_cast<opxy::i32>(state.sequence >> 16);
        state.held = (high >= 32768 ? high - 65536 : high) * 65536;
    }
    if (state.smoothed == state.held) return state.smoothed;

    // Advance the one-pole through 64 native samples with six Q31 squares.
    // This is an end-of-block control sample. Intermediate audio samples are
    // unnecessary for AUX modulation; fixed-point rounding differs by a few LSBs.
    opxy::i32 old_weight = opxy::q31::maximum - random_sample_blend(increment);
    for (unsigned power = 0; power < 6; ++power) {
        old_weight = opxy::q31::multiply(old_weight, old_weight);
    }
    state.smoothed = opxy::q31::add(opxy::q31::multiply(old_weight, state.smoothed),
                                    opxy::q31::multiply(opxy::q31::maximum - old_weight, state.held));
    return state.smoothed;
}

}  // namespace aux_lfo
