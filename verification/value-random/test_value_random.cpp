#include "aux_value_random_stock.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace aux_lfo;
using opxy::i32;
using opxy::u32;
static_assert(sizeof(ValueRandomState) == 16, "Expected four words per destination");

void check_rate() {
    const u32 tempo = 0x06543211u;
    for (unsigned raw = 0; raw < 16384; ++raw) {
        const i32 low = static_cast<i32>(raw * 131072u);
        const i32 high = low + 131071;
        const u32 distance = raw - 8192u;
        const u32 expected = raw < 8192 ? tempo >> (raw / 1024) : 2u * distance * distance;
        assert(value_random_increment(low, tempo) == expected);
        assert(value_random_increment(high, tempo) == expected);
    }
    assert(value_random_increment(-1, tempo) == tempo);
    assert(value_random_increment(0, 0) == 0);
}

void check_waves(bool consume_stock_table) {
    opxy::i16 table[8192];
    for (unsigned i = 0; i < 8192; ++i) {
        const long expected = std::lround(32767 * std::sin(i * std::acos(-1.0) / 4096));
        if (consume_stock_table) {
            const int low = std::getchar();
            const int high = std::getchar();
            assert(low >= 0 && high >= 0);
            const int ordinal = high * 256 + low;
            const int sample = ordinal >= 32768 ? ordinal - 65536 : ordinal;
            assert(sample == expected);
            table[i] = static_cast<opxy::i16>(sample);
        } else {
            table[i] = static_cast<opxy::i16>(expected);
        }
        ValueRandomState state = {i << 19, 1, 0, 0};
        assert(value_wave(state, 959, 0, table) == expected * 65536);
        assert(state.phase == (i << 19) + 896);
    }
    if (consume_stock_table) assert(std::getchar() == EOF);
    for (u32 hi = 0; hi < 65536; ++hi) {
        ValueRandomState square = {(hi << 16) + 65535, 1, 0, 0};
        ValueRandomState down = square, up = square;
        assert(value_wave(square, 64, 1, table) == (hi < 32768 ? 2147418112 : -2147483647 - 1));
        const i32 rising = static_cast<i32>(hi / 2) * 65536;
        assert(value_wave(down, 64, 2, table) == 2147418112 - rising);
        assert(value_wave(up, 64, 3, table) == rising);
    }
    ValueRandomState state;
    reset_value_random(state, 42);
    const i32 quarters[] = {0, 2147418112, 0, -2147418112};
    for (unsigned i = 0; i < 8; ++i) {
        assert(value_wave(state, 0x40000000u, 0, table) == quarters[i % 4]);
    }
}

i32 expected_sample(i32 prev, i32 held, i32 alpha) {
    const auto mul = [](i32 a, i32 b) -> long long {
        const long long prod = static_cast<long long>(a) * b;
        return prod >= 0 ? prod / 2147483648LL : -((-prod + 2147483647LL) / 2147483648LL);
    };
    const long long sample = mul(held, alpha) + mul(prev, 2147483647 - alpha);
    if (sample < -2147483648LL) return -2147483647 - 1;
    return sample > 2147483647LL ? 2147483647 : static_cast<i32>(sample);
}

void check_random() {
    ValueRandomState state;
    reset_value_random(state, 1);
    const i32 held[] = {0x3c880000, 0x5e880000, -2129264640};
    for (const i32 target : held) {
        state.phase = 0xffffffffu;
        random_wave(state, 1);
        assert(state.held == target);
        assert(state.phase == 0);
        const u32 seed = state.sequence;
        random_wave(state, 0);
        assert(state.sequence == seed && state.held == target);
    }
    assert(random_sample_blend(0) == 16777216);
    assert(random_sample_blend(21474836) == 16777216);
    assert(random_sample_blend(21474837) == 16777220);
    assert(random_sample_blend(0x7fffffffu) == 285212671);

    ValueRandomState reference, target, neighbor;
    reset_value_random(reference, 12);
    reset_value_random(target, 12);
    reset_value_random(neighbor, 171);
    long long max_difference = 0;
    i32 reference_smoother = 0;
    for (unsigned tick = 0; tick < 12000; ++tick) {
        const u32 increment = value_random_increment(static_cast<i32>((tick * 32871937u) & 0x7fffffffu),
                                                     0x03159138u);
        const i32 value = random_wave(target, increment);
        random_wave(neighbor, 0x13577u * tick);
        assert(value == random_wave(reference, increment) && reference.sequence == target.sequence);
        const i32 alpha = random_sample_blend(increment);
        for (unsigned s = 0; s < 64; ++s) reference_smoother = expected_sample(reference_smoother, target.held, alpha);
        const long long difference = std::llabs(static_cast<long long>(reference_smoother) - value);
        if (difference > max_difference) max_difference = difference;
        assert(difference < 512);
    }
    assert(reference.sequence != neighbor.sequence);
    std::printf("Random reference: peak difference %lld Q31 LSBs across 12,000 blocks\n", max_difference);
}
}

int main(int argc, char **) {
    check_rate();
    check_waves(argc > 1);
    check_random();
    std::puts("PASS rate buckets, four Value shapes, phase wrap, independent Random and response");
}
