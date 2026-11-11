"""m92-pi: the parts of the table generators shared by the M92 and M90 drivers:
MAME's INPUT_PORTS (expanded by the C preprocessor) and the names as shown in
the DIP SWITCHES panel."""
import os, re, subprocess, tempfile

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
def load_inputs(ports_src, ipt):
    """the INPUT_PORTS blocks in ports_src, with iremipt.h's macros: {name: [(keyword, rest)]}"""
    body = re.sub(r'^#(ifndef|define MAME_IREM|endif|pragma).*$', '', ipt, flags=re.M)
    with tempfile.NamedTemporaryFile('w', suffix='.c', delete=False) as f:
        f.write(defs + body + '\n' + ports_src.replace('static INPUT_PORTS_START', 'INPUT_PORTS_START'))
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

    return inputs

def resolve(inputs, name):
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
            for pn, p in resolve(inputs, rest).items():
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

