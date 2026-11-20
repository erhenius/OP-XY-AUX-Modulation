#!/usr/bin/env python3
"""Compile and run the real AUX GUI/audio service across simulated stock role boundaries."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=Path, help="assembled AUX modulation SDK")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    platform = sdk / "sdk/platform/opxy-1.2.1"
    with tempfile.TemporaryDirectory(prefix="aux-lfo-runtime-") as tmp:
        fixture = Path(tmp)
        # Match the upstream host tests' stock-address shim. Only memory addresses
        # and external firmware calls are redirected. The runtime and GUI are real.
        for source in platform.iterdir():
            if source.is_file() and source.suffix in (".hpp", ".cpp", ".inc"):
                shutil.copy(source, fixture / source.name)
        stock = fixture / "stock.hpp"
        text = stock.read_text()
        old = "typedef __UINTPTR_TYPE__ Address;"
        assert old in text
        text = text.replace(old, old + "\nAddress host_stock_address(Address);")
        for ty in ("T*", "const volatile u32*", "const volatile unsigned char*"):
            old = f"reinterpret_cast<{ty}>(address)"
            assert old in text
            text = text.replace(old, f"reinterpret_cast<{ty}>(host_stock_address(address))")
        stock.write_text(text)

        # The Value adapter holds two direct, read-only native pointers. Route
        # only these pointer literals to the host fixture's clock and sine table.
        value = fixture / "aux_value_random_stock.hpp"
        text = value.read_text()
        for ty, address in (("u32", "0x840426f4u"), ("i16", "0x84048aa4u")):
            expr = f"reinterpret_cast<const volatile opxy::{ty} *>({address})"
            assert expr in text, f"Update shim for {value.name}"
            text = text.replace(expr, f"opxy::platform::at<const volatile opxy::{ty}>({address})")
        value.write_text('#include "stock.hpp"\n' + text)

        element = fixture / "element_stock.cpp"
        if not element.exists():
            element = fixture / "aux_lfo_element.cpp"
        binary = fixture / "runtime-tests"
        cmd = [args.cxx, "-std=c++11", "-O2", "-Wall", "-Werror", "-DOPXY_AUX_LFO",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all", "-pipe",
                        "-I" + str(fixture), "-I" + str(sdk / "sdk/include"),
                        "-I" + str(sdk / "sdk/platform"), str(HERE / "runtime_host.cpp"),
                        str(element), str(fixture / "track_control.cpp"), str(fixture / "synth_pages.cpp"), str(fixture / "track_control_project.cpp"), "-o", str(binary)]
        for flags in (["-DQA_STANDALONE"], []):
            subprocess.run(cmd + flags, check=True)
            subprocess.run([binary], cwd=fixture, check=True)


if __name__ == "__main__":
    main()
