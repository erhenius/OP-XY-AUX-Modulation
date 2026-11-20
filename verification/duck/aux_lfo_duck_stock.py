"""Prepare the isolated OS 1.2.1 Duck generator from the owner's stock image."""
import hashlib
import struct

from tooling.formats import ldr, mapped_offset, require

STOCK_SHA = "60358afdaa9b58ed40411602f6d5d4e1e3e02553ff2da7e25038e71dd002b52f"
KERNEL_START = 0x8d5044d4
KERNEL_END = 0x8d504774
NATIVE_SHA = "b4d0927d13354b6343e6e7008562c8a69ccf771e8e3d925eaa7bc7f897670bb9"
NATIVE_SIZE = 682
# P1.L = 0xa130; P1.H = 0x8d51; JUMP (P1). No relocations.
COEFFICIENT_TAIL = struct.pack("<5H", 0xe109, 0xa130, 0xe149, 0x8d51, 0x0051)
KERNEL_SHA = "636780dc86bc7e62a852ad1ec2aa3754e88ff4564c80fad7e4e4940ded48c07e"


def private_pointer(register, offset):
    # Four 16-bit Blackfin instructions, keeping all original offsets intact.
    # Pn = P4; Pn += 48; Pn += 48; Pn += offset-96. Pointer arithmetic leaves
    # CC and DSP accumulator registers available to the original generator.
    require(96 <= offset <= 159 and 0 <= register <= 2, "Duck private pointer range")
    move_from_p4 = 0x3000 | (1 << 9) | (1 << 6) | (register << 3) | 4
    def add_pointer(value):
        return 0x6c00 | (value << 3) | register
    return struct.pack("<4H", move_from_p4, add_pointer(48),
                       add_pointer(48), add_pointer(offset - 96))


def isolated_code(stock):
    require(hashlib.sha256(stock).hexdigest() == STOCK_SHA, "Duck requires verified OS 1.2.1")
    at = mapped_offset(ldr(stock), KERNEL_START, KERNEL_END - KERNEL_START)
    code = bytearray(stock[at:at + KERNEL_END - KERNEL_START])
    require(hashlib.sha256(code).hexdigest() == KERNEL_SHA, "Unexpected Duck kernel")
    def replace(pc, old, new):
        start = pc - KERNEL_START
        require(len(old) == len(new) and code[start:start + len(old)] == old,
                f"Duck instruction mismatch at {pc:#x}")
        code[start:start + len(old)] = new

    # Original state occupies [0x00,0x79). Pass private logical index zero.
    # All three former global pointers are now offsets in the SAME instance.
    replace(0x8d504556, bytes.fromhex("08da0308b88c0000"), private_pointer(0, 0x84))
    replace(0x8d50455e, bytes.fromhex("09da0308988c0000"), private_pointer(1, 0x80))
    replace(0x8d50461e, bytes.fromhex("0ada9011002b0000"), private_pointer(2, 0x88))

    # The two relative calls into stock both compute envelope coefficients.
    # Target a local tail trampoline with a full absolute callee address.
    # Other calls use fixed BF707 CALL.A immediates; short branches and
    # hardware-loop bounds are internal and unmoved relative to each other.
    for pc, old in ((0x8d5046e8, "00e324ad"), (0x8d504744, "00e3f6ac")):
        distance = (KERNEL_END - pc) // 2
        replace(pc, bytes.fromhex(old), struct.pack("<HH", 0xe300, distance))
    return bytes(code)


def write_asm(stock, path):
    """Write privately generated assembly to the current build's scratch folder."""
    code = isolated_code(stock) + COEFFICIENT_TAIL
    require(len(code) == NATIVE_SIZE and hashlib.sha256(code).hexdigest() == NATIVE_SHA,
            "Isolated Duck code differs from the approved native routine")
    lines = [
        '.section .text', '.align 4', '.global sdk_aux_duck_kernel',
        '.type sdk_aux_duck_kernel, STT_FUNC', 'sdk_aux_duck_kernel:',
    ]
    for start in range(0, len(code), 16):
        lines.append('    .byte ' + ','.join(f'0x{v:02x}' for v in code[start:start+16]))
    lines.append('.size sdk_aux_duck_kernel, .-sdk_aux_duck_kernel')
    path.write_text('\n'.join(lines) + '\n')
