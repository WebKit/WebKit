# OTSVGGradientStops-currentColor.ttf / OTSVGGradientStops-baked.ttf

Test fonts for `fast/text/otsvg-currentcolor-gradient-stops.html`. The `A` glyph
is two cells: a linear gradient `currentColor` -> `currentColor` @0, and a radial
`currentColor` -> `var(--color1, #ff7fff)`. `-baked.ttf` resolves both for
`color:green`.

Split out of the `OTSVGMatrix` pair because no port resolves `currentColor` in a
stop: CoreText discards it ([323793](https://bugs.webkit.org/show_bug.cgi?id=323793)),
Skia renders it black ([323792](https://bugs.webkit.org/show_bug.cgi?id=323792)).

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
X0, Y_TOP, CELL, GAP = 50, -650, 300, 8


def rect(c):
    return X0 + c * CELL + GAP, Y_TOP + GAP, CELL - 2 * GAP, CELL - 2 * GAP


def svg_doc(baked):
    fg = GREEN if baked else "currentColor"
    pal1 = INV if baked else f"var(--color1, {INV})"
    defs = (
        "<defs>"
        f'<linearGradient id="lg" x1="0" y1="0" x2="1" y2="0">'
        f'<stop offset="0" stop-color="{fg}"/>'
        f'<stop offset="1" stop-color="{fg}" stop-opacity="0"/></linearGradient>'
        f'<radialGradient id="rg" cx="0.5" cy="0.5" r="0.5">'
        f'<stop offset="0" stop-color="{fg}"/>'
        f'<stop offset="1" stop-color="{pal1}"/></radialGradient>'
        "</defs>"
    )
    cells = ""
    for c, ref in ((0, "lg"), (1, "rg")):
        x, y, w, h = rect(c)
        cells += f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="url(#{ref})"/>'
    return ('<svg xmlns="http://www.w3.org/2000/svg" '
            'xmlns:xlink="http://www.w3.org/1999/xlink">'
            f'<g id="glyph1">{defs}{cells}</g></svg>')


def cpal():
    def t(h):
        return (int(h[1:3], 16) / 255, int(h[3:5], 16) / 255, int(h[5:7], 16) / 255, 1.0)
    return buildCPAL([[t("#0072ce"), t(INV)]])


def build(baked, family, stem):
    pen = TTGlyphPen(None)
    x0, x1 = X0, X0 + 2 * CELL
    y0, y1 = -(Y_TOP + CELL), -Y_TOP
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


build(False, "OTSVG GradientStops CC", "OTSVGGradientStops-currentColor")
build(True, "OTSVG GradientStops Baked", "OTSVGGradientStops-baked")
print("wrote OTSVGGradientStops-currentColor.ttf and OTSVGGradientStops-baked.ttf")
```
