#include "element_stock.hpp"

namespace {

aux_lfo::ElementMic mic;
bool using_internal_lane;

bool mic_selected(unsigned track) {
    const opxy::u32 address = 0x84eab39cu + 4u * (58u * track + 57u);
    return (opxy::platform::word(address) & 0xfff0000fu) == 0x4af00002u;
}

aux_lfo::CaptureStamp capture_stamp() {
    // 11a02a04 / 11a02a06 write these two words before calling 8d4ff32c.
    // Producer and consumer run serially on the audio role.
    return {opxy::platform::word(0x08034f68), opxy::platform::word(0x08034f6c)};
}

} // namespace

opxy::i32 aux_lfo::mic_q31_from_stock() {
    return mic_read(mic, capture_stamp());
}

opxy::u32 aux_lfo_mic_input_lane() {
    const opxy::u32 original = opxy::platform::stock<opxy::u32()>(0x8d4db364)();
    using_internal_lane = original == 0;
    return original;
}

void aux_lfo_observe_mic(opxy::u32 source, const opxy::i32* pairs) {
    const bool internal = using_internal_lane && source == 14 && pairs;
    using_internal_lane = false;
    if (mic_selected(11) || mic_selected(14) || mic_selected(15)) {
        aux_lfo::mic_observe(mic, pairs, internal, capture_stamp());
    } else if (mic.active) {
        mic = {};
    }
    opxy::platform::stock<void(opxy::u32, const opxy::i32*)>(0x8d51a3c8)(source, pairs);
}
