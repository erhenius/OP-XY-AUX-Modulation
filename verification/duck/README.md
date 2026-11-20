# Duck for AUX

This module ports the OP-XY 1.2.1 Duck detector and envelope to the audio role. Each target gets its own envelope, two histories, and a 64-frame output block. The user’s authenticated OS supplies the DSP instructions at build time. The module includes no artwork and draws nothing.

## SDK integration

1. Build support installs `aux_lfo_duck_stock.py` next to `tooling/formats.py`. Call `write_asm(stock_image_bytes, private_build_dir / "aux_duck_kernel.s")`, assemble the generated source for BF707 and include its object in the audio payload. The generated assembly and object contain private OS material and belong only in the user's build directory. The image hash must match the SDK 0.0.7 OS 1.2.1 profile.
2. Include `aux_lfo_duck.hpp` in the audio-side AUX control evaluator. Allocate one zero-initialized `DuckState` for each of tracks 11, 14 and 15. Each occupies 512 bytes with the target ABI. Reset on ownership entry and project, pattern, source-mode or instance-generation change. Disable/re-enable must cause a new generation. Repeated calls while one instance owns its source retain its histories.
3. Call `duck_step(state, {words[24], words[25], words[26], words[27], source_type})` once per enabled **64-frame audio block**. It reads stock detectors directly on the audio role. The four words are source, amount, hold and release, all normalized, nonnegative Q31. Source type is zero for audio or `INT32_MAX` for note. Bins 8–16 are always event sources, exactly as on stock. The audio detector for bins 0–7 comes from the ordinary instrument mix path and may describe the previous block.
4. Choose the normalized destination’s *restored* baseline. For FX I/II use return-level slot 41; on Morph use movement slot 3. Compute `duck_delta(base, duck_step(...))` and let stock apply that additive offset transiently once. If the AUX application will multiply it by the generic signed depth, replace that multiplier with Q31 unity. Stock Duck instead takes the unipolar K2 word directly. At minimum there is no ducking; at maximum a full detector envelope can take the return or Morph position to its normalized zero. Do not write the movement result into a pattern or feed the previous tick’s ducked value back as the next baseline.
5. The control and drawing adapter lives in the shared service. Stock-screen parity requires comparing the actual Duck labels, detector type, drawing and picker behavior on the OP-XY alongside the stock instrument M4 page.

## What remains stock

The kernel resides at 0x8d5044d4 through 0x8d504773. The generator preserves its detector selection, software coefficient calculations, hardware envelope loop and first output sample. Three absolute loads now address the instance's private histories at offsets 0x80 and 0x84 and its private result block at offset 0x88. The physical generator index is always zero. Two PC-relative coefficient-helper calls reach the original helper through a local trampoline. No stock instrument histories or scratch samples are overwritten. The steady-state wrapper avoids the kernel's unequal hold/amount cache comparison repeating the same software coefficient calculation. It refreshes on initial entry and changes to amount, hold or release.

Stock instrument volume takes sample zero of the native output at 0x11a04320 and applies K2 depth at 0x11a04328. The result block spans **64 words**, not a pair. From 0x8d50462a the immediate-count hardware loop stores 63 values through P2++ and the epilogue stores the 64th. An independent stock 64-frame gain loop at 0x11a0436c has the same count field. The A1 notes identified the two store sites before determining their loop count. The Blackfin+ architecture initializes the counter from an unsigned immediate and decrements it after each loop bottom. [Analog Devices instruction note](https://ez.analog.com/dsp/blackfin-processors/bf70x/a/documents/do9550/faq-enhancements-in-hardware-loop-in-blackfin-core)

The return and Morph adapters operate in normalized parameter space. Instrument group-3 volume has its own sample-rate gain ramp. Agreement of the raw Duck envelope does not establish identical loudness curves for that instrument ramp and the FX wet-level control.

## Verification

Run from the module directory, using the private OS and SDK checked out locally.

```sh
python3 test_stock.py /path/to/open-xy-0.0.7 /path/to/private/stock.os.ldr
c++ -std=c++11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
  -I/path/to/open-xy-0.0.7/sdk/include test_duck.cpp -o /tmp/test-duck
/tmp/test-duck
```

The authenticated 682-byte build result, including its 10-byte trampoline, must have SHA-256 `b4d0927d13354b6343e6e7008562c8a69ccf771e8e3d925eaa7bc7f897670bb9`. The stock audit checks the two call destinations, every replaced pointer, the unchanged instruction spans, the 64-word write extent and refusal of an unknown OS. A host ABI double writes all 64 samples, checks isolation, coefficient caching and reset, and exercises the fixed-point depth and baseline scaling. It does not execute the stock BF707 kernel.

For device acceptance, compare stock instrument Duck and AUX Duck fed by the same drum using audio and note selection. Test short/long gates, silence, retrigger and held notes, the metronome, minimum/middle/maximum amount, hold and release, and activity while navigating other pages. On FX I and II, solo each wet return and listen to the dry signal separately. On Morph test rest at A, centre, and B and manual pad/encoder takeover. Finally run both sends with opposing settings alongside Morph and a stock instrument Duck, change patterns, stop/start, and disable/re-enable. Check for cross-talk, stuck attenuation, altered saved parameter values and audible underruns. These hardware observations remain open.
