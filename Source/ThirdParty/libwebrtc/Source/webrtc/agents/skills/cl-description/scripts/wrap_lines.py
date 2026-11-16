#!/usr/bin/env vpython3

# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Wraps a CL description (commit message) at 72 columns.

Usage:
  wrap_lines.py [FILE ...]

Reads the draft from the given files (or stdin) and prints the wrapped
description to stdout. Rules:
  - The subject (first line) is kept on one line with whitespace normalized
    and a single trailing period removed.
  - Body paragraphs are re-flowed at 72 columns.
  - Bullet items ("-", "*", "+", "1.", "1)") are re-flowed with continuation
    lines aligned to the bullet text.
  - Indented lines that are not bullets (code, logs, quotes) are kept as is.
  - The trailing footer block (e.g. "Bug:", "Change-Id:") is kept as is, one
    footer per line.
"""

import fileinput
import re
import textwrap

_BULLET_RE = re.compile(r'^(\s*(?:[-*+]|\d+[.)])\s+)(.*)$')
_FOOTER_RE = re.compile(r'^[A-Za-z][A-Za-z0-9_-]*(?:\[[^\]]+\])?[:=]\s*\S')
_INDENTED_RE = re.compile(r'^(?:\s{2,}|\t)\S')


def _strip_trailing_period(text):
    if text.endswith('.') and not text.endswith('..'):
        return text[:-1]
    return text


def _fill(text, width, initial_indent='', subsequent_indent=''):
    wrapper = textwrap.TextWrapper(width=width,
                                   initial_indent=initial_indent,
                                   subsequent_indent=subsequent_indent,
                                   break_long_words=False,
                                   break_on_hyphens=False)
    return wrapper.fill(' '.join(text.split()))


def _is_footer_block(paragraph):
    lines = paragraph.splitlines()
    return bool(lines) and all(_FOOTER_RE.match(line) for line in lines)


def _wrap_paragraph(paragraph, width):
    """Wraps a paragraph that may mix prose, bullets and indented blocks."""
    # Each item is (kind, lines) where kind is 'text', 'bullet' or 'verbatim'.
    items = []
    for line in paragraph.splitlines():
        if _BULLET_RE.match(line):
            items.append(('bullet', [line]))
        elif _INDENTED_RE.match(line) and (not items
                                           or items[-1][0] != 'bullet'):
            items.append(('verbatim', [line]))
        elif items and items[-1][0] in ('text', 'bullet'):
            items[-1][1].append(line)
        else:
            items.append(('text', [line]))

    out = []
    for kind, lines in items:
        if kind == 'verbatim':
            out.append('\n'.join(line.rstrip() for line in lines))
        elif kind == 'bullet':
            marker = _BULLET_RE.match(lines[0]).group(1)
            content = ' '.join(lines)[len(marker):]
            out.append(_fill(content, width, marker, ' ' * len(marker)))
        else:
            out.append(_fill(' '.join(lines), width))
    return '\n'.join(out)


def wrap_text(text, width=72):
    """Returns `text` formatted as a CL description wrapped at `width`."""
    if not text.strip():
        return ''

    parts = text.strip('\n').split('\n', 1)
    subject = _strip_trailing_period(' '.join(parts[0].split()))
    body = parts[1].strip('\n') if len(parts) > 1 else ''
    if not body.strip():
        return subject

    paragraphs = [p for p in re.split(r'\n\s*\n', body) if p.strip()]
    footer = None
    if paragraphs and _is_footer_block(paragraphs[-1]):
        footer = '\n'.join(line.strip()
                           for line in paragraphs.pop().splitlines())

    blocks = [_wrap_paragraph(p, width) for p in paragraphs]
    if footer:
        blocks.append(footer)
    return subject + '\n\n' + '\n\n'.join(blocks)


if __name__ == '__main__':
    print(wrap_text(''.join(fileinput.input())))
