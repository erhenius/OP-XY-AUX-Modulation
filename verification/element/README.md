# AUX Element on OS 1.2.1

This source provides read-only audio-role controls for gyro and built-in microphone. A single microphone follower supplies all three clients. Existing AUX code owns control depth, destination and release. Stock draws the artwork.

## Gyro

The instrument Element gyro branch at 0x8d504308 calls the signed 16-bit motion getter at 0x8d4db5bc and reverses polarity. The adapter adopts this polarity and maps Q15 to Q31. It saturates raw −32768 at the representable positive endpoint. The published scalar is the current sensor position; the instrument engine's per-sample interpolation buffer is private.

## Built-in microphone

Stock instrument Element reads paired samples from 0x11b00d48. That buffer follows the active recorder/external-input source and can contain line input, USB, or another source. The combined build captures the independent physical microphone branch inside the same stock audio producer.

| Evidence | Stock OS 1.2.1 |
| --- | --- |
| Audio block cycle timestamp, including a null-input block | 0x11a02a04 and 0x11a02a06 write 0x08034f68 and 0x08034f6c before the call at 0x11a02a0a |
| Internal and headset identity | The in.svg renderer at 0x8d548f6c binds mic and headset nodes from 0x840579a4. On source 0, the tests at 0x8d548f54 and 0x8d548f60 select mic for a zero return from 0x8d4db364 and headset otherwise. |
| Physical signal | 0x8d4ff38c obtains the same selector. 0x8d4ff390–0x8d4ff3a4 choose capture lane 0 for internal mic, lane 2 for headset, and step 48 bytes through the physical input frame. This pass follows each recorder-source switch branch. |
| Prepared block | 0x8d4ff3c4 loads source 14. 0x8d4ff3c8 passes the paired input buffer to 0x8d51a3c8. Its copy size at 0x8d51a410 is 512 bytes, 64 pairs of 32-bit input. |
| Follower shape | Stock at 0x8d504360 takes abs(max(a,b)), then cascades two attack/release Q31 poles. The constructor at 0x8d504264 supplies 10 and 200 ms at 44.1 kHz. The adapter uses precomputed default coefficients and emits the final value of the block. Coefficient rounding and microphone gain require hardware comparison. |

The first replacement calls the original getter exactly once and records its actual return for the second replacement. This prevents a headset hotplug between selection and capture from relabeling the buffer. The observer forwards the original source and pointer exactly once even when disabled. It changes neither input selection nor level, active recording, source-14 routing, or effects wet level.

| LDR offset | Call site | Expected bytes | Replacement symbol | Original target |
| --- | --- | --- | --- | --- |
| 0x1365e8 | 0x8d4ff38c | fe e3 ec df | aux_lfo_mic_input_lane | 0x8d4db364 |
| 0x136624 | 0x8d4ff3c8 | 00 e3 00 d8 | aux_lfo_observe_mic | 0x8d51a3c8 |

Both checked PC-relative CALLs require an even target displacement from −16,777,216 through +16,777,214 bytes. The exporter must verify both preimages and displacements before emitting the image. These hooks are in input capture, independent of the MultiFX/Veff destination and telemetry sites later in AUX evaluation.

## Cost and lifecycle

The observer checks word 57 of the live audio parameter row for Morph (11), FX I (14) and FX II (15). A marked, enabled Mic source enables the follower. Any other combination skips its sample loop. With Mic active, cost is 128 SDK Q31 multiplies per 64-frame capture block, 88,200 per second at 44.1 kHz, shared among selected tracks. The producer records a block timestamp, and reads with a different timestamp return zero. Headset, missing input, unexpected source, and last-subscriber removal clear the envelope. There are no audio allocations or per-sample messages. Native timing and the end-to-end capture-to-HUD delay need measurement on hardware.

## Verification

From the workspace root, set the two paths and run the host tests.

```sh
element=aux-lfo-work/a2/modules/element
sdk=aux-lfo-work/a2/base/sdk/sdk
c++ -std=c++11 -O2 -Wall -Wextra -Werror -fsanitize=undefined \
  -fno-sanitize-recover=all -I"$element/tests/mock" -I"$element" -I"$sdk/include" \
  "$element/element_stock.cpp" "$element/tests/test_element.cpp" -o /tmp/test-aux-element
/tmp/test-aux-element
python3 "$element/tests/check_stock.py" /path/to/stock.os.ldr
```

On an XY, compare built-in microphone response against stock instrument Element. Leave external input alternately on mic, headset, line, USB and resample. With Mic enabled on Morph and both FX, tap the case, inject line/USB audio in silence, insert/remove a headset and deselect the last subscriber. Check audio recording, monitor level, source identity, motion direction, destinations, project switching, timing and CPU under an eight-track project. A powered-down internal microphone can return silence. The extension leaves hardware power policy to stock.
