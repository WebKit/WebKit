#!/usr/bin/env python3
# Add a stylistic-set substitution to the existing WPT MathML test font.
# Requires fontTools. Run from this directory to regenerate the adjacent font.
from pathlib import Path
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parent
font = TTFont(root / "../../imported/w3c/web-platform-tests/fonts/math/mathvariant-italic.woff")
font.recalcTimestamp = False
# Ordinary shaping substitutes the encoded italic A with the original ASCII A.
# The legacy MathML painter must continue using the encoded glyph and its ink.
addOpenTypeFeaturesFromString(font, "feature ss01 { sub u1D434 by A; } ss01;")
font.save(root / "mathvariant-highlight-shaping.woff")
