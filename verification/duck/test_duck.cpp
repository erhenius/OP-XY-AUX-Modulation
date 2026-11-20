#include "aux_lfo_duck.hpp"
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstring>

using namespace aux_lfo;
namespace {
const i32 one = INT_MAX;
i32 next_wave;
bool fresh, coefficients;
unsigned calls;
DuckControls expected;
}

// Test double only. The coefficient and envelope calculations live in the OS kernel.
extern "C" void sdk_aux_duck_kernel(i32* k, unsigned idx, const i32* w) {
    assert(idx == 0 && w[24] == expected.source && w[25] == expected.amount);
    assert(w[26] == expected.hold && w[27] == expected.release && w[28] == expected.source_type);
    assert(k[0x70 / 4] == (coefficients ? -1 : expected.hold));
    assert(k[0x74 / 4] == expected.release);
    assert((k[0x78 / 4] != 0) == fresh);
    if (fresh) {
        assert(k[0x80 / 4] == 0 && k[0x84 / 4] == 0);
        k[0x78 / 4] = 0;
    }
    ++k[0x80 / 4];
    ++k[0x84 / 4];
    for (int frame = 0; frame < 64; ++frame)
        k[0x88 / 4 + frame] = frame ? -123 : next_wave;
    k[0x70 / 4] = w[25]; // Model the stock cache-store quirk.
    ++calls;
}
int main() {
    static_assert(sizeof(i32) == 4, "requires four-byte words");
    static_assert(sizeof(DuckState) == 512, "per-target static budget");
    static_assert(offsetof(DuckState, words) == 0x88 + 64 * 4, "all samples must fit");
    DuckState a{}, b{}, c{};
    fresh = coefficients = true;
    expected = DuckControls{17, 1 << 30, 1000, 500, 0};
    next_wave = -1; // A negative fractional sample.
    assert(duck_step(a, expected) == -1);
    const DuckState after_first = a;
    next_wave = INT_MIN;
    assert(duck_step(b, expected) == -(1 << 30));
    assert(!std::memcmp(&a, &after_first, sizeof(a)));
    assert(duck_delta(1 << 30, -(1 << 30)) == -(1 << 29));
    assert(duck_delta(0, INT_MIN) == 0);
    assert(duck_delta(one, 0) == 0);
    assert(duck_delta(one, one) == 0);
    assert(duck_delta(one, INT_MIN) == -one);
    assert(duck_step(c, expected) == -(1 << 30));

    fresh = coefficients = false;
    expected.source_type = one;
    expected.source = 201;
    next_wave = -32768;
    assert(duck_step(a, expected) == -16384);
    assert(a.kernel[0x80/4] == 2);
    assert(b.kernel[0x80/4] == 1 && c.kernel[0x80/4] == 1);
    coefficients = true;
    ++expected.amount;
    (void)duck_step(a, expected);
    ++expected.hold;
    (void)duck_step(a, expected);
    ++expected.release;
    (void)duck_step(a, expected);
    coefficients = false;
    next_wave = 7200;
    assert(duck_step(a, expected) == 0);
    next_wave = INT_MIN;
    expected.amount = 0; coefficients = true;
    assert(duck_step(a, expected) == 0);
    duck_reset(a);
    fresh = true;
    expected = DuckControls{0, 0, 0, 0, 0};
    assert(duck_step(a, DuckControls{-1, -10, -500, -88, -1}) == 0);
    assert(a.kernel[0x80/4] == 1 && calls == 10);
}
