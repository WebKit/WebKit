#!/usr/bin/env python3
##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
"""Wraps paragraphs of text, preserving manual formatting

This is like fold(1), but has the special convention of not modifying lines
that start with whitespace. This allows you to intersperse blocks with
special formatting, like code blocks, with written prose. The prose will
be wordwrapped, and the manual formatting will be preserved.

 * This won't handle the case of a bulleted (or ordered) list specially, so
   manual wrapping must be done.

Occasionally it's useful to put something with explicit formatting that
doesn't look at all like a block of text inline.

  indicator = has_leading_whitespace(line);
  if (indicator)
    preserve_formatting(line);

The intent is that this docstring would make it through the transform
and still be legible and presented as it is in the source. If additional
cases are handled, update this doc to describe the effect.
"""

__author__ = "jkoleszar@google.com"
import textwrap
import sys

def wrap(text):
    if text:
        return textwrap.fill(text, break_long_words=False) + '\n'
    return ""


def main(fileobj):
    text = ""
    output = ""
    while True:
        line = fileobj.readline()
        if not line:
            break

        if line.lstrip() == line:
            text += line
        else:
            output += wrap(text)
            text=""
            output += line
    output += wrap(text)

    # Replace the file or write to stdout.
    if fileobj == sys.stdin:
        fileobj = sys.stdout
    else:
        fileobj.seek(0)
        fileobj.truncate(0)
    fileobj.write(output)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        main(open(sys.argv[1], "r+"))
    else:
        main(sys.stdin)
