"""Makefile-syntax depfile helpers, shared by the Swift build scripts.

Depfiles are Makefile syntax, not shell syntax: a space is escaped with a
backslash, and a backslash before anything else stays literal so that Windows
paths survive. Implement Make-style quoting rules instead of using shlex.
"""

import re
from pathlib import Path

_UNESCAPED_SPACE = re.compile(r"(?<!\\)\s+")


def escape(path):
    return path.replace(" ", "\\ ")


def unescape(path):
    return path.replace("\\ ", " ")


def parse_text(text):
    """Yield the dependencies recorded in the contents of one depfile.

    Covers both spellings the Swift build produces: swiftc writes
    `output.o : dep dep`, and write_ninja_depfile in swiftc-wrapper.py writes
    `output:` followed by one continuation line per dependency.
    """
    for rule in text.replace("\\\n", " ").splitlines():
        tokens = [token for token in _UNESCAPED_SPACE.split(rule.strip()) if token]
        past_targets = False
        for token in tokens:
            if not past_targets:
                # A Windows drive letter (C:\foo.o) does not end in a colon, so
                # it cannot be mistaken for the separator.
                past_targets = token.endswith(":")
                continue
            yield unescape(token)


def parse(path):
    return parse_text(Path(path).read_text(errors="replace"))
