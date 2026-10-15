# Ahem-CurrentColor-SVG.ttf

Test font for `fast/text/otsvg-currentcolor.html`, used to verify that
`currentColor` inside an [OpenType-SVG glyph resolves to the CSS text color]
(https://bugs.webkit.org/show_bug.cgi?id=320029).

## Regenerating
Script is PEP-723 compliant, so you can run with any python built tool that supports it, like `uv run make.py`
If not, just `pip install fonttools && python3 make.py` would work.

```python
#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = ["fonttools"]
# ///
from pathlib import Path
from fontTools.ttLib import TTFont

HERE = Path(__file__).parent
font = TTFont(HERE / "Ahem-SVG.ttf")
changed = 0
for entry in font["SVG "].docList:
    if 'fill="green"' in entry.data:
        entry.data = entry.data.replace('fill="green"', 'fill="currentColor"')
        changed += 1
assert changed == 1, f"expected one green glyph doc, changed {changed}"
font.save(HERE / "Ahem-CurrentColor-SVG.ttf")
print(f"wrote Ahem-CurrentColor-SVG.ttf ({changed} glyph doc: green -> currentColor)")
```