#!/usr/bin/env python3
"""m92-pi: a tiny made-up "uccops" set to smoke-test the machine without real ROMs.

Main CPU: sets up the PIC, a grey palette through the GA21 DMA, fills layer 3 with a
patterned tile, scrolls it in the vertical blank interrupt and sends the frame count to
the sound CPU. Sound CPU (opcodes encrypted with the Undercover Cops table): plays a
tone on the YM2151 and answers every command through the second latch.
    python3 test/make_synth.py third_party/mame/irem_cpu.cpp out/uccops.zip
"""
import re, sys, zipfile

src = open(sys.argv[1]).read()
m = re.search(r'dynablaster_decryption_table\[256\]\s*=\s*\{(.*?)\};', src, re.S)
body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
table = [int(x, 16) if x.startswith('0x') else -1 for x in (t.strip() for t in body.split(',')) if x]
assert len(table) == 256, len(table)
inv = {}
for enc, dec in enumerate(table):
    inv.setdefault(dec, enc)

def enc(op):
    assert op in inv, hex(op)
    return inv[op]

# ---------------------------------------------------------------- main CPU
main = bytearray(0x100000)
def put(a, bs):
    main[a:a + len(bs)] = bytes(bs)

put(0x7fff0, [0xEA, 0x00, 0x01, 0x00, 0x00])          # jmp far 0000:0100
put(0x80, [0x00, 0x02, 0x00, 0x00])                    # vector 0x20 -> 0000:0200

code = []
def c(*b): code.extend(b)
c(0xFA)                          # cli
c(0xB8, 0x00, 0xE0, 0x8E, 0xD0)  # mov ax,e000 / mov ss,ax
c(0xBC, 0xF0, 0xFF)              # mov sp,fff0
c(0xB0, 0x13, 0xE6, 0x40)        # ICW1: edge, single, ICW4
c(0xB0, 0x20, 0xE6, 0x42)        # ICW2: vectors from 0x20
c(0xB0, 0x01, 0xE6, 0x42)        # ICW4
c(0xB0, 0xF6, 0xE6, 0x42)        # OCW1: IR0 (vblank) and IR3 (sound answer)
c(0xB8, 0x10, 0x00, 0xE7, 0x98, 0xE7, 0x9A)   # layers 1, 2 off
c(0x31, 0xC0, 0xE7, 0x9C)        # layer 3 on, VRAM 0
c(0xB8, 0x00, 0xF0, 0x8E, 0xD8)  # ds = f000
c(0xBB, 0x00, 0x88)              # bx = 8800 (palette buffer)
c(0xB9, 0x10, 0x00)              # cx = 16
c(0x31, 0xC0)                    # ax = 0
loop1 = len(code)
c(0x89, 0x07)                    # mov [bx],ax
c(0x05, 0x42, 0x08)              # add ax,0842 (r+2 g+2 b+2)
c(0x83, 0xC3, 0x02)              # add bx,2
c(0xE2, (loop1 - (len(code) + 2)) & 0xff)   # loop
c(0xA3, 0x08, 0x90)              # mov [9008],ax: start DMA (palette + sprites)
c(0xB8, 0x00, 0xD0, 0x8E, 0xD8)  # ds = d000 (VRAM)
c(0x31, 0xDB)                    # bx = 0
c(0xB9, 0x00, 0x10)              # cx = 4096 tiles
loop2 = len(code)
c(0xC7, 0x07, 0x01, 0x00)        # mov word [bx],1
c(0xC7, 0x47, 0x02, 0x00, 0x00)  # mov word [bx+2],0
c(0x83, 0xC3, 0x04)              # add bx,4
c(0xE2, (loop2 - (len(code) + 2)) & 0xff)
c(0xFB)                          # sti
c(0xEB, 0xFE)                    # jmp $
put(0x100, code)

irq = []
def i(*b): irq.extend(b)
i(0x50, 0x1E)                    # push ax / push ds
i(0xB8, 0x00, 0xE0, 0x8E, 0xD8)  # ds = e000
i(0xA1, 0x00, 0x00, 0x40, 0xA3, 0x00, 0x00)   # inc word [0]
i(0xE7, 0x94)                    # layer 3 scroll x
i(0xE6, 0x00)                    # sound command
i(0xE4, 0x08)                    # in al,08: read the answer (clears it)
i(0xA2, 0x02, 0x00)              # mov [2],al
i(0xB0, 0x20, 0xE6, 0x40)        # EOI
i(0x1F, 0x58, 0xCF)              # pop ds / pop ax / iret
put(0x200, irq)
# IR3 (vector 0x23) -> same handler is fine
put(0x23 * 4, [0x00, 0x02, 0x00, 0x00])

# ---------------------------------------------------------------- sound CPU
snd = bytearray(0x20000)
snd[0x1fff0:0x1fff5] = bytes([enc(0xEA), 0x00, 0x01, 0x00, 0x00])
sc = []
def s_(op, *rest): sc.extend([enc(op)] + list(rest))
s_(0xB8, 0x00, 0xA8); s_(0x8E, 0xD8)                  # ds = a800
regs = [(0x20, 0xC7), (0x28, 0x4A), (0x30, 0x00)]
for op in range(4):
    o = op * 8
    regs += [(0x40 + o, 0x01), (0x60 + o, 0x18), (0x80 + o, 0x1F), (0xA0 + o, 0x00), (0xC0 + o, 0x00), (0xE0 + o, 0x0F)]
regs += [(0x08, 0x78)]
for r, v in regs:
    s_(0xC6, 0x06, 0x40, 0x00, r)                   # mov byte [0040],reg
    s_(0xC6, 0x06, 0x42, 0x00, v)                   # mov byte [0042],val
# answer loop: wait for a command (read a8044), echo it to a8046, acknowledge
lp = len(sc)
s_(0xA0, 0x44, 0x00)                                 # mov al,[0044]
s_(0xA2, 0x46, 0x00)                                 # mov [0046],al
s_(0xA2, 0x44, 0x00)                                 # mov [0044],al (acknowledge)
# busy wait ~ a while
s_(0xB9, 0x00, 0x40)                                 # mov cx,4000
w = len(sc)
s_(0xE2, (w - (len(sc) + 2)) & 0xff)                 # loop $
s_(0xEB, (lp - (len(sc) + 2)) & 0xff)                # jmp lp
snd[0x100:0x100 + len(sc)] = bytes(sc)

# ---------------------------------------------------------------- graphics
gfx1 = bytearray(0x200000)
q = len(gfx1) // 4
for y in range(8):
    for p in range(4):
        b = 0
        for x in range(8):
            pen = ((x ^ y) + (x >> 1)) & 15
            if (pen >> p) & 1:
                b |= 0x80 >> x
        gfx1[p * q + 8 + y] = b      # char 1, plane p (quarter p holds bit p)
gfx2 = bytearray(0x400000)
samples = bytearray(0x80000)

def split16(region, files):
    for name, off, size in files:
        yield name, bytes(region[off + i * 2] for i in range(size))

with zipfile.ZipFile(sys.argv[2], 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in split16(main, [('ucc_e-h0.ic28', 1, 0x40000), ('ucc_e-l0.ic39', 0, 0x40000),
                                     ('ucc_h1.ic27', 0x80001, 0x20000), ('ucc_l1.ic38', 0x80000, 0x20000)]):
        z.writestr(name, data)
    for name, data in split16(snd, [('ucc_e-sh0-.ic30', 1, 0x10000), ('ucc_e-sl0-.ic31', 0, 0x10000)]):
        z.writestr(name, data)
    for k, name in enumerate(['uc_w38m.ic26', 'uc_w39m.ic25', 'uc_w40m.ic24', 'uc_w41m.ic23']):
        z.writestr(name, bytes(gfx1[k * 0x80000:(k + 1) * 0x80000]))
    for k, name in enumerate(['uc_k16m.ic37', 'uc_k17m.ic36', 'uc_k18m.ic35', 'uc_k19m.ic34']):
        z.writestr(name, bytes(gfx2[k * 0x100000:(k + 1) * 0x100000]))
    z.writestr('uc_w42.ic9', bytes(samples))
print('wrote', sys.argv[2])
