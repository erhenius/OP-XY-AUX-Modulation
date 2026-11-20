"""Audit the five A2 relocations, output extent, and OS checks against a private image."""
import argparse
import hashlib
from pathlib import Path
import struct
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("sdk", type=Path)
parser.add_argument("stock", type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.sdk.resolve()))
import aux_lfo_duck_stock as duck
from tooling.formats import ldr, mapped_offset

image = args.stock.read_bytes()
blocks = ldr(image)
def read(addr, n):
    offset = mapped_offset(blocks, addr, n)
    return image[offset:offset+n]

out = duck.isolated_code(image)
original = read(duck.KERNEL_START, len(out))
assert len(out) == 672 and hashlib.sha256(original).hexdigest() == duck.KERNEL_SHA
def words(data, pc, n):
    return struct.unpack_from(f"<{n}H", data, pc - duck.KERNEL_START)
expected_edits = set()
for pc, reg, offset in ((0x8d504556, 0, 0x84), (0x8d50455e, 1, 0x80), (0x8d50461e, 2, 0x88)):
    expected_edits.update(range(pc-duck.KERNEL_START, pc-duck.KERNEL_START+8))
    move, *adds = words(out, pc, 4)
    assert move == 0x3244 + (reg << 3)  # P[reg] = P4
    assert all((v & 0xfc07) == (0x6c00 | reg) for v in adds)
    assert sum((v >> 3) & 127 for v in adds) == offset
for pc in (0x8d5046e8, 0x8d504744):
    expected_edits.update(range(pc-duck.KERNEL_START, pc-duck.KERNEL_START+4))
    for body, callee in ((original, 0x8d51a130), (out, duck.KERNEL_END)):
        op, rel = words(body, pc, 2)
        assert op == 0xe300 and pc + rel * 2 == callee
assert all(x == y for i, (x, y) in enumerate(zip(original, out)) if i not in expected_edits)

# BF707 immediate-counter loop. The bottom offset is relative to the setup instruction.
setup, fields = words(out, 0x8d50462a, 2)
assert setup == 0xe0c0
count = (setup & 15) << 6 | fields >> 10
bottom = 0x8d50462a + (fields & 1023) * 2
assert count == 63 and bottom == 0x8d504664
# Independent 64-frame stock gain loop: 63 iterations, then one epilogue sample.
gain_setup, gain_fields = struct.unpack("<HH", read(0x11a0436c, 4))
assert gain_setup == setup and (gain_fields >> 10) == count
assert 0x11a0436c + (gain_fields & 1023) * 2 == 0x11a0437c
assert words(out, 0x8d504640, 1) == (0x9211,)  # [P2++] = R1
assert words(out, 0x8d504678, 1) == (0x9311,)  # [P2] = R1
assert (count + 1) * 4 == 0x11902c00 - 0x11902b00
with tempfile.TemporaryDirectory() as temp:
    asm = Path(temp) / "private.s"
    duck.write_asm(image, asm)
    assembly = asm.read_text().splitlines()
    assert "sdk_aux_duck_kernel:" in assembly
    native = bytes(int(v, 16) for line in assembly if line.strip().startswith(".byte ")
                   for v in line.strip()[6:].split(","))
    assert native[:-10] == out and len(native) == duck.NATIVE_SIZE == 682
    assert struct.unpack("<5H", native[-10:]) == (0xe109, 0xa130, 0xe149, 0x8d51, 0x0051)
    assert hashlib.sha256(native).hexdigest() == duck.NATIVE_SHA
try:
    duck.isolated_code(bytes(len(image)))
except Exception as error:
    assert "OS 1.2.1" in str(error)
else:
    raise AssertionError("unknown OS image accepted")
print("PASS verified stock kernel, pointer instructions, 64-sample footprint, two external calls, and incorrect OS rejection")
