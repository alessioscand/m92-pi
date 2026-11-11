#!/usr/bin/env python3
"""m92-pi: a tiny made-up "dynablst" set to smoke-test the M90 machine without real ROMs.

Main CPU (V35, opcodes encrypted with the Bomber Man table): sets the clock divider,
unmasks INTP0 (vertical blank), writes a palette, fills layer 1 with a patterned tile,
puts a sprite up, and in the interrupt scrolls the layer, moves the sprite and sends
the frame count to the sound CPU. Sound CPU (Z80): plays a tone on the YM2151, takes
each command (RST 18h) and plays a sawtooth sample through the DAC in its NMI.
    python3 test/make_synth90.py third_party/mame/irem_cpu.cpp out/dynablst.zip
"""
import re, sys, zipfile

src = open(sys.argv[1]).read()
m = re.search(r'bomberman_decryption_table\[256\]\s*=\s*\{(.*?)\};', src, re.S)
body = re.sub(r'/\*.*?\*/', '', m.group(1), flags=re.S)
table = [int(x, 16) if x.startswith('0x') else -1 for x in (t.strip() for t in body.split(',')) if x]
assert len(table) == 256, len(table)
inv = {}
for e, d in enumerate(table):
    inv.setdefault(d, e)

def enc(op):
    assert op in inv, hex(op)
    return inv[op]

# ---------------------------------------------------------------- main CPU (V35)
main = bytearray(0x40000)
code = []
def op(o, *rest):                # an opcode (encrypted) and its operand bytes (not)
    code.append(enc(o)); code.extend(rest)
def pre(p):                      # a prefix is fetched as an opcode too
    code.append(enc(p))

op(0xFA)                                        # di
op(0xB8, 0x00, 0xF0); op(0x8E, 0xC0)            # mov ax,f000 / mov es,ax
pre(0x26); op(0xC6, 0x06, 0xEB, 0xFF, 0x00)     # PRC = 0: clock / 2
pre(0x26); op(0xC6, 0x06, 0x4C, 0xFF, 0x07)     # EXIC0 = 7: INTP0 unmasked
op(0xB8, 0x00, 0xA0); op(0x8E, 0xD0)            # ss = a000
op(0xBC, 0xF0, 0x3F)                            # sp = 3ff0
op(0xB8, 0x00, 0xE0); op(0x8E, 0xD8)            # ds = e000 (palette)
op(0xBB, 0x00, 0x00)                            # bx = 0
op(0xB9, 0x00, 0x01)                            # cx = 256 entries (tiles and sprites)
op(0xB8, 0x00, 0x00)                            # ax = 0
loop1 = len(code)
op(0x89, 0x07)                                  # mov [bx],ax
op(0x05, 0x21, 0x04)                            # add ax,0421 (r+1 g+1 b+1)
op(0x83, 0xC3, 0x02)                            # add bx,2
op(0xE2, (loop1 - (len(code) + 2)) & 0xff)      # loop
op(0xB8, 0x00, 0xD0); op(0x8E, 0xD8)            # ds = d000 (VRAM)
op(0xBB, 0x00, 0x80)                            # bx = 8000 (page 2)
op(0xB9, 0x00, 0x10)                            # cx = 4096 tiles
loop2 = len(code)
op(0xC7, 0x07, 0x01, 0x00)                      # mov word [bx],1
op(0xC7, 0x47, 0x02, 0x03, 0x00)                # mov word [bx+2],3 (colour 3)
op(0x83, 0xC3, 0x04)                            # add bx,4
op(0xE2, (loop2 - (len(code) + 2)) & 0xff)      # loop
op(0xC7, 0x06, 0x00, 0xEE, 0x00, 0x03)          # sprite 0, word 0: y 0x100, colour 1
op(0xC7, 0x06, 0x02, 0xEE, 0x02, 0x00)          # word 1: code 2
op(0xC7, 0x06, 0x04, 0xEE, 0x00, 0x01)          # word 2: x 0x100
op(0xB8, 0x10, 0x00); op(0xE7, 0x8A)            # layer 0 off
op(0xB8, 0x02, 0x00); op(0xE7, 0x8C)            # layer 1: page 2
op(0xB8, 0x00, 0x00); op(0xE7, 0x8E)            # picture on
op(0xFB)                                        # ei
idle = len(code)
op(0xEB, (idle - (len(code) + 2)) & 0xff)       # jmp $

def place(at, c):
    main[at:at + len(c)] = bytes(c)
place(0x100, code)

code = []                                       # INTP0: vertical blank
op(0x50)                                        # push ax
op(0xFF, 0x06, 0x00, 0x30)                      # inc word [3000] (ds = d000: VRAM, unused)
op(0xA1, 0x00, 0x30)                            # mov ax,[3000]
op(0xE7, 0x86)                                  # layer 1 x scroll
op(0xA3, 0x04, 0xEE)                            # sprite x
op(0xE6, 0x00)                                  # sound command
op(0x58)                                        # pop ax
op(0x0F, 0x92)                                  # fint: end of interrupt
op(0xCF)                                        # reti
place(0x200, code)

main[0x60:0x64] = bytes([0x00, 0x02, 0x00, 0x00])   # vector 24 (INTP0) -> 0000:0200
code = []
op(0xEA, 0x00, 0x01, 0x00, 0x00)                # jmp far 0000:0100
place(0x3fff0, code)

# ---------------------------------------------------------------- sound CPU (Z80)
snd = bytearray(0x10000)
def zput(a, bs):
    snd[a:a + len(bs)] = bytes(bs)
ym = []
for r, v in ((0x20, 0xC7), (0x28, 0x4A), (0x60, 0x10), (0x68, 0x7F), (0x70, 0x7F), (0x78, 0x7F),
             (0x80, 0x1F), (0xE0, 0x0F), (0x08, 0x78)):
    ym += [0x3E, r, 0xD3, 0x00, 0x3E, v, 0xD3, 0x01]
zput(0x0000, [0xF3, 0x31, 0xFF, 0xFF, 0xED, 0x46, 0xC3, 0x00, 0x01])  # di / ld sp / im 0 / jp 0100
zput(0x0018, [0xC3, 0x00, 0x02])                                      # RST 18h: command
zput(0x0028, [0xFB, 0xC9])                                            # RST 28h: ei / ret
zput(0x0066, [0xF5, 0xDB, 0x84, 0xD3, 0x82, 0xF1, 0xED, 0x45])        # NMI: one sample
zput(0x0100, ym + [0x3E, 0x00, 0xD3, 0x80, 0xD3, 0x81,                # sample address 0
                   0xFB, 0x76, 0x18, 0xFD])                           # ei / halt / jr
zput(0x0200, [0xF5, 0xDB, 0x80, 0x32, 0x00, 0xF0, 0xD3, 0x83,         # read, keep, acknowledge
              0xE6, 0x3F, 0xC2, 0x20, 0x02,                           # every 64 commands:
              0xD3, 0x80, 0xD3, 0x81,                                 #   sample from the start
              0xF1, 0xFB, 0xC9])                                      # pop / ei / ret
zput(0x0220, [0xF1, 0xFB, 0xC9])

# ---------------------------------------------------------------- graphics and samples
gfx = bytearray(0x100000)
q = len(gfx) // 4
for y in range(8):                                   # tile 1: diagonal stripes
    row = 0
    for x in range(8):
        pen = ((x + y) // 2) & 15
        for pl in range(4):
            if pen >> pl & 1:
                gfx[pl * q + 8 + y] |= 0x80 >> x
for y in range(16):                                  # sprite 2: a filled square with a frame
    for x in range(16):
        pen = 15 if x in (0, 15) or y in (0, 15) else 9
        byte = 2 * 32 + (y if x < 8 else 16 + y)
        for pl in range(4):
            if pen >> pl & 1:
                gfx[pl * q + byte] |= 0x80 >> (x & 7)
samples = bytes(0x40 + (i * 4 & 0x7f) for i in range(0x20000))

# ---------------------------------------------------------------- the set
with zipfile.ZipFile(sys.argv[2], 'w') as z:
    z.writestr('bbm-cp1e.ic62', bytes(main[1::2]))
    z.writestr('bbm-cp0e.ic65', bytes(main[0::2]))
    z.writestr('bbm-sp.ic23', bytes(snd))
    for i in range(4):
        z.writestr('bbm-c%d.ic%d' % (i, 66 + i), bytes(gfx[i * q:(i + 1) * q]))
    z.writestr('bbm-v0.ic20', samples)
