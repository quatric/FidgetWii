#!/usr/bin/env python3
"""Convert assets/spinner.png into GX textures for FidgetWii.

The photo is split into two layers that are drawn on top of each other:

  data/body.bin    IA8, 512x512  - the plastic body as a grey shade map. It is
                                   multiplied by the selected color at runtime.
  data/detail.bin  RGBA8, 512x512 - bearings and anything that is not plastic,
                                   drawn untinted over the body.

The image is re-centred on the spinner's centre of mass so it rotates around
the middle cap instead of wobbling. Requires Pillow.
"""
import colorsys
import math
import sys
from pathlib import Path

from PIL import Image

SIZE = 512
ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "assets" / "spinner.png"
OUT = ROOT / "data"


def smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3 - 2 * t)


def recentre(im):
    w, h = im.size
    alpha = im.getchannel("A").load()
    total = sx = sy = 0.0
    for y in range(h):
        for x in range(w):
            a = alpha[x, y]
            if a:
                total += a
                sx += a * x
                sy += a * y
    cx, cy = sx / total, sy / total
    radius = 0.0
    for y in range(h):
        for x in range(w):
            if alpha[x, y] > 8:
                radius = max(radius, math.hypot(x - cx, y - cy))
    radius += 4
    box = (round(cx - radius), round(cy - radius), round(cx + radius), round(cy + radius))
    print(f"centre ({cx:.1f}, {cy:.1f}) radius {radius:.1f}")
    return im.crop(box).resize((SIZE, SIZE), Image.LANCZOS)


def body_mask(r, g, b):
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    # The source spinner is yellow plastic; hue ~ 0.15
    hue_dist = min(abs(h - 0.15), 1 - abs(h - 0.15))
    return smoothstep(0.25, 0.55, s) * (1 - smoothstep(0.06, 0.12, hue_dist)) * smoothstep(0.15, 0.45, v), v


def tile(width, height, bw, bh, pixel_bytes):
    """Yield pixel coordinates in GX block order."""
    for ty in range(0, height, bh):
        for tx in range(0, width, bw):
            yield [(tx + x, ty + y) for y in range(bh) for x in range(bw)]


def main():
    im = recentre(Image.open(SRC).convert("RGBA"))
    px = im.load()

    body = {}
    detail = {}
    for y in range(SIZE):
        for x in range(SIZE):
            r, g, b, a = px[x, y]
            m, v = body_mask(r, g, b)
            # Plastic shading normalised so the flat body colour maps to white
            shade = min(255, round(v * 255 / 0.98))
            body[x, y] = (a, shade)
            detail[x, y] = (round(a * (1 - m)), r, g, b)

    OUT.mkdir(exist_ok=True)

    ia8 = bytearray()
    for block in tile(SIZE, SIZE, 4, 4, 2):
        for p in block:
            ia8 += bytes(body[p])
    (OUT / "body.bin").write_bytes(ia8)

    rgba8 = bytearray()
    for block in tile(SIZE, SIZE, 4, 4, 4):
        ar = bytearray()
        gb = bytearray()
        for p in block:
            a, r, g, b = detail[p]
            ar += bytes((a, r))
            gb += bytes((g, b))
        rgba8 += ar + gb
    (OUT / "detail.bin").write_bytes(rgba8)

    if "--preview" in sys.argv:
        prev = Image.new("RGBA", (SIZE * 2, SIZE), (40, 40, 60, 255))
        bl = Image.new("RGBA", (SIZE, SIZE))
        dl = Image.new("RGBA", (SIZE, SIZE))
        for (x, y), (a, s) in body.items():
            bl.putpixel((x, y), (round(s * 0.2), round(s * 0.5), s, a))
        for (x, y), (a, r, g, b) in detail.items():
            dl.putpixel((x, y), (r, g, b, a))
        left = Image.alpha_composite(Image.new("RGBA", (SIZE, SIZE), (40, 40, 60, 255)), bl)
        left = Image.alpha_composite(left, dl)
        prev.paste(left, (0, 0))
        prev.paste(Image.alpha_composite(Image.new("RGBA", (SIZE, SIZE), (40, 40, 60, 255)), dl), (SIZE, 0))
        prev.save(OUT.parent / "build_preview.png")


if __name__ == "__main__":
    main()
