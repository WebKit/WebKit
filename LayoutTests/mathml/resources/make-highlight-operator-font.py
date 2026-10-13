#!/usr/bin/env python3
"""Give the bundled operator test font a thin minus without changing its metrics."""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.pens.t2CharStringPen import T2CharStringPen

root = Path(__file__).resolve().parents[2]
font = TTFont(root / "imported/w3c/web-platform-tests/fonts/math/operators.woff", recalcTimestamp=False)
minus = font.getBestCmap()[0x2212]
charstrings = font["CFF "].cff.topDictIndex[0].CharStrings
original = charstrings[minus]
pen = T2CharStringPen(1000, None)
pen.moveTo((200, 300))
pen.lineTo((800, 300))
pen.lineTo((800, 500))
pen.lineTo((200, 500))
pen.closePath()
charstrings[minus] = pen.getCharString(private=original.private, globalSubrs=original.globalSubrs)
font.save(Path(__file__).with_name("highlight-operators.woff"))
