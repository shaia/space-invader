"""Generate the app icons from the in-game 'crab' invader sprite.

This script is the source of truth for every platform's icon art:
    res/app.ico                         Windows  (embedded via app.rc)
    res/app.icns                        macOS    (bundle Resources/)
    res/linux/hicolor/<s>x<s>/apps/...  Linux    (hicolor icon theme)
Re-run after tweaking to regenerate all of them. Requires Pillow.

    python res/make_icon.py
"""
import io
import os
import struct

from PIL import Image, ImageDraw, ImageFilter

# Classic crab invader (config.h kCrab frame 0): 8 cols x 6 rows.
CRAB = [
    "..#..#..",
    "#.####.#",
    "########",
    "########",
    ".##..##.",
    "##....##",
]

# Game palette (config.h): deep space bg + hot-magenta accent.
BG_TOP    = (14, 12, 38)
BG_BOT    = (4, 5, 16)
INVADER   = (255, 60, 190)   # kColAccent
GLINT     = (255, 200, 240)
EYE       = (20, 30, 60)


def render(size, ss=None):
    """Render one square icon frame at `size` px (supersampled by `ss`)."""
    if ss is None:
        ss = 4 if size <= 128 else 2  # big frames need less AA, stay fast
    S = size * ss
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # --- rounded-rect deep-space plate with vertical gradient ---
    col = Image.new("RGBA", (1, S))  # build one column, stretch to full width
    cl = col.load()
    for y in range(S):
        t = y / (S - 1)
        cl[0, y] = (
            int(BG_TOP[0] + (BG_BOT[0] - BG_TOP[0]) * t),
            int(BG_TOP[1] + (BG_BOT[1] - BG_TOP[1]) * t),
            int(BG_TOP[2] + (BG_BOT[2] - BG_TOP[2]) * t),
            255,
        )
    grad = col.resize((S, S))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
    img.paste(grad, (0, 0), mask)

    # soft center glow behind the invader
    glow_c = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow_c).ellipse(
        [S * 0.18, S * 0.18, S * 0.82, S * 0.82], fill=(120, 30, 90, 120))
    glow_c = glow_c.filter(ImageFilter.GaussianBlur(S * 0.10))
    img.alpha_composite(Image.composite(glow_c, Image.new("RGBA", (S, S)), mask))

    # --- invader geometry: 8x6 grid centred, with margin ---
    cols, rows = 8, 6
    cell = S * 0.088
    gw, gh = cols * cell, rows * cell
    x0 = (S - gw) / 2
    y0 = (S - gh) / 2

    def draw_cells(canvas, col, inset=0.0):
        cd = ImageDraw.Draw(canvas)
        for ry, line in enumerate(CRAB):
            for cx, ch in enumerate(line):
                if ch != "#":
                    continue
                px = x0 + cx * cell
                py = y0 + ry * cell
                cd.rectangle([px + inset, py + inset,
                              px + cell - inset, py + cell - inset], fill=col)

    # glow pass: blurred magenta body
    glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    draw_cells(glow, INVADER + (255,))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.035))
    img.alpha_composite(glow)

    # crisp body with a lit top / darker bottom (matches DrawInvaderArt shading)
    body = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(body)
    for ry, line in enumerate(CRAB):
        shade = 1.18 - 0.4 * (ry / (rows - 1))
        col = tuple(min(255, int(c * shade)) for c in INVADER) + (255,)
        for cx, ch in enumerate(line):
            if ch != "#":
                continue
            px = x0 + cx * cell
            py = y0 + ry * cell
            bd.rectangle([px, py, px + cell - max(1, ss), py + cell - max(1, ss)], fill=col)
    img.alpha_composite(body)

    # eyes: two dark sockets on row 2 with a bright glint
    for ex in (2.0, 5.0):
        px = x0 + ex * cell
        py = y0 + 2 * cell
        d.rectangle([px, py, px + cell - ss, py + 2 * cell - ss], fill=EYE + (255,))
        d.ellipse([px + cell * 0.15, py + cell * 0.2,
                   px + cell * 0.75, py + cell * 0.8], fill=GLINT + (255,))

    return img.resize((size, size), Image.LANCZOS)


# macOS .icns OSType per pixel size (PNG-encoded entries, read by modern macOS).
ICNS_TYPES = {16: b"icp4", 32: b"icp5", 64: b"ic12",
              128: b"ic07", 256: b"ic08", 512: b"ic09", 1024: b"ic10"}


def write_icns(path, frames):
    """Pack {size: Image} into an .icns file (PNG entries)."""
    body = b""
    for size in sorted(frames):
        buf = io.BytesIO()
        frames[size].save(buf, format="PNG")
        data = buf.getvalue()
        body += ICNS_TYPES[size] + struct.pack(">I", len(data) + 8) + data
    with open(path, "wb") as f:
        f.write(b"icns" + struct.pack(">I", len(body) + 8) + body)


def main():
    here = os.path.dirname(os.path.abspath(__file__))

    # Windows .ico
    win = [256, 128, 64, 48, 32, 16]
    wf = {s: render(s) for s in win}
    ico = os.path.join(here, "app.ico")
    wf[256].save(ico, format="ICO", sizes=[(s, s) for s in win],
                 append_images=[wf[s] for s in win[1:]])
    print("wrote", ico, win)

    # macOS .icns
    mac = [16, 32, 64, 128, 256, 512, 1024]
    icns = os.path.join(here, "app.icns")
    write_icns(icns, {s: render(s) for s in mac})
    print("wrote", icns, mac)

    # Linux hicolor PNGs
    lin = [16, 32, 48, 64, 128, 256, 512]
    for s in lin:
        d = os.path.join(here, "linux", "hicolor", f"{s}x{s}", "apps")
        os.makedirs(d, exist_ok=True)
        render(s).save(os.path.join(d, "space_invader_plus.png"), format="PNG")
    print("wrote", os.path.join(here, "linux", "hicolor"), lin)


if __name__ == "__main__":
    main()
