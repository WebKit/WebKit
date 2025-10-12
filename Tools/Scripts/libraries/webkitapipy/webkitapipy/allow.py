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
from typing import Any, NamedTuple, Optional, Union

if sys.version_info < (3, 11):
    from webkitapipy._vendor import tomli as tomllib
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
    reason: AllowedReason
    bugs: Bugs

    symbols: list[str]
    selectors: list[Selector]
    classes: list[str]
    requires: list[str] = field(default_factory=list)

    class Selector(NamedTuple):
        name: str
        class_: Optional[str]

    class Bugs(NamedTuple):
        request: Optional[str]
        cleanup: Optional[str]


class AllowedReason(StrEnum):
    LEGACY = 'legacy'

    # For SPI that we intend to replace with API before the next release.
    TEMPORARY_USAGE = 'temporary-usage'

    # For pre-adopting new API before it is available in the SDK. There should
    # be no active `staging` entries when WebKit ships.
    STAGING = 'staging'

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
        seen_syms: dict[Union[str, AllowedSPI.Selector], AllowedSPI] = {}
        seen_sels: dict[Union[str, AllowedSPI.Selector], AllowedSPI] = {}
        seen_clss: dict[Union[str, AllowedSPI.Selector], AllowedSPI] = {}
        for reason in AllowedReason:
            for entry in doc.pop(reason.value, []):
                clss = entry.pop('classes', [])
                reqs = entry.pop('requires', [])
                sels = []
                for sel in entry.pop('selectors', []):
                    receiver = sel.get('class')
                    sels.append(AllowedSPI.Selector(sel['name'],
                                                    None if receiver == '?' else receiver))
                # Symbols use C-style name mangling rules (implicit leading
                # underscore), so that the names of C symbols in allowlists
                # match their spelling in code. Internally, symbols are tracked
                # in their raw form.
                syms = []
                for sym in entry.pop('symbols', []):
                    syms.append(f'_{sym}')

                bugs = AllowedSPI.Bugs(entry.pop('request', None),
                                       entry.pop('cleanup', None))
                allow = AllowedSPI(reason=reason, bugs=bugs, symbols=syms,
                                   selectors=sels, classes=clss, requires=reqs)

                if reason == AllowedReason.TEMPORARY_USAGE:
                    if not bugs.cleanup:
                        # Typically a temporary-use entry should have *both* a
                        # request and cleanup bug, but in some cases the
                        # temporary usage does not require new API to resolve.
                        # For example, using SPI to work around a bug in an
                        # underlying framework.
                        raise ValueError('Allowlist entries marked '
                                         'temporary-usage must have a '
                                         f'"cleanup" bug: {allow}')
                elif reason not in (AllowedReason.LEGACY,
                                    AllowedReason.STAGING):
                    if not bugs.request:
                        raise ValueError('Allowlist entries must have a '
                                         f'"request" bug: {allow}')

                if entry:
                    raise ValueError('Unrecognized items in allowlist entry: '
                                     f'{entry}')

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
                            raise ValueError(f'"{item}" in "{bugs.request}" '
                                             'already mentioned in allowlist '
                                             f'at "{prev.bugs.request}".')
                        prevs[item] = allow
                entries.append(allow)
        if doc:
            raise ValueError(f'Unrecognized items in allowlist: {doc.keys()}')
        return cls(entries)

    @classmethod
    def from_file(cls, config_file: Path) -> AllowList:
        try:
            doc = tomllib.load(config_file.open('rb'))
        except tomllib.TOMLDecodeError as error:
            if sys.version_info < (3, 11):
                raise ValueError(f'{config_file}: error: decode failed') from error
            else:
                error.add_note(f'{config_file}: error: decode failed"')
                raise
        return cls.from_dict(doc)
