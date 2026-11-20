#!/usr/bin/env python3
"""Feed all 8192 private OS sine samples to the compiled host test over stdin."""

import argparse
from pathlib import Path
import subprocess
import sys

STOCK_121_SHA256 = "60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--stock", required=True, type=Path)
    parser.add_argument("--test", required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.sdk.resolve()))
    from tooling.formats import ldr, mapped_offset, require, sha

    data = args.stock.read_bytes()
    require(sha(data) == STOCK_121_SHA256, "Expected authenticated OS 1.2.1")
    start = mapped_offset(ldr(data), 0x84048aa4, 8192 * 2)
    subprocess.run([str(args.test.resolve()), "--stock-table"], input=data[start:start + 8192 * 2], check=True)
    print("PASS complete signed stock sine table from authenticated OS 1.2.1")


if __name__ == "__main__":
    main()
