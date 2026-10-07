#!/usr/bin/env python3
"""
EchoLine - source asset generator.

Creates importable source assets (PNG textures and WAV sound effects) using only the
Python standard library, so they can be regenerated anywhere without art tools.

These are *source* files. Unreal imports them and stores the result as .uasset inside the
project; that import step must happen in the Unreal Editor (it cannot run headless here).

Usage:
    python3 Tooling/generate_assets.py

Output:
    ArtSource/Textures/*.png
    ArtSource/Audio/*.wav
    ArtSource/README.md
"""

import math
import os
import random
import struct
import wave
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX_DIR = os.path.join(ROOT, "ArtSource", "Textures")
AUD_DIR = os.path.join(ROOT, "ArtSource", "Audio")


# --------------------------------------------------------------------------------------
# PNG writer (no third-party dependencies)
# --------------------------------------------------------------------------------------
def write_png(path, width, height, rgb_rows):
    """rgb_rows: list of rows, each a list of (r,g,b) tuples."""
    raw = bytearray()
    for row in rgb_rows:
        raw.append(0)  # filter type 0 (None)
        for (r, g, b) in row:
            raw += bytes((r & 0xFF, g & 0xFF, b & 0xFF))

    def chunk(tag, data):
        out = struct.pack(">I", len(data)) + tag + data
        out += struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        return out

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    with open(path, "wb") as f:
        f.write(png)


def clamp8(v):
    return max(0, min(255, int(v)))


def mix(a, b, t):
    return tuple(clamp8(a[i] + (b[i] - a[i]) * t) for i in range(3))


def noise(rng, amount):
    return rng.uniform(-amount, amount)


# --------------------------------------------------------------------------------------
# Texture generators
# --------------------------------------------------------------------------------------
def operator_texture(seed, base, accent, camo):
    """Base-colour texture with subtle fabric noise, camo blotches and accent trim."""
    rng = random.Random(seed)
    w = h = 256
    rows = []

    blotches = [(rng.uniform(0, 1), rng.uniform(0, 1), rng.uniform(0.06, 0.16)) for _ in range(14)]

    for y in range(h):
        row = []
        for x in range(w):
            u, v = x / w, y / h
            c = base
            if camo:
                for (bx, by, br) in blotches:
                    if (u - bx) ** 2 + (v - by) ** 2 < br * br:
                        c = mix(base, accent, 0.55)
                        break
            n = noise(rng, 9)
            r, g, b = c
            # Thin accent trim near the top and bottom edges.
            if y < 6 or y > h - 6:
                r, g, b = accent
            row.append((clamp8(r + n), clamp8(g + n), clamp8(b + n)))
        rows.append(row)

    return rows


def metal_texture(seed):
    """Dark, worn gunmetal with vertical wear streaks."""
    rng = random.Random(seed)
    w = h = 256
    base = (46, 48, 52)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            c = base
            # Horizontal wear lines.
            if rng.random() < 0.03:
                c = mix(base, (110, 112, 118), rng.uniform(0.2, 0.6))
            n = noise(rng, 7)
            row.append((clamp8(c[0] + n), clamp8(c[1] + n), clamp8(c[2] + n)))
        rows.append(row)
    return rows


def panel_texture(seed):
    """Dark technical panel with glowing cyan circuit lines (used as an emissive mask)."""
    rng = random.Random(seed)
    w = h = 256
    dark = (6, 14, 22)
    glow = (60, 220, 255)
    rows = [[dark for _ in range(w)] for _ in range(h)]

    def line(x0, y0, x1, y1, width, bright):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            t = i / steps
            x = int(x0 + (x1 - x0) * t)
            y = int(y0 + (y1 - y0) * t)
            for dy in range(-width, width + 1):
                for dx in range(-width, width + 1):
                    px, py = x + dx, y + dy
                    if 0 <= px < w and 0 <= py < h:
                        fall = 1.0 - min(1.0, math.hypot(dx, dy) / (width + 1))
                        rows[py][px] = mix(rows[py][px], glow, bright * fall)

    for _ in range(9):
        x0 = rng.randrange(20, w - 20)
        y0 = rng.randrange(20, h - 20)
        x1 = rng.randrange(20, w - 20)
        y1 = rng.randrange(20, h - 20)
        line(x0, y0, x0, y1, 1, 0.9)
        line(x0, y1, x1, y1, 1, 0.9)
    # A few nodes.
    for _ in range(6):
        x = rng.randrange(24, w - 24)
        y = rng.randrange(24, h - 24)
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                if 0 <= x + dx < w and 0 <= y + dy < h:
                    rows[y + dy][x + dx] = mix(dark, glow, 0.9)
    return rows


def icon_medkit():
    w = h = 64
    bg = (18, 92, 44)
    cross = (235, 245, 238)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            c = bg
            if 22 <= x < 42 and 12 <= y < 52:
                if 28 <= x < 36:
                    c = cross
            if 12 <= x < 52 and 26 <= y < 38:
                if 26 <= y < 34:
                    c = cross
            row.append(c)
        rows.append(row)
    return rows


def icon_ammo():
    w = h = 64
    bg = (70, 52, 16)
    bullet = (240, 200, 90)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            c = bg
            # Three stacked bullets.
            for i, bx in enumerate((14, 28, 42)):
                if bx <= x < bx + 8 and 14 <= y < 50:
                    c = bullet
                    # Pointed tip.
                    if y < 22:
                        spread = (22 - y) // 2
                        if not (bx + spread <= x < bx + 8 - spread):
                            c = bg
            row.append(c)
        rows.append(row)
    return rows


def core_texture(seed):
    """Glowing energy core for anchors and pickups (emissive)."""
    rng = random.Random(seed)
    w = h = 128
    center = (w / 2, h / 2)
    hot = (90, 220, 255)
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            d = math.hypot(x - center[0], y - center[1]) / (w / 2)
            t = max(0.0, 1.0 - d)
            c = mix((4, 10, 18), hot, t ** 1.5)
            n = noise(rng, 5)
            row.append((clamp8(c[0] + n), clamp8(c[1] + n), clamp8(c[2] + n)))
        rows.append(row)
    return rows


# --------------------------------------------------------------------------------------
# WAV generators
# --------------------------------------------------------------------------------------
SAMPLE_RATE = 44100


def write_wav(path, samples):
    """samples: list of floats in [-1, 1]."""
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for s in samples:
            v = int(max(-1.0, min(1.0, s)) * 32767)
            frames += struct.pack("<h", v)
        f.writeframes(bytes(frames))


def lowpass(samples, alpha):
    """Simple one-pole low-pass; alpha in (0,1], smaller = more filtering."""
    out = []
    prev = 0.0
    for s in samples:
        prev += alpha * (s - prev)
        out.append(prev)
    return out


def gunshot(tau, thump_hz, seed, drive=1.0):
    rng = random.Random(seed)
    duration = min(0.5, tau * 6.0)
    n = int(SAMPLE_RATE * duration)
    samples = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t / tau)
        crack = rng.uniform(-1, 1) * env
        body = math.sin(2 * math.pi * thump_hz * t) * math.exp(-t / (tau * 1.6))
        s = (crack * 0.7 + body * 0.9) * drive
        samples.append(s)
    samples = lowpass(samples, 0.55)
    peak = max(1e-6, max(abs(s) for s in samples))
    return [s / peak * 0.95 for s in samples]


def click(seed, hz=1800, dur=0.03):
    rng = random.Random(seed)
    n = int(SAMPLE_RATE * dur)
    samples = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t / (dur * 0.25))
        s = (rng.uniform(-1, 1) * 0.5 + math.sin(2 * math.pi * hz * t) * 0.5) * env
        samples.append(s)
    return samples


def beep(hz, dur, seed=0):
    n = int(SAMPLE_RATE * dur)
    samples = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t / (dur * 0.4))
        samples.append(math.sin(2 * math.pi * hz * t) * env * 0.8)
    return samples


def chirp(f0, f1, dur):
    n = int(SAMPLE_RATE * dur)
    samples = []
    phase = 0.0
    for i in range(n):
        t = i / SAMPLE_RATE
        f = f0 + (f1 - f0) * (t / dur)
        phase += 2 * math.pi * f / SAMPLE_RATE
        env = math.sin(math.pi * (t / dur))  # fade in/out
        samples.append(math.sin(phase) * env * 0.7)
    return samples


def footstep(seed):
    rng = random.Random(seed)
    dur = 0.09
    n = int(SAMPLE_RATE * dur)
    samples = []
    for i in range(n):
        t = i / SAMPLE_RATE
        env = math.exp(-t / (dur * 0.3))
        samples.append(rng.uniform(-1, 1) * env * 0.5)
    return lowpass(samples, 0.25)


# --------------------------------------------------------------------------------------
# Build everything
# --------------------------------------------------------------------------------------
def main():
    os.makedirs(TEX_DIR, exist_ok=True)
    os.makedirs(AUD_DIR, exist_ok=True)

    # --- Operator base-colour textures (match the design doc palettes) ---
    operators = {
        "T_Rook_BaseColor.png": dict(seed=101, base=(70, 82, 98), accent=(224, 228, 232), camo=False),
        "T_Wisp_BaseColor.png": dict(seed=202, base=(52, 74, 56), accent=(34, 52, 38), camo=True),
        "T_Patch_BaseColor.png": dict(seed=303, base=(126, 104, 74), accent=(232, 130, 40), camo=True),
        "T_Cairn_BaseColor.png": dict(seed=404, base=(198, 202, 200), accent=(120, 130, 132), camo=False),
    }
    for name, cfg in operators.items():
        write_png(os.path.join(TEX_DIR, name), 256, 256,
                  operator_texture(cfg["seed"], cfg["base"], cfg["accent"], cfg["camo"]))
        print("texture", name)

    # --- Weapon and environment textures ---
    write_png(os.path.join(TEX_DIR, "T_Gunmetal_BaseColor.png"), 256, 256, metal_texture(777))
    write_png(os.path.join(TEX_DIR, "T_EchoPanel_Emissive.png"), 256, 256, panel_texture(888))
    write_png(os.path.join(TEX_DIR, "T_EchoPanel_BaseColor.png"), 256, 256,
              [[(10, 18, 28) for _ in range(256)] for _ in range(256)])
    write_png(os.path.join(TEX_DIR, "T_Core_Emissive.png"), 128, 128, core_texture(999))
    write_png(os.path.join(TEX_DIR, "UI_Medkit.png"), 64, 64, icon_medkit())
    write_png(os.path.join(TEX_DIR, "UI_Ammo.png"), 64, 64, icon_ammo())
    print("texture T_Gunmetal_BaseColor.png")
    print("texture T_EchoPanel_Emissive.png")
    print("texture T_EchoPanel_BaseColor.png")
    print("texture T_Core_Emissive.png")
    print("texture UI_Medkit.png")
    print("texture UI_Ammo.png")

    # --- Weapons: each firearm gets a distinct report ---
    write_wav(os.path.join(AUD_DIR, "SFX_Meridian_Fire.wav"), gunshot(0.080, 90.0, 11, 1.0))
    write_wav(os.path.join(AUD_DIR, "SFX_Kestrel_Fire.wav"), gunshot(0.050, 150.0, 22, 0.9))
    write_wav(os.path.join(AUD_DIR, "SFX_Lancer_Fire.wav"), gunshot(0.170, 70.0, 33, 1.15))
    write_wav(os.path.join(AUD_DIR, "SFX_Reload.wav"), click(44, 1500, 0.05))
    write_wav(os.path.join(AUD_DIR, "SFX_Hitmarker.wav"), beep(1350, 0.05))
    write_wav(os.path.join(AUD_DIR, "SFX_Kill.wav"), beep(900, 0.12))
    write_wav(os.path.join(AUD_DIR, "SFX_EchoPlace.wav"), chirp(420, 1250, 0.18))
    write_wav(os.path.join(AUD_DIR, "SFX_EchoDestroy.wav"), chirp(1200, 260, 0.15))
    write_wav(os.path.join(AUD_DIR, "SFX_Pickup.wav"), chirp(600, 1300, 0.14))
    write_wav(os.path.join(AUD_DIR, "SFX_UIClick.wav"), click(55, 2400, 0.02))
    write_wav(os.path.join(AUD_DIR, "SFX_Footstep.wav"), footstep(66))
    for name in ("SFX_Meridian_Fire.wav", "SFX_Kestrel_Fire.wav", "SFX_Lancer_Fire.wav",
                 "SFX_Reload.wav", "SFX_Hitmarker.wav", "SFX_Kill.wav", "SFX_EchoPlace.wav",
                 "SFX_EchoDestroy.wav", "SFX_Pickup.wav", "SFX_UIClick.wav", "SFX_Footstep.wav"):
        print("audio", name)

    write_readme()
    print("done")


def write_readme():
    readme = """# EchoLine source assets

These files are **source assets**, generated by `Tooling/generate_assets.py`.
They are not Unreal `.uasset` files: Unreal imports the sources below and stores its own
binary copy in `Content/`. That import happens in the Unreal Editor.

Regenerate everything with:

```
python3 Tooling/generate_assets.py
```

## Textures

| File | Intended use |
|---|---|
| `T_Rook_BaseColor.png` | Mara "Rook" Venn operator base colour |
| `T_Wisp_BaseColor.png` | Ilan "Wisp" Sato operator base colour |
| `T_Patch_BaseColor.png` | Dara "Patch" Okafor operator base colour |
| `T_Cairn_BaseColor.png` | Soren "Cairn" Vale operator base colour |
| `T_Gunmetal_BaseColor.png` | Shared weapon metal base colour |
| `T_EchoPanel_BaseColor.png` | Echo panel surface |
| `T_EchoPanel_Emissive.png` | Echo panel glowing circuit lines (emissive) |
| `T_Core_Emissive.png` | Anchor / pickup glow core (emissive) |
| `UI_Medkit.png` | Medkit HUD icon |
| `UI_Ammo.png` | Ammo HUD icon |

## Audio

| File | Intended use |
|---|---|
| `SFX_Meridian_Fire.wav` | AR-7 Meridian shot |
| `SFX_Kestrel_Fire.wav` | Kestrel-9 shot |
| `SFX_Lancer_Fire.wav` | MR-12 Lancer shot |
| `SFX_Reload.wav` | Reload start |
| `SFX_Hitmarker.wav` | Confirmed hit |
| `SFX_Kill.wav` | Confirmed elimination |
| `SFX_EchoPlace.wav` | Anchor placed |
| `SFX_EchoDestroy.wav` | Anchor destroyed |
| `SFX_Pickup.wav` | Medkit / ammo collected |
| `SFX_UIClick.wav` | Menu button press |
| `SFX_Footstep.wav` | Movement footstep |

## Importing into Unreal

1. In the Content Browser, create folders `Content/_Game/Textures` and `Content/_Game/Audio`.
2. Drag the PNGs into `Textures`. In the import dialog:
   - Set **Compression Settings** to `Default` for base colours and `UserInterface2D` for icons.
   - For `T_EchoPanel_Emissive` and `T_Core_Emissive`, keep `sRGB` **on**; they are used as emissive maps.
3. Drag the WAVs into `Audio`. Unreal converts them to Sound Wave assets.
4. Create materials in the editor and plug these textures in, or use the textures directly on
   dynamic material instances.

These assets are original, generated for this project, and carry no third-party licence.
"""
    with open(os.path.join(ROOT, "ArtSource", "README.md"), "w") as f:
        f.write(readme)


if __name__ == "__main__":
    main()
