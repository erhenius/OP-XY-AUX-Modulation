#pragma once
#include <opxy/q31.hpp>

namespace aux_lfo {
using opxy::i32;

struct DuckControls {
    i32 source, amount, hold, release, source_type; // stock non-negative Q31 words
};

struct DuckState {
    // Native accesses use offsets relative to the start of this buffer.
    // 0x80..0x87 hold two histories; 0x88 holds the first of 64 per-sample results.
    i32 kernel[0x88 / sizeof(i32) + 64];
    i32 words[29]; // Stock parameter block through source-type word 28.
    bool initialized;
};

extern "C" void sdk_aux_duck_kernel(i32* state, unsigned private_index,
                                    const i32* controls) __asm__("sdk_aux_duck_kernel");

inline void duck_reset(DuckState& s) {
    // Clear on *entry*, once. A BSS-zeroed instance has the same behaviour.
    s.initialized = false;
}

inline i32 duck_step(DuckState& s, const DuckControls& controls) {
    const i32 amount = controls.amount < 0 ? 0 : controls.amount;
    const i32 hold = controls.hold < 0 ? 0 : controls.hold;
    const i32 release = controls.release < 0 ? 0 : controls.release;
    const bool new_coefficients = !s.initialized || s.words[25] != amount ||
                                 s.words[26] != hold || s.words[27] != release;
    if (!s.initialized) {
        for (auto& value : s.kernel) value = 0;
        s.kernel[0x78 / 4] = 1; // Stock refreshes the stages from the pristine histories.
        s.initialized = true;
    }
    s.words[24] = controls.source < 0 ? 0 : controls.source;
    s.words[25] = amount;
    s.words[26] = hold;
    s.words[27] = release;
    s.words[28] = controls.source_type < 0 ? 0 : controls.source_type;
    // Stock's compare uses the hold word, but its compute branch stores amount.
    // Recompute on edits to any coefficient input and reuse them while steady.
    s.kernel[0x70 / 4] = new_coefficients ? -1 : hold;
    s.kernel[0x74 / 4] = release;
    sdk_aux_duck_kernel(s.kernel, 0, s.words);
    // Instrument volume's stock destination consumes the first sample of the output block.
    // A positive result cannot duck. Bound unexpected input to unity (no change).
    const i32 raw = s.kernel[0x88 / 4];
    return raw < 0 ? opxy::q31::multiply(raw, amount) : 0;
}

inline i32 duck_delta(i32 base, i32 depth_scaled_envelope) {
    // The AUX destination takes an additive delta; the equivalent of ducking
    // a normalized control by a fractional amount is base * (-attenuation).
    return base > 0 && depth_scaled_envelope < 0
        ? opxy::q31::multiply(base, depth_scaled_envelope) : 0;
}
}
