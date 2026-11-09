"""Build the plugin's sample layer set: the 15 Cymatics sounds (trimmed, tuned check) + 8 new Fusions
made by combining them. Writes 16-bit FLAC stereo files + a manifest (name, root note, measured f0, group)."""
import os, sys, json, subprocess, numpy as np, scipy.signal as sg, scipy.io.wavfile as wavio
sys.path.insert(0, os.path.dirname(__file__))
import engine as E
SR = 44100; C1 = 32.703195662574829
OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(808)

def L(name): return E.load_sample(name).copy()
def mono(x): return x.mean(1)
def st(x): return np.stack([x, x], 1) if x.ndim == 1 else x
def norm(x, db=-1.0): return x / (np.abs(x).max() + 1e-12) * E.db(db)
def fit(x, n):
    x = st(x)
    return x[:n] if len(x) >= n else np.concatenate([x, np.zeros((n - len(x), 2))])
def decay_env(n, d_ms, curve=1.2): return E.ahdr(n, a=1, d=d_ms, curve=curve)
def f0_of(x, a=0.15, b=0.6, lo=25, hi=140):
    m = mono(st(x)); a = int(a * SR); b = min(len(m), int(b * SR))
    seg = m[a:b] * np.hanning(b - a); sp = np.abs(np.fft.rfft(seg, 2 ** 19)); f = np.fft.rfftfreq(2 ** 19, 1 / SR)
    k = (f > lo) & (f < hi); return float(f[k][np.argmax(sp[k])])
def align(ref, x, maxlag=0.03):
    """Shift x so its low band lines up with ref's (maximise sum, no cancellation)."""
    lo_r = E.lp(mono(st(ref))[:int(.3 * SR)], 150); lo_x = E.lp(mono(st(x))[:int(.3 * SR)], 150)
    c = sg.correlate(lo_r, lo_x, 'full'); lags = np.arange(-len(lo_x) + 1, len(lo_r))
    m = np.abs(lags) <= int(maxlag * SR); k = lags[m][np.argmax(c[m])]
    x = st(x)
    return np.concatenate([np.zeros((k, 2)), x])[:len(x)] if k >= 0 else np.concatenate([x[-k:], np.zeros((-k, 2))])

def stft(x, n=2048, h=512):
    return sg.stft(x, SR, nperseg=n, noverlap=n - h, boundary=None, padded=True)[2]
def istft(Z, n=2048, h=512):
    return sg.istft(Z, SR, nperseg=n, noverlap=n - h, boundary=None)[1]

def spectral_envelope(mag, lifter=30):
    """Cepstral smoothing of each frame's magnitude."""
    lg = np.log(mag + 1e-9); c = np.fft.irfft(lg, axis=0)
    c[lifter:-lifter] = 0
    return np.exp(np.fft.rfft(c, axis=0).real[:mag.shape[0]])

F = {}

# 1. Heavy Reese: Heavyweight 808 front, Toronto reese growl riding the same decay, aligned low ends
n = int(4.0 * SR)
hv = fit(L('808_Heavyweight'), n); tor = fit(L('REESE_Toronto'), n)
tor = E.hp(tor, 90) * decay_env(n, 3600, 1.3)[:, None]
F['Fusion Heavy Reese'] = (hv + 0.9 * norm(tor, -3) * np.abs(hv).max(), 24)

# 2. Talking 808: cross-synthesis. Carrier = Standard 808 driven rich; its spectrum is reshaped
#    frame by frame by the Faith reese's spectral envelope -> an 808 that "speaks".
n = int(3.0 * SR)
car = mono(fit(L('808_Standard'), n)); car_rich = np.tanh(4 * car / np.abs(car).max())
mod = mono(fit(L('REESE_Faith'), n))
Zc = stft(car_rich); Zm = stft(mod)
env_m = spectral_envelope(np.abs(Zm)); env_c = spectral_envelope(np.abs(Zc))
Zx = Zc * (env_m / (env_c + 1e-9)) ** 0.85
talk = istft(Zx)[:n]
talk = E.hp(talk, 110) / (np.abs(E.hp(talk, 110)).max() + 1e-9) * 0.8 * decay_env(n, 2400, 1.2)
F['Fusion Talking 808'] = (st(E.lp(car, 110) + talk), 24)

# 3. Glass Donk: Perfect Donk played through a 250 ms slice of Eternity as an impulse response
n = int(1.8 * SR)
dk = mono(fit(L('BASS_Perfect_Donk'), n)); ir = mono(L('REESE_Eternity'))[int(1.0 * SR):int(1.25 * SR)]
ir = E.hp(ir, 150) * np.exp(-np.arange(len(ir)) / SR / 0.08); ir /= np.sqrt(np.sum(ir ** 2))
wet = sg.fftconvolve(dk, ir)[:n]; wet = wet / np.abs(wet).max() * np.abs(dk).max() * 0.6
F['Fusion Glass Donk'] = (st(dk + wet), 24)

# 4. Frozen Whale: the Whale reese frozen at one moment (magnitude held, phases moving), shaped like an 808,
#    over the Offset 808's sub and attack
n = int(3.5 * SR)
wh = mono(L('REESE_Whale')); Zw = stft(wh)
fr = Zw.shape[1] // 6; mag = np.abs(Zw[:, fr - 2:fr + 3]).mean(1, keepdims=True)
frames = int(n / 512) + 4
ph = rng.uniform(0, 2 * np.pi, (Zw.shape[0], frames))
frozen = istft(np.repeat(mag, frames, 1) * np.exp(1j * ph))[:n]
frozen = E.hp(frozen, 120) / np.abs(E.hp(frozen, 120)).max() * decay_env(len(frozen), 3000, 1.4)
off = mono(fit(L('808_Offset'), n))
F['Fusion Frozen Whale'] = (st(off) + np.stack([frozen, np.roll(frozen, 31)], 1) * 0.45 * np.abs(off).max(), 24)

# 5. Grit Stack: Reaper + Cyclical + Unstable, low ends aligned by correlation, glued with tube saturation
n = int(2.6 * SR)
rp = fit(L('BASS_Reaper'), n); cy = align(rp, fit(L('808_Cyclical'), n)); un = align(rp, fit(L('BASS_Unstable'), n))
stack = rp + 0.8 * cy + 0.5 * E.hp(un, 100)
stack = stack / np.abs(stack).max()
sat = np.tanh(2.2 * stack + 0.3 * stack ** 2); sat -= sat.mean(0)
F['Fusion Grit Stack'] = (E.lp(stack, 100) + E.hp(sat, 100), 24)

# 6. Rum Stretch: the short Rum 808 granular-stretched 3x with its pitch kept: the same grit, a long tail
rum = mono(L('808_Rum'))
head = int(0.06 * SR)                                   # keep the real attack untouched
g = int(0.09 * SR); hop_out = int(0.03 * SR); stretch = 3.0
n_out = int((len(rum) - head) * stretch); y = np.zeros(n_out + g); w = np.hanning(g)
f0r = f0_of(rum); per = SR / f0r
for i, pos_o in enumerate(range(0, n_out, hop_out)):
    pos_i = head + int(pos_o / stretch)
    pos_i = int(round(pos_i / per) * per)               # grains start on whole cycles: no phase smear
    if pos_i + g >= len(rum): break
    y[pos_o:pos_o + g] += rum[pos_i:pos_i + g] * w
y = y[:n_out] / 1.5 * decay_env(n_out, 2600, 1.1)
k = int(0.01 * SR)
tail = y; tail[:k] *= np.linspace(0, 1, k)
F['Fusion Rum Stretch'] = (st(np.concatenate([rum[:head], np.zeros(0)]) if False else np.concatenate([rum[:head] * np.r_[np.ones(head - k), np.linspace(1, 0, k)], np.zeros(0)])), 24)
full = np.zeros(head + n_out); full[:head] = rum[:head] * np.r_[np.ones(head - k), np.linspace(1, 0, k)]
full[head - k:head - k + n_out] += tail[:len(full) - (head - k)]
F['Fusion Rum Stretch'] = (st(full), 24)

# 7. Brooding Ring: Brooding bass ring-modulated at a fifth above, blended back: metal around a solid note
br = L('BASS_Brooding')[:int(3 * SR)]
t = np.arange(len(br)) / SR
ring = E.hp(br, 120) * np.sin(2 * np.pi * C1 * 2 ** (19 / 12) * t)[:, None]
F['Fusion Brooding Ring'] = (br + 0.6 * ring, 36)

# 8. Eternity Pluck: the endless Eternity reese forced into an 808's shape: pitch dives into the note,
#    the level falls like an 808, plus a sine sub under it
n = int(2.8 * SR)
et = L('REESE_Eternity')[int(0.05 * SR):]
pc = E.pitch_curve(n, 0, 9, 45)
etp = E.warp_read(et, 2 ** (pc / 12)) * decay_env(n, 2600, 1.3)[:, None]
sub = np.sin(2 * np.pi * np.cumsum(C1 * 2 ** (pc / 12)) / SR) * decay_env(n, 2600, 1.3)
F['Fusion Eternity Pluck'] = (E.hp(etp, 100) + 0.8 * st(sub) * np.abs(etp).max(), 24)

# ------------------------------------------------------------------ write everything
manifest = []
def finish(x, maxlen=6.0):
    x = st(x); x = x - x.mean(0)
    x = x[:int(maxlen * SR)]
    env = np.abs(x).max(1); on = max(0, int(np.argmax(env > 0.02 * env.max())) - 4); x = x[on:]
    pk = np.abs(x).max(); above = np.where(np.abs(x).max(1) > pk * E.db(-70))[0]
    x = x[:above[-1] + 1] if len(above) else x
    fd = min(len(x) // 4, int(0.25 * SR)) if len(x) > maxlen * SR * 0.95 else int(0.01 * SR)
    x[-fd:] *= np.linspace(1, 0, fd)[:, None]
    return norm(x, -1.0)

def save(name, x, root, group):
    x = finish(x)
    lo, hi = (25, 70) if root == 24 else (50, 140)
    f0 = f0_of(x, .15, min(.6, len(x) / SR), lo, hi)
    tmp = os.path.join(OUT, '_tmp.wav'); wavio.write(tmp, SR, (np.clip(x, -1, 1) * 32767).astype(np.int16))
    fn = name.replace(' ', '_') + '.flac'
    subprocess.run(['ffmpeg', '-nostdin', '-loglevel', 'error', '-y', '-i', tmp, '-c:a', 'flac', '-compression_level', '8',
                    os.path.join(OUT, fn)], check=True)
    os.remove(tmp)
    manifest.append(dict(name=name, file=fn, root=root, f0=round(f0, 3), group=group, seconds=round(len(x) / SR, 2)))
    print(f'{name:26s} root={root} f0={f0:6.2f}Hz ({1200*np.log2(f0/(C1*2**((root-24)/12))):+.0f}c) len={len(x)/SR:.2f}s')

ROOTS = {'BASS_Brooding': 36, 'BASS_Unstable': 36, 'REESE_Faith': 36, 'REESE_Punchy': 36, 'REESE_Toronto': 36}
for nm in ['808_Cyclical', '808_Heavyweight', '808_Offset', '808_Rum', '808_Short', '808_Standard',
           'BASS_Brooding', 'BASS_Perfect_Donk', 'BASS_Reaper', 'BASS_Unstable',
           'REESE_Eternity', 'REESE_Faith', 'REESE_Punchy', 'REESE_Toronto', 'REESE_Whale']:
    kind, rest = nm.split('_', 1)
    disp = {'808': '808', 'BASS': 'Bass', 'REESE': 'Reese'}[kind] + ' ' + rest.replace('_', ' ')
    save(disp, L(nm), ROOTS.get(nm, 24), {'808': '808s', 'BASS': 'Basses', 'REESE': 'Reeses'}[kind])
for k, (x, root) in F.items():
    save(k, x, root, 'Fusions')
json.dump(manifest, open(os.path.join(OUT, 'manifest.json'), 'w'), indent=1)
print(sum(os.path.getsize(os.path.join(OUT, m['file'])) for m in manifest) / 1e6, 'MB total')
