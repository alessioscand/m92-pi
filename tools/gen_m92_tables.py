#!/usr/bin/env python3
"""m92-pi: build core/m92_games.inc from MAME's Irem M92 driver.

    python3 tools/gen_m92_tables.py <mame>/src/mame/irem > core/m92_games.inc

Reads m92.cpp (ROM_START blocks, INPUT_PORTS, machine configurations, GAME lines)
and iremipt.h, and writes plain C++ tables: ROM lists with CRCs, DIP switches with
their settings and defaults, controls and the per-game hardware choices.
The C preprocessor expands the input port macros.
"""
import os, re, subprocess, sys, tempfile

src = sys.argv[1]
drv = open(os.path.join(src, 'm92.cpp')).read()
ipt = open(os.path.join(src, 'iremipt.h')).read()

# ---------------------------------------------------------------- input ports
a = drv.index('static INPUT_PORTS_START( m92_2player )')
b = drv.index('INPUT_PORTS_END', drv.index('static INPUT_PORTS_START( gunforc2 )')) + len('INPUT_PORTS_END')
ports_src = drv[a:b].replace('static INPUT_PORTS_START', 'INPUT_PORTS_START')
defs = r'''
#define INPUT_PORTS_START(n) @START n
#define INPUT_PORTS_END @END
#define PORT_START(t) @PORT t
#define PORT_MODIFY(t) @PORT t
#define PORT_INCLUDE(n) @INCLUDE n
#define PORT_BIT(m,s,t) @BIT m s t
#define PORT_DIPNAME(m,d,n) @DIP m d n
#define PORT_DIPSETTING(v,n) @SET v n
#define DEF_STR(x) #x
#define PORT_DIPLOCATION(x) @LOC x
#define PORT_CONDITION(a,b,c,d) @COND b c d
#define PORT_DIPUNKNOWN_DIPLOC(m,d,l) @DIP m d "Unknown" @LOC l @SET d "Off" @SET 0 "On"
#define PORT_DIPUNUSED_DIPLOC(m,d,l) @DIP m d "Unknown" @LOC l @SET d "Off" @SET 0 "On"
#define PORT_SERVICE_DIPLOC(m,s,l) @DIP m m "Service_Mode" @LOC l @SET m "Off" @SET 0 "On"
#define PORT_PLAYER(n) @PLAYER n
#define PORT_8WAY
#define PORT_4WAY
#define PORT_READ_LINE_MEMBER(x)
#define PORT_NAME(x)
#define IP_ACTIVE_LOW LOW
#define IP_ACTIVE_HIGH HIGH
'''
body = re.sub(r'^#(ifndef|define MAME_IREM|endif|pragma).*$', '', ipt, flags=re.M)
with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
    f.write(defs + body + '\n' + ports_src)
    tmp = f.name
out = subprocess.run(['cpp', '-P', tmp], capture_output=True, text=True, check=True).stdout
os.unlink(tmp)
toks = out.replace('\n', ' ').split('@')[1:]

inputs = {}
cur = None
for t in toks:
    t = t.strip()
    if not t:
        continue
    kw, _, rest = t.partition(' ')
    rest = rest.strip()
    if kw == 'START':
        cur = {'name': rest, 'items': []}
        inputs[rest] = cur
    elif kw == 'END':
        cur = None
    else:
        cur['items'].append((kw, rest))

def resolve(name):
    """flatten includes; returns {port: {'bits': {mask: [type, player, state]}, 'dips': [...]}}

    A new field removes the fields it overlaps, as MAME's ioport_port::insert_field,
    unless both carry conditions that differ (coinage for coin mode 1 and coin A/B
    for coin mode 2 share the same switches)."""
    ports = {}
    port = None
    dip = None
    pending = None              # (port, new dip) waiting for its condition

    def settle():
        nonlocal pending
        if pending:
            p, d = pending
            keep = []
            for x in p['dips']:
                if x is d or not (x['mask'] & d['mask']):
                    keep.append(x)
                elif x['cond'] and d['cond'] and x['cond'] != d['cond']:
                    keep.append(x)
            p['dips'] = keep
            p['bits'] = {k: v for k, v in p['bits'].items() if not (k & d['mask'])}
            pending = None

    for kw, rest in inputs[name]['items']:
        if kw not in ('SET', 'LOC', 'COND'):
            settle()
        if kw == 'INCLUDE':
            for pn, p in resolve(rest).items():
                ports[pn] = {'bits': {k: list(v) for k, v in p['bits'].items()},
                             'dips': [dict(d, settings=list(d['settings'])) for d in p['dips']]}
        elif kw == 'PORT':
            pn = rest.strip('"')
            port = ports.setdefault(pn, {'bits': {}, 'dips': []})
        elif kw == 'BIT':
            m, s_, t = rest.split(None, 2)
            m = int(m, 16)
            port['bits'] = {k: v for k, v in port['bits'].items() if not (k & m)}
            port['dips'] = [d for d in port['dips'] if not (d['mask'] & m)]
            port['bits'][m] = [t.strip(), 0, s_]
            dip = None
        elif kw == 'PLAYER':
            last = list(port['bits'].keys())[-1]
            port['bits'][last][1] = int(rest)
        elif kw == 'DIP':
            m, d, n = re.match(r'(\S+)\s*,?\s*(\S+)\s*,?\s*(.*)', rest.replace(',', ' ')).groups()
            m = int(m, 16); d = int(d, 16)
            n = n.strip().strip('"')
            dip = {'mask': m, 'default': d, 'name': n, 'settings': [], 'cond': None, 'loc': ''}
            port['dips'].append(dip)
            pending = (port, dip)
        elif kw == 'SET':
            v, n = rest.split(None, 1)
            dip['settings'].append((int(v.strip(','), 16), n.strip().strip(',').strip().strip('"')))
        elif kw == 'LOC':
            dip['loc'] = rest.strip('"')
        elif kw == 'COND':
            b_, c_, d_ = rest.replace(',', ' ').split()
            dip['cond'] = (int(b_, 16), c_, int(d_, 16))
    settle()
    return ports

NICE = {
    'Allow_Continue': 'Allow Continue', 'Demo_Sounds': 'Demo Sounds', 'Flip_Screen': 'Flip Screen',
    'Service_Mode': 'Service Mode', 'Bonus_Life': 'Bonus Life', 'Coin_A': 'Coin A', 'Coin_B': 'Coin B',
    'Very_Easy': 'Very Easy', 'Free_Play': 'Free Play', 'Very_Hard': 'Very Hard',
    'Cabinet': 'Cabinet', 'Upright': 'Upright', 'Cocktail': 'Cocktail',
}
def nice(s):
    # shortened so that name and value fit on one line of the DIP SWITCHES panel
    if s == '700k 1500k 3000k 4500k':
        return '700K 1.5M 3M 4.5M'
    if s == 'Joystick Configuration':
        return 'Joystick Layout'
    m = re.fullmatch(r'(\d) Joysticks', s)
    if m:
        return m.group(1)
    m = re.fullmatch(r'(\d+ Points?)/1 Ticket', s)
    if m:
        return m.group(1)
    if s == 'Any Button to Start':
        return 'Any Button Start'
    if s == '2 Coins to Start/1 to Continue':
        return '2C Start/1C Cont.'
    s = NICE.get(s, s)
    m = re.fullmatch(r'(\d+)C_(\d+)C', s)
    if m:
        s = '%s Coin%s/%s Credit%s' % (m.group(1), 's' if m.group(1) != '1' else '', m.group(2), 's' if m.group(2) != '1' else '')
    return s.replace('_', ' ')

def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

# ---------------------------------------------------------------- ROMs
roms = {}
for m in re.finditer(r'ROM_START\(\s*(\w+)\s*\)(.*?)ROM_END', drv, re.S):
    name, body_ = m.group(1), m.group(2)
    regions = []
    reg = None
    for line in body_.splitlines():
        line = line.split('//')[0].strip()
        r = re.match(r'ROM_REGION\(\s*(0x[0-9a-fA-F]+)\s*,\s*"(\w+)"', line)
        if r:
            reg = {'name': r.group(2), 'size': int(r.group(1), 16), 'loads': []}
            regions.append(reg)
            continue
        r = re.match(r'(ROM_LOAD16_BYTE|ROM_LOAD)\(\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(?:CRC\((\w+)\)|NO_DUMP)', line)
        if r:
            reg['loads'].append((r.group(2), int(r.group(3), 16), int(r.group(4), 16),
                                 r.group(1) == 'ROM_LOAD16_BYTE', int(r.group(5), 16) if r.group(5) else 0))
        elif line.startswith('ROM_'):
            reg['unsupported'] = True
    roms[name] = regions

# ---------------------------------------------------------------- machines
configs = {}
for m in re.finditer(r'void m92_state::(\w+)\(machine_config &config\)\s*\{(.*?)\n\}', drv, re.S):
    configs[m.group(1)] = m.group(2)
def machine_props(cfg, seen=None):
    p = {'table': None, 'map': 'm92', 'gfx': 'm92', 'eeprom': False}
    body_ = configs.get(cfg, '')
    base = re.search(r'^\s*(\w+)\(config\);', body_, re.M)
    if base and base.group(1) in configs:
        p.update(machine_props(base.group(1)))
    t = re.search(r'set_decryption_table\((\w+)_decryption_table\)', body_)
    if t: p['table'] = t.group(1)
    mp = re.findall(r'm_maincpu->set_addrmap\(AS_PROGRAM, &m92_state::(\w+)_map\)', body_)
    if mp: p['map'] = mp[-1]
    if 'gfx_psoldier' in body_: p['gfx'] = 'psoldier'
    if 'EEPROM_2864' in body_: p['eeprom'] = True
    return p

games = []
for m in re.finditer(r'^GAME\(\s*(\d+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*([^)]*)\)', drv, re.M):
    year, name, parent, mcfg, inp, cls, init, rot, manu, desc, flags = m.groups()
    if cls != 'm92_state':
        continue              # Peter Pan and Bat Man II bootlegs: different hardware
    games.append(dict(year=year, name=name, parent=parent, mcfg=mcfg, inp=inp, init=init, rot=rot,
                      manu=manu, desc=desc, props=machine_props(mcfg)))

# ---------------------------------------------------------------- output
o = []
w = o.append
w('// Generated by tools/gen_m92_tables.py from MAME src/mame/irem/m92.cpp (BSD-3-Clause,')
w('// copyright-holders: Bryan McPhail). Do not edit by hand.')
w('')
used_inputs = sorted({g['inp'] for g in games})
for inp in used_inputs:
    ports = resolve(inp)
    dips = []
    for pn, shift in (('DSW', 0), ('COINS_DSW3', 0)):
        for d in ports.get(pn, {}).get('dips', []):
            if pn == 'COINS_DSW3' and d['mask'] < 0x100:
                continue
            dips.append((pn, d))
    def lockey(item):
        m = re.match(r'SW(\d+):(\d+)', item[1]['loc'])
        return (int(m.group(1)), int(m.group(2))) if m else (9, 9)
    dips.sort(key=lockey)
    w('static const m92_dip_setting dipset_%s[] = {' % inp)
    for pn, d in dips:
        for v, n in d['settings']:
            w('\t{ 0x%04x, %s },' % (v, cstr(nice(n))))
    w('\t{ 0, nullptr }\n};')
    w('static const m92_dip dips_%s[] = {' % inp)
    idx = 0
    for pn, d in dips:
        cond = d['cond']
        w('\t{ %d, 0x%04x, 0x%04x, %s, %s, %d, %d, 0x%04x, %d, 0x%04x },' % (
            1 if pn == 'COINS_DSW3' else 0, d['mask'], d['default'], cstr(nice(d['name'])), cstr(d['loc']),
            idx, len(d['settings']),
            cond[0] if cond else 0, (1 if cond[1] == 'EQUALS' else 2) if cond else 0, cond[2] if cond else 0))
        idx += len(d['settings'])
    w('\t{ 0, 0, 0, nullptr, nullptr, 0, 0, 0, 0, 0 }\n};')
    # controls
    nbut = 0; players = 2
    for pn in ('P1_P2', 'P3_P4'):
        for mask, (t, pl, s) in ports.get(pn, {}).get('bits', {}).items():
            mb = re.match(r'IPT_BUTTON(\d)', t)
            if mb: nbut = max(nbut, int(mb.group(1)))
            if pl: players = max(players, pl)
    dsw_def = 0xffff
    for d in ports.get('DSW', {}).get('dips', []):
        dsw_def = (dsw_def & ~d['mask']) | d['default']
    d3 = 0xff00
    for d in ports.get('COINS_DSW3', {}).get('dips', []):
        if d['mask'] >= 0x100:
            d3 = (d3 & ~d['mask']) | d['default']
    w('static const m92_inputs inputs_%s = { dips_%s, dipset_%s, 0x%04x, 0x%02x, %d, %d };\n' % (
        inp, inp, inp, dsw_def, d3 >> 8, players, nbut))

for g in games:
    regions = roms[g['name']]
    w('static const m92_rom roms_%s[] = {' % g['name'])
    for r in regions:
        if r['name'] in ('plds',):
            continue
        for fn, off, size, b16, crc in r['loads']:
            w('\t{ M92_REG_%s, %s, 0x%06x, 0x%06x, %d, 0x%08x },' % (r['name'].upper(), cstr(fn), off, size, 1 if b16 else 0, crc))
    w('\t{ 0, nullptr, 0, 0, 0, 0 }\n};')

w('static const m92_game m92_games[] = {')
for g in games:
    regs = {r['name']: r['size'] for r in roms[g['name']]}
    p = g['props']
    w('\t{ %s, %s, %s, %s, %s, roms_%s, &inputs_%s, %s, M92_MAP_%s, %d, %d, %d, { 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x } },' % (
        cstr(g['name']), cstr(g['parent'] if g['parent'] != '0' else ''), cstr(g['desc']), cstr(g['year']), cstr(g['manu']),
        g['name'], g['inp'], (p['table'] + '_decryption_table') if p['table'] else 'nullptr',
        p['map'].upper(), 1 if p['gfx'] == 'psoldier' else 0, 1 if g['init'] == 'init_bank' else 0,
        1 if p['eeprom'] else 0,
        regs.get('maincpu', 0), regs.get('soundcpu', 0), regs.get('gfx1', 0), regs.get('gfx2', 0), regs.get('irem', 0), regs.get('eeprom', 0)))
w('};')
print('\n'.join(o))
