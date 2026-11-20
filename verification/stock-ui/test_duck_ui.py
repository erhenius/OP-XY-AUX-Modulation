"""Check stock Duck selectors and exercise the actual patched SDK control editor."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def check_stock(sdk, image):
    sys.path.insert(0, str(sdk))
    from tooling.formats import ldr, mapped_offset, require, sha
    data = image.read_bytes()
    require(sha(data) == '60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f',
            'Expected stock 1.2.1')
    blocks = ldr(data)

    def read(address, size):
        offset = mapped_offset(blocks, address, size)
        return data[offset:offset + size]

    def verify(branch, target):
        op, low = struct.unpack('<2H', read(branch, 4))
        offset = ((op & 0xff) << 16) | low
        if offset & 0x800000:
            offset -= 0x1000000
        require(op >> 8 == 0xe3 and branch + 2 * offset == target, 'Stock call site mismatch')

    require(read(0x84057190, 17) == b'sidechainlfo.svg\x00', 'Duck artwork mismatch')
    require(read(0x8d541c7c, 8) == struct.pack('<4H', 0xda02, 0x8405, 0x7190, 0), 'Duck constructor asset')
    require(read(0x8d541c84, 2) == bytes.fromhex('f160'), 'Duck page id 30')
    require(struct.unpack('<I', read(0x8400e2ec + 6 * 4, 4))[0] == 0x8c0b2634, 'Duck classifier')
    require(read(0x8c0b2658, 4) == bytes.fromhex('88601000'), 'Duck source returns 17')
    require(read(0x8c0b2594, 4) == bytes.fromhex('f8631000'), 'Default returns continuous')
    require(struct.unpack('<I', read(0x840570d4 + 12, 4))[0] == 0x8d5428bc, 'Duck draw callback')
    verify(0x8d542942, 0x8d4d6f14)
    verify(0x8d542946, 0x8d4d7de8)
    verify(0x8d542cc6, 0x8d54216c)
    require(read(0x8d542d8e, 2) == bytes.fromhex('08a1'), 'Duck source type display input')


def check_steps(sdk, target):
    platform = sdk / 'sdk/platform/opxy-1.2.1'
    fixture = (sdk / 'tests/fixtures/synth_steps.cpp').read_text()
    # The SDK test service supplies the real sdk_param_steps plus its stock fallback.
    # Add only the new page-mode boundary, then exercise all eligible controls.
    fixture = fixture.replace('int main() {', '''
static opxy::u32 visible_track, visible_mode;
extern "C" opxy::u32 sdk_aux_lfo_screen_mode(opxy::u32 track) {
    return track == visible_track ? visible_mode : 0;
}
int main() {
    for (auto track : {11u, 14u, 15u}) {
        visible_track = track;
        for (auto mode : {aux_lfo::Duck, aux_lfo::DuckNote}) {
            visible_mode = mode;
            const u32 before = stock_asks;
            u32 scale = 2;
            require(sdk_param_steps(track, 24, &scale) == 17 && scale == 2, "Source quantization");
            for (u32 slot = 25; slot <= 27; ++slot)
                require(sdk_param_steps(track, slot, &scale) == 0xffffffffu, "Continuous Duck knob");
            require(stock_asks == before, "Duck delegated to generic AUX classifier");
            sdk_param_steps(track, 28, &scale);
            sdk_param_steps(2, 26, &scale);
            require(stock_asks == before + 2, "Duck intercepted another slot or track");
        }
        visible_mode = aux_lfo::Stock;
        u32 scale = 2;
        const u32 before = stock_asks;
        sdk_param_steps(track, 26, &scale);
        require(stock_asks == before + 1, "Generic AUX override leaked");
    }
    stock_asks = 0;
''')
    stock = (platform / 'stock.hpp').read_text()
    stock = stock.replace('typedef __UINTPTR_TYPE__ Address;',
                          'typedef __UINTPTR_TYPE__ Address;\nAddress host_stock_address(Address);')
    for ptr in ('T*', 'const volatile u32*', 'const volatile unsigned char*'):
        stock = stock.replace('reinterpret_cast<' + ptr + '>(address)',
                              'reinterpret_cast<' + ptr + '>(host_stock_address(address))')
    (target / 'stock.hpp').write_text(stock)
    for source in ('synth_controls.cpp', 'drum_lock_store.hpp'):
        (target / source).write_bytes((platform / source).read_bytes())
    (target / 'test.cpp').write_text('#include <initializer_list>\n' + fixture)
    binary = target / 'run'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++11', '-O2', '-Wall', '-Werror', '-DOPXY_AUX_LFO=1',
                    '-I' + str(target), '-I' + str(sdk / 'sdk/include'), '-I' + str(platform),
                    str(target / 'test.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--stock', required=True, type=Path)
    args = parser.parse_args()
    check_stock(args.sdk.resolve(), args.stock)
    with tempfile.TemporaryDirectory() as d:
        check_steps(args.sdk.resolve(), Path(d))
    print('PASS stock Duck drawable, parameter-vector path and all Duck control classifiers')
