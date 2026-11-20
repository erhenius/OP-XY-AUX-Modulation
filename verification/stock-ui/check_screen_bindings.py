"""Check the private OS provides the native screens and drawing inputs used by A3."""
import argparse
from hashlib import sha256
from pathlib import Path
import struct
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--stock', required=True, type=Path)
args = parser.parse_args()
sys.path.insert(0, str(args.sdk.resolve()))
from tooling.formats import ldr, mapped_offset, require
from tooling.firmware import decode_branch

data = args.stock.read_bytes()
require(sha256(data).hexdigest() == '60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f',
        'Expected OS 1.2.1')
blocks = ldr(data)
def read(address, size):
    offset = mapped_offset(blocks, address, size)
    return data[offset:offset + size]
def check(actual, expected, purpose):
    require(actual == expected, purpose)

for title, page, constructor, asset, name, vtable, frame, params, hints in (
    ('Element', 29, 0x8d53f8a8, 0x84056f98, 'reroutelfo.svg', 0x84056ef8, 0x8d53fab8, 0x8d53fade,
     ['gforce', 'input', 'lfo_value', 'lfo_line']),
    ('Random', 28, 0x8d540730, 0x840570b0, 'rndlfo.svg', 0x84056fe0, 0x8d540a84, 0x8d540a9a,
     ['speed_label', 'level_label', 'lfo_value', 'env']),
    ('Value', 27, 0x8d543100, 0x840572b4, 'singlelfo.svg', 0x840571cc, 0x8d543208, 0x8d543220,
     ['graph_sine', 'graph_sqr', 'graph_saw', 'graph_saw_inv']),
):
    check(read(asset, len(name)+1), name.encode()+b'\0', title+' artwork')
    check(read(constructor+8, 8), struct.pack('<4H', 0xda02, asset>>16, asset&65535, 0), title+' binds original asset')
    instruction, = struct.unpack('<H',read(constructor+16, 2))
    check(instruction, 0x6000 + page*8 + 1, title+' page number')  # R1 = imm7 signed
    check(read(vtable+12, 4), struct.pack('<I',frame), title+' stock draw vtable')
    check(decode_branch(params, read(params, 4)), (0x8d4d7de8, True), title+' draw parameter accessor')
    names = read(vtable+24, asset-vtable-24)
    for hint in hints:
        require(hint.encode()+b'\0' in names, title+' expected original leaf '+hint)
# Selected source samples reach these two pages on the standard GUI telemetry
# lane. The check authenticates the target register of each absolute load.
for at in (0x8d53ffca, 0x8d541098):
    check(read(at, 8), struct.pack('<4H', 0xd800, 0x0803, 0x773c, 7),
          'Native page reads selected LFO wave from telemetry word 22')
print('PASS native stock screen identity, draw vectors, leaf families and live telemetry readers')
