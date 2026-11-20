#pragma once

#include <opxy/q31.hpp>

namespace aux_lfo {

inline opxy::i32 gyro_q31(opxy::i16 motion) {
    const opxy::i32 reversed = -static_cast<opxy::i32>(motion);
    return (reversed > 32767 ? 32767 : reversed) * 65536;
}

// Read only on the audio role. The stock DMA service records this cycle pair
// before each input callback, including a callback with no input buffer.
struct CaptureStamp {
    opxy::u32 low;
    opxy::u32 high;
};

struct ElementMic {
    opxy::i32 first;
    opxy::i32 second;
    CaptureStamp captured;
    bool active;
};

// Stock Element computes two cascaded Q31 one-pole followers. The fixed
// coefficients below are derived from the stock 10/200 ms defaults at 44.1 kHz
// (1% remaining after the nominal time). No floating point in audio.
inline opxy::i32 mic_pole(opxy::i32 old, opxy::i32 target) {
    const opxy::i32 coefficient = old < target ? 2125175096 : 2146362679;
    // Both operands lie in [0, INT_MAX], so neither the difference nor
    // target + the weighted difference can overflow.
    return target + opxy::q31::multiply(old - target, coefficient);
}

inline void mic_observe(ElementMic& state, const opxy::i32* pairs,
                        bool internal_mic, CaptureStamp stamp) {
    state.active = internal_mic && pairs;
    state.captured = stamp;
    if (!internal_mic || !pairs) {
        state.first = state.second = 0;
        return;
    }
    for (unsigned i = 0; i < 64; ++i) {
        // Same ordering as instrument Element: abs(max(a,b)).
        const opxy::i32 a = *pairs++;
        const opxy::i32 b = *pairs++;
        const opxy::i32 max = a > b ? a : b;
        const opxy::i32 magnitude = max >= 0 ? max :
            (max == -2147483647 - 1 ? 2147483647 : -max);
        state.first = mic_pole(state.first, magnitude);
        state.second = mic_pole(state.second, state.first);
    }
}

inline opxy::i32 mic_read(ElementMic& state, CaptureStamp current) {
    const bool fresh = state.active && current.low == state.captured.low &&
                       current.high == state.captured.high;
    if (!fresh) state.first = state.second = 0;
    return state.second;
}

} // namespace aux_lfo
