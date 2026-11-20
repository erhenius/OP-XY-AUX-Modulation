# Building AUX Modulation

TE Builder applies both patches from `extension-bundle.json` in order, verifies their checksums and builds the extension selected in the queue. For a manual build, start with a fresh checkout of Open-XY and the AUX Modulation repository.

From the root of AUX Modulation, run the following in one shell. Pick an unused path for the SDK checkout and build output.

```sh
aux_repo="$PWD"
git clone https://github.com/etcetc000/open-xy.git ../open-xy-aux-sdk
cd ../open-xy-aux-sdk
git checkout --detach 28dd1a2e36599108c5eecf1a03f3b28601e50b39
git apply "$aux_repo/integration/extension-only-collection.patch"
git apply "$aux_repo/integration/aux-lfo-service.patch"
```

The first patch allows a collection containing extensions alone. The second wires the selected AUX service into that build. The extension ID is `aux_lfo`. The stock OS remains an input even for measuring the extension because the build prepares Duck’s kernel from the owner’s image.

Set `stock_os` below to the path of your original OP-XY 1.2.1 image. Use the pinned SDK’s [toolchain instructions](https://github.com/etcetc000/open-xy/blob/28dd1a2e36599108c5eecf1a03f3b28601e50b39/docs/build.md) for your host. On a supported download host, install the SDK toolchain with `python3 sdk.py toolchain fetch`.

```sh
stock_os="/absolute/path/to/stock.os.ldr"
python3 "$aux_repo/verification/run_checks.py" --sdk "$PWD" --stock "$stock_os"
python3 sdk.py build --extension "$aux_repo/extension/aux_lfo" --stock-os "$stock_os" --profiling off --out ../aux-modulation-build
```

The checks exercise the four source families, alternate trigger and waveform choices, GUI and audio parameter routing, project save and restore, and selection across both FX sends and Morph. They require a C++11 host compiler with undefined-behavior sanitizer support. Device playback and native build verification are separate from the host fixtures.

## Packaging

From the AUX Modulation repository root, run `python3 tools/package_release.py` to regenerate the TE Builder ZIP under `release/`. Run `python3 tools/package_release.py --check` to check its contents against the source tree and validate the patch digests. For Git imports, keep the manifest and both integration patches together at the root layout shown in this repository.
