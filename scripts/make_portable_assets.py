#!/usr/bin/env python3
"""Decode the GX textures in data/ into portable linear images.

The Wii build uses the tiled GX textures in data/*.bin directly. Every other
platform needs plain pixels, so this writes, into assets/portable/:

  data_portable/*.bin  the same textures as raw linear bytes (SDL builds)
  body.png      512x512 RGBA plastic shade map (tinted at runtime)
  detail.png    512x512 RGBA bearings, untinted
  font_small.png / font_large.png  white glyph atlases, alpha = coverage
  ds_*.bin      reduced textures for the Nintendo DS 3D engine

Requires Pillow.
"""
import re
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
OUT = ROOT / "assets" / "portable"
RAW = ROOT / "data_portable"


def atlas_sizes():
    text = (ROOT / "source" / "common" / "font_data.h").read_text()
    sizes = {}
    for name in ("small", "large"):
        m = re.search(r"font_%s_info\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)" % name, text)
        sizes[name] = (int(m.group(4)), int(m.group(5)))
    return sizes


def decode_i8(buf, w, h):
    px = bytearray(w * h)
    i = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 8):
            for y in range(4):
                for x in range(8):
                    px[(by + y) * w + bx + x] = buf[i]
                    i += 1
    return px


def decode_ia8(buf, w, h):
    px = bytearray(w * h * 2)  # I, A
    i = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            for y in range(4):
                for x in range(4):
                    o = ((by + y) * w + bx + x) * 2
                    px[o + 1] = buf[i]      # alpha comes first in the file
                    px[o] = buf[i + 1]
                    i += 2
    return px


def decode_rgba8(buf, w, h):
    px = bytearray(w * h * 4)
    i = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            for y in range(4):
                for x in range(4):
                    o = ((by + y) * w + bx + x) * 4
                    px[o + 3] = buf[i]       # A
                    px[o] = buf[i + 1]       # R
                    i += 2
            for y in range(4):
                for x in range(4):
                    o = ((by + y) * w + bx + x) * 4
                    px[o + 1] = buf[i]       # G
                    px[o + 2] = buf[i + 1]   # B
                    i += 2
    return px


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    RAW.mkdir(parents=True, exist_ok=True)
    sizes = atlas_sizes()

    body = decode_ia8((DATA / "body.bin").read_bytes(), 512, 512)
    body_im = Image.frombytes("LA", (512, 512), bytes(body)).convert("RGBA")
    body_im.save(OUT / "body.png")
    (RAW / "body_ia.bin").write_bytes(bytes(body))

    detail = decode_rgba8((DATA / "detail.bin").read_bytes(), 512, 512)
    detail_im = Image.frombytes("RGBA", (512, 512), bytes(detail))
    detail_im.save(OUT / "detail.png")
    (RAW / "detail_rgba.bin").write_bytes(bytes(detail))

    for name in ("small", "large"):
        w, h = sizes[name]
        i8 = decode_i8((DATA / f"font_{name}.bin").read_bytes(), w, h)
        im = Image.new("RGBA", (w, h), (255, 255, 255, 0))
        im.putalpha(Image.frombytes("L", (w, h), bytes(i8)))
        im.save(OUT / f"font_{name}.png")
        (RAW / f"font_{name}_a8.bin").write_bytes(bytes(i8))
        print(name, w, h)

    make_t3x()
    make_nds(body_im, detail_im)


def make_t3x():
    """Nintendo 3DS textures (needs tex3ds from devkitPro; skipped without it)."""
    import shutil
    import subprocess

    tex3ds = shutil.which("tex3ds")
    if not tex3ds:
        print("tex3ds not found, skipping data_3ds")
        return
    out = ROOT / "data_3ds"
    out.mkdir(exist_ok=True)
    for name in ("body", "detail", "font_small", "font_large"):
        subprocess.run([tex3ds, "-f", "rgba8888", "-z", "auto", str(OUT / f"{name}.png"),
                        "-o", str(out / f"{name}.t3x")], check=True)



def make_nds(body_im, detail_im):
    """Nintendo DS 3D-engine textures: 128x128 A3I5 shade map + RGBA5551 bearings."""
    out = ROOT / "data_nds"
    out.mkdir(exist_ok=True)
    size = 128

    def shrink(im):
        return im.convert("RGBa").resize((size, size), Image.LANCZOS).convert("RGBA")

    body = shrink(body_im)
    data = bytearray()
    for r, g, b, a in body.getdata():
        data.append(((a >> 5) << 5) | (r >> 3))   # alpha in the top 3 bits, shade index below
    (out / "body_a3i5.bin").write_bytes(bytes(data))
    pal = bytearray()
    for i in range(32):
        pal += ((i << 10) | (i << 5) | i).to_bytes(2, "little")
    (out / "body_pal.bin").write_bytes(bytes(pal))

    detail = shrink(detail_im)
    data = bytearray()
    for r, g, b, a in detail.getdata():
        v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | (0x8000 if a >= 128 else 0)
        data += v.to_bytes(2, "little")
    (out / "detail_rgba5551.bin").write_bytes(bytes(data))


if __name__ == "__main__":
    main()
