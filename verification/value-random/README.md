# Value and Random for AUX

Open-XY 0.0.7, OP-XY OS 1.2.1. These two source headers run in the audio-side AUX evaluator. Each enabled destination owns its own phase and Random state. The entire state occupies 16 bytes per destination. The adapter reuses the installed waveform and tempo data. It allocates no memory and advances only the selected source.

## Integration contract

Use `aux_value_random_stock.hpp`. Create one `aux_lfo::ValueRandomState` per supported AUX destination. Before its first evaluation or on a deliberate reset, call `aux_lfo::reset_value_random(state, seed)`. Give each track a distinct, repeatable seed. Call exactly one of the following per 64-frame evaluation with the raw rate parameter word from AUX slot 24.

| Source | Entry point | Return value |
| --- | --- | --- |
| Value | `value_wave_stock(state, rate_word, shape)` | Q31 first sample of the native Value block |
| Random | `random_wave_stock(state, rate_word)` | Q31 estimate of the final sample of the native Random block, using its private sequence |

Shape numbers 0, 1, 2 and 3 select sine, square, falling saw and rising saw, respectively. Sine and square are bipolar. Both saws are unipolar. Shape choice is explicit at the connector, so AUX can keep its normal destination and amount controls. A zero rate word selects the first tempo division. A half-scale word selects a stationary free oscillator. A new Random stream begins at zero and produces its first target at phase wrap, which can take a full cycle. Choosing a slow speed therefore has an audible startup consequence.

The tempo input is the stock audio-side word at 0x840426f4. The lower half of K1 has eight tempo divisions. Starting at half travel the increment is quadratic and independent of tempo. Value rounds its increment down to a multiple of 64 for the substeps. Random advances by the full increment. Native accesses in this module are read-only and confined to the version-specific adapter.

## Comparison with the instrument implementation

| Aspect | Evidence | Intentional differences or check still needed |
| --- | --- | --- |
| Value shapes | All 8,192 entries of the installed native sine table matched the host phase reference. The sign transition of the square and both ramp endpoints match the stock branches. | AUX consumes one scalar per evaluation. Native sine prepares intermediate audio-rate samples for its instrument caller. |
| Rate | Both native instrument branches read the same tempo input or use their matching Q31 rate equation. All 16,384 nonnegative K1 buckets and their endpoints passed the host test. | Actual callback cadence, note retrigger, pattern and transport behavior require the device comparison below. |
| Random | Phase-wrap rule, LCG constants, signed high word, blend threshold and saturating operations match the 1.2.1 routine. | The native global RNG would change the sequence heard on other tracks. Here each destination has its own seed. |
| Random response | The endpoint uses six Q31 squares and a weighted sum. In a 12,000-block test at changing rates it stayed within 241 Q31 LSBs of a sample-by-sample 64-iteration reference (about 0.000012% of positive full scale). | The reference uses the reconstructed 64-sample recurrence. It is a host regression check. Clock, rounded endpoints and duration still require hardware capture. |
| Screens | The module writes no display or asset data. | Browser content, interaction and stock artwork are owned and tested by the integration. |

## Run the checks

From this directory, set `OPEN_XY_SDK` to the pinned SDK checkout and `OP_XY_STOCK` to your stock 1.2.1 image. The test requires C++11, Python 3 and the SDK's Python reader. Its private sine-table check reads the authenticated image in memory.

```sh
c++ -std=c++11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
  -fno-sanitize-recover=all -I"$OPEN_XY_SDK/sdk/include" \
  test_value_random.cpp -o /tmp/test-value-random
/tmp/test-value-random
python3 verify_stock_table.py --sdk "$OPEN_XY_SDK" --stock "$OP_XY_STOCK" \
  --test /tmp/test-value-random
```

For device comparison, use the same rate, tempo and destination range on an instrument and one AUX track. Test the slowest sync division, the half-travel stationary point, and a fast free rate with transport stopped and running. Check each Value waveform against the expected polarity and period. For Random compare wrap timing and response time. Then enable all three AUX destinations, turn the rate on one, and verify the other two keep their timing. Change the source, leave and reopen the page, and repeat on a saved project to verify the integration's selected-source and reset behavior.
