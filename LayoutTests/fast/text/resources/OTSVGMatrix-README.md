# OTSVGMatrix-currentColor.ttf / OTSVGMatrix-baked.ttf

Test fonts for `fast/text/otsvg-currentcolor-matrix.html`, verifying that
`currentColor` reaches an [OpenType-SVG glyph](https://learn.microsoft.com/en-us/typography/opentype/spec/svg)
through every spec-compliant path
([bug 320029](https://bugs.webkit.org/show_bug.cgi?id=320029)).

Both fonts have one glyph, `A`, drawn as a 3×3 grid:

| | col 0 | col 1 | col 2 |
|---|---|---|---|
| row 0 | `currentColor` @.75 | radial green → inverse | `currentColor` @.25 |
| row 1 | linear green → 0 | **`currentColor` solid** | `var(--color1)` (= inverse) |
| row 2 | `currentColor` @.50 | `#808080` **invariant** | `currentColor` @.90 |

- **`-currentColor.ttf`** (the test) pulls the foreground / palette live.
- **`-baked.ttf`** (the reference) resolves every reference for `color:green`
  (`currentColor` → `#008000`, `--color1` → `#ff7fff`, the inverse of green).

The `--color1` cell also exercises CPAL `var()` resolution; its
`var(--color1, #ff7fff)` fallback keeps it correct without CPAL wiring.

Gradient stops are literal in both fonts — `stop-color="currentColor"` lives in
the `OTSVGGradientStops` pair instead, since no port resolves it.

## Regenerating

The script is PEP-723 compliant, so any Python tool that supports it works, e.g.
`uv run make.py`. Otherwise `pip install fonttools brotli && python3 make.py`.

```python
#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.12"
# dependencies = ["fonttools", "brotli"]
# ///
from pathlib import Path
from fontTools.colorLib.builder import buildCPAL
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable

HERE = Path(__file__).parent
GREEN = "#008000"


def inv(h):
    r, g, b = (int(h[i:i + 2], 16) for i in (1, 3, 5))
    return f"#{255 - r:02x}{255 - g:02x}{255 - b:02x}"


INV = inv(GREEN)  # #ff7fff
EM = 1000
X0, Y_TOP, CELL, GAP = 50, -950, 300, 8
CELLS = [
    (0, 0, "op", 0.75), (1, 0, "radial", None), (2, 0, "op", 0.25),
    (0, 1, "linear", None), (1, 1, "cc", None), (2, 1, "pal1", None),
    (0, 2, "op", 0.50), (1, 2, "fixed", "#808080"), (2, 2, "op", 0.90),
]


def rect(c, r):
    return X0 + c * CELL + GAP, Y_TOP + r * CELL + GAP, CELL - 2 * GAP, CELL - 2 * GAP


def cell(c, r, k, p, fg, baked):
    x, y, w, h = rect(c, r)
    box = f'x="{x}" y="{y}" width="{w}" height="{h}"'
    pal1 = INV if baked else f"var(--color1, {INV})"
    if k == "cc":
        return f'<rect {box} fill="{fg}"/>'
    if k == "op":
        return f'<rect {box} fill="{fg}" fill-opacity="{p:.2f}"/>'
    if k == "fixed":
        return f'<rect {box} fill="{p}"/>'
    if k == "linear":
        return f'<rect {box} fill="url(#lg)"/>'
    if k == "radial":
        return f'<rect {box} fill="url(#rg)"/>'
    if k == "pal1":
        return f'<rect {box} fill="{pal1}"/>'


def svg_doc(baked):
    fg = GREEN if baked else "currentColor"
    # Stops are literal in both fonts; see OTSVGGradientStops for the currentColor case.
    defs = (
        "<defs>"
        f'<linearGradient id="lg" x1="0" y1="0" x2="1" y2="0">'
        f'<stop offset="0" stop-color="{GREEN}"/>'
        f'<stop offset="1" stop-color="{GREEN}" stop-opacity="0"/></linearGradient>'
        f'<radialGradient id="rg" cx="0.5" cy="0.5" r="0.5">'
        f'<stop offset="0" stop-color="{GREEN}"/>'
        f'<stop offset="1" stop-color="{INV}"/></radialGradient>'
        "</defs>"
    )
    cells = "".join(cell(c, r, k, p, fg, baked) for c, r, k, p in CELLS)
    return ('<svg xmlns="http://www.w3.org/2000/svg" '
            'xmlns:xlink="http://www.w3.org/1999/xlink">'
            f'<g id="glyph1">{defs}{cells}</g></svg>')


def cpal():
    def t(h):
        return (int(h[1:3], 16) / 255, int(h[3:5], 16) / 255, int(h[5:7], 16) / 255, 1.0)
    return buildCPAL([[t("#0072ce"), t(INV)]])


def build(baked, family, stem):
    pen = TTGlyphPen(None)
    x0, x1 = X0, X0 + 3 * CELL
    y0, y1 = -(Y_TOP + 3 * CELL), -Y_TOP
    for i, pt in enumerate([(x0, y0), (x1, y0), (x1, y1), (x0, y1)]):
        (pen.moveTo if i == 0 else pen.lineTo)(pt)
    pen.closePath()
    glyphs = {".notdef": TTGlyphPen(None).glyph(), "A": pen.glyph()}
    fb = FontBuilder(EM, isTTF=True)
    fb.setupGlyphOrder([".notdef", "A"])
    fb.setupCharacterMap({0x41: "A"})
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({".notdef": (EM, 0), "A": (EM, x0)})
    fb.setupHorizontalHeader(ascent=1000, descent=0)
    fb.setupNameTable({"familyName": family, "styleName": "Regular",
                       "psName": family.replace(" ", "") + "-Regular"})
    fb.setupOS2(sTypoAscender=1000, sTypoDescender=0, usWinAscent=1000, usWinDescent=0)
    fb.setupPost()
    svg = newTable("SVG ")
    svg.docList = [(svg_doc(baked), 1, 1)]
    svg.colorPalettes = None
    fb.font["SVG "] = svg
    if not baked:
        fb.font["CPAL"] = cpal()
    fb.save(HERE / f"{stem}.ttf")


build(False, "OTSVG Matrix CC", "OTSVGMatrix-currentColor")
build(True, "OTSVG Matrix Baked", "OTSVGMatrix-baked")
print("wrote OTSVGMatrix-currentColor.ttf and OTSVGMatrix-baked.ttf")
```
