# AUX Modulation for OP-XY

Give FX I and FX II their own modulation sources. Duck a reverb tail from a kick, tip the XY to move a delay parameter, or run different LFOs on the two sends. With Morph installed on the CV track, its movement can follow a third independent source. Each track and pattern remembers its choice.

## Install

AUX Modulation can be built and installed with the Open-XY SDK alone. It targets OP-XY OS 1.2.1 and Open-XY 0.0.7 (SDK API 1.16).

**Using the Open-XY CLI:** follow the [build and install instructions](docs/BUILD.md). The repository includes both SDK patches and the extension source. Supply your stock OS image and the SDK’s supported Blackfin toolchain.

**Using TE Builder:** version 0.5.8 supports this standalone extension and manages those inputs for you.

- **GitHub** — copy this repository’s HTTPS clone URL into **File → Import Git Repository…**. TE Builder follows the default branch and reads the extension manifest at the repository root.
- **ZIP** — use **File → Import Machine or Extension ZIP…** and select [AUX-Modulation-1.0.0.zip](release/AUX-Modulation-1.0.0.zip) intact.

In **Available**, select **AUX modulation 1.0.0**, queue it, review the build and stage it on your XY. For an FX-only setup you can build the extension on its own. To use Morph as well, select the regular **Morph 1.3.1-dev-gui2d22**. When upgrading from the prototype named **Morph-gui2d22 · AUX LFO A1**, switch Morph to the regular edition first. A1 and this extension claim the same firmware hook.

## Play

On FX I, FX II or the Morph track, hold **Shift** and press **M4** to choose a type. Press **M4** to see its original stock screen. Enable the LFO on that track and raise its amount. For Morph, enable LIVE and select movement for sources with a selectable destination.

| Type | Controls |
| --- | --- |
| Stock | The regular AUX oscillator and its usual controls. |
| Duck | Turn K1 to select a trigger track or the metronome. For instrument tracks 1–8, click K1 to switch between audio and note events. AUX tracks 1–8 use note events. K2 sets amount, K3 hold and K4 release. On FX it ducks the wet return. On Morph it pulls movement toward A. |
| Element | Turn K1 to choose the gyro or built-in mic. K2 sets depth. K3 and K4 select the destination and parameter. Mic operation follows the stock input routing. |
| Random | Turn K1 for rate and K2 for depth. K3 and K4 select the destination and parameter. Each target has its own free-running sequence. |
| Value | Turn K1 for rate. Click K1 to cycle sine, square, falling saw and rising saw. K2 sets depth. K3 and K4 select the destination and parameter. Value runs freely on AUX. |

**Try Duck on a reverb.** Send a sustained sound to FX I. Choose Duck, turn K1 to the track playing your kick and raise K2. Turn K4 to hear the reverb return between hits. Click K1 on an instrument source to compare how Duck follows the kick’s audio tail and its note events.

## Build and verification

The repository contains the extension, both required SDK patches and host checks. The public release carries the hardware-tested A3 implementation and keeps its saved source IDs. See [building from source](docs/BUILD.md) for the pinned checkout, tests and release packaging.

The extension source uses [GPL version 3](LICENSE), following the Morph and Septet releases. The [Open-XY license](LICENSES/OPEN-XY-LICENSE) accompanies the SDK-derived portions. The build reads stock code and artwork from the owner’s installation.
