"""Verify the selected OS dispatches physical Shift+M4 to the SDK's exact site."""
import argparse
import json
from pathlib import Path
import struct
import sys


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--stock", required=True, type=Path)
    args.add_argument("--sdk", required=True, type=Path)
    args = args.parse_args()
    sdk = args.sdk.resolve()
    sys.path.insert(0, str(sdk))
    from tooling.aux_lfo_profile import configure
    from tooling.formats import ldr, mapped_offset, require, sha

    profile = json.loads((sdk / "profiles/opxy-1.2.1.json").read_text())
    configure(profile)
    image = args.stock.read_bytes()
    require(sha(image) == profile["stock_sha256"], "Unrecognized stock image")
    blocks = ldr(image)

    def word(address):
        offset = mapped_offset(blocks, address, 4)
        return struct.unpack_from("<I", image, offset)[0]

    require(word(0x0803fb6c + 0x10) == 0x8c08f6b0, "Unexpected AUX shifted-key method")
    key_code = 9   # M1..M4 are 6..9. The switch indexes from key code 2.
    case_address = word(0x8400b4e8 + 4 * (key_code - 2))
    site, = (p for p in profile["patches"] if p.get("symbol") == "sdk_aux_lfo_shift_m4_hook")
    require(case_address == int(site["address"], 16), "Shift+M4 case differs from patch site")
    offset = mapped_offset(blocks, case_address, site["length"])
    require(offset == int(site["file_offset"], 16), "Shift+M4 stock mapping differs")
    require(image[offset:offset + site["length"]] == bytes.fromhex(site["before"]), "Shift+M4 stock bytes differ")
    # Verify instrument M4 alternate parameter selection / menu of allowed discrete values.
    for table, index, expected in ((0x8400e308, 1, 0x8c0b26ce), (0x8400e308, 6, 0x8c0b26e2),
                                   (0x8400e2ec, 1, 0x8c0b2598), (0x8400e2ec, 6, 0x8c0b2634)):
        require(word(table + 4 * index) == expected, "Unexpected instrument M4 control mapping")
    print("PASS stock SHA, shifted handler, M4 key, original bytes, instrument Value/Duck control mappings")


if __name__ == "__main__":
    main()
