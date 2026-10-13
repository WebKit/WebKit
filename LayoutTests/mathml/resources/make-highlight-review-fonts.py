#!/usr/bin/env python3
"""Derive deterministic radical and acute-assembly controls from the WPT font."""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.pens.t2CharStringPen import T2CharStringPen

root = Path(__file__).resolve().parents[2]
source = root / "imported/w3c/web-platform-tests/fonts/math/operators.woff"


def replace_outline(font, glyph, points):
    charstrings = font["CFF "].cff.topDictIndex[0].CharStrings
    original = charstrings[glyph]
    pen = T2CharStringPen(1000, None)
    pen.moveTo(points[0])
    for point in points[1:]:
        pen.lineTo(point)
    pen.closePath()
    charstrings[glyph] = pen.getCharString(private=original.private, globalSubrs=original.globalSubrs)


# No MATH or RTLM: a bare radical must use vertical scaling and RTL reflection.
font = TTFont(source, recalcTimestamp=False)
del font["MATH"]
replace_outline(font, font.getBestCmap()[0x221A], [(0, 400), (150, 400), (300, 0), (600, 1000), (1000, 1000), (1000, 900), (700, 900), (300, 200)])
font.save(Path(__file__).with_name("highlight-radical-fallback.woff"))

# Retain the 3em cap metrics, but expose a miter extending beyond half a stroke.
font = TTFont(source, recalcTimestamp=False)
replace_outline(font, "v2", [(0, 0), (200, 0), (200, 1200), (1000, 1500), (200, 1800), (200, 3000), (0, 3000)])
# Expose the unchanged assembly pieces to the ordinary-text reference painter.
for cmap in font["cmap"].tables:
    if cmap.isUnicode():
        cmap.cmap[0xE000] = "v2"
        cmap.cmap[0xE001] = "v1"
font.save(Path(__file__).with_name("highlight-acute-assembly.woff"))
