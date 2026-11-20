"""Check the A2 Element input evidence in the user's unmodified 1.2.1 LDR."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("image", type=Path)
image = parser.parse_args().image.read_bytes()
if hashlib.sha256(image).hexdigest() != "60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f":
    parser.exit(1, "Stock OS 1.2.1 LDR hash mismatch\n")

sites = [
    ("Element selector table", 0x2CB48, "0843508d6043508df443508d4a44508d"),
    ("gyro read and negate", 0x13B564, "fee35ab9246c0fc400c0"),
    ("stock Element pooled input", 0x13B5BC, "09dab011480d0000"),
    ("stock Element follower defaults", 0x13B4C0, "81e144ac00da20410000000000e360af383081e144ac00da48430000000000e357af"),
    ("audio block timestamp then producer", 0x6094, "1a9359b0033000de4f8d2cf30000"),
    ("input UI labels", 0x3C118, "6d6963006c696e65000000006865616473657400"),
    ("in.svg mic label argument", 0x1801D2, "01da0584a4790000"),
    ("in.svg headset choice", 0x1801BC, "fce30292000c0860b91bb92f"),
    ("input lane selection hook", 0x1365E8, "fee3ecdf000c116001060832"),
    ("source 14 call hook", 0x136620, "70600e5b00e300d8"),
    ("stock router stereo block length", 0x15166C, "22e1000200de0204c4010000"),
]
for name, offset, data in sites:
    expected = bytes.fromhex(data)
    if image[offset:offset + len(expected)] != expected:
        parser.exit(1, f"{name}: unexpected bytes at {offset:#x}\n")
print(f"PASS stock 1.2.1 hash and {len(sites)} Element sites")
