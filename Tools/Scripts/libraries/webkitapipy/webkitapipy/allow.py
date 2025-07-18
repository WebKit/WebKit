# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from enum import Enum
from typing import Any

if sys.version_info < (3, 11):
    from webkitapipy._vendor import tomli
else:
    import tomllib

if sys.version_info < (3, 11):
    class StrEnum(str, Enum):
        def __str__(self):
            return self.value
else:
    from enum import StrEnum


@dataclass
class AllowedSPI:
    key: str
    bug: str | PermanentlyAllowedReason

    symbols: list[str]
    selectors: list[str]
    classes: list[str]
    requires: list[str] = field(default_factory=list)


class PermanentlyAllowedReason(StrEnum):
    LEGACY = 'legacy'

    # For SPI implementing non-essential web engine features that a browser
    # vendor would either not use or provide their own implementation.
    NOT_WEB_ESSENTIAL = 'not-web-essential'

    # For SPI that has same behavior as API except in internal builds.
    EQUIVALENT_API = 'equivalent-api'


@dataclass
class AllowList:
    allowed_spi: list[AllowedSPI]

    @classmethod
    def from_dict(cls, doc: dict[str, Any]) -> AllowList:
        entries = []
        seen_syms: dict[str, AllowedSPI] = {}
        seen_sels: dict[str, AllowedSPI] = {}
        seen_clss: dict[str, AllowedSPI] = {}
        for key in doc:
            for bug, entry in doc[key].items():
                if bug.startswith('rdar://') or \
                        bug.startswith('https://bugs.webkit.org') or \
                        bug.startswith('https://webkit.org/b/'):
                    pass
                else:
                    bug = PermanentlyAllowedReason(bug)

                syms = entry.get('symbols', [])
                sels = entry.get('selectors', [])
                clss = entry.get('classes', [])
                reqs = entry.get('requires', [])
                allow = AllowedSPI(key=key, bug=bug, symbols=syms,
                                   selectors=sels, classes=clss, requires=reqs)

                # Validate that each section is a list (not a string, to avoid
                # treating each character as a separate declaration), and that
                # there are no repeats.
                for items, prevs in (
                    (syms, seen_syms),
                    (sels, seen_sels),
                    (clss, seen_clss),
                    # Repeats in different `requires` lists are allowed, though
                    # repeats in the same list should be detected.
                    (reqs, {}),
                ):
                    if isinstance(items, str):
                        raise ValueError(f'"{items}" in allowlist is a '
                                         'string, expected a list')
                    for item in items:
                        if (prev := prevs.get(item)) and prev.requires == reqs:
                            raise ValueError(f'"{item}" already mentioned in '
                                             f'allowlist at "{prev.key}".'
                                             f'"{prev.bug}"')
                        prevs[item] = allow
                entries.append(allow)
        return cls(entries)

    @classmethod
    def from_file(cls, config_file: Path) -> AllowList:
        if sys.version_info < (3, 11):
            doc = tomli.load(config_file.open('rb'))
        else:
            doc = tomllib.load(config_file.open('rb'))
        return cls.from_dict(doc)
