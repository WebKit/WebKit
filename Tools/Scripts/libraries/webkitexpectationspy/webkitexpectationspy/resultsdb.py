# Copyright (C) 2024-2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""ResultsDB status codes and severity-ordered sort helpers.

ResultsDB stores test outcomes as integer codes whose numeric ordering is the
canonical severity order (lowest = most severe). This module is the single
authority for that ordering — callers who need to sort result statuses should
use ``sort_key`` rather than reinventing one.
"""

from enum import Flag, IntEnum
from typing import Iterable

from webkitexpectationspy.expectations import ResultStatus


class ResultsDBStatus(IntEnum):
    CRASH = 0x00
    TIMEOUT = 0x08
    LEAK = 0x10
    AUDIO = 0x18
    TEXT = 0x20
    IMAGE_PLUS_TEXT = 0x24
    FAIL = 0x28
    IMAGE = 0x30
    MISSING = 0x38
    PASS = 0x40
    SKIP = 0x50


_RESULTSDB_MAP = {
    'PASS': ResultsDBStatus.PASS,
    'FAIL': ResultsDBStatus.FAIL,
    'CRASH': ResultsDBStatus.CRASH,
    'TIMEOUT': ResultsDBStatus.TIMEOUT,
    'SKIP': ResultsDBStatus.SKIP,
    'IMAGE': ResultsDBStatus.IMAGE,
    'TEXT': ResultsDBStatus.TEXT,
    'IMAGE_PLUS_TEXT': ResultsDBStatus.IMAGE_PLUS_TEXT,
    'AUDIO': ResultsDBStatus.AUDIO,
    'LEAK': ResultsDBStatus.LEAK,
    'MISSING': ResultsDBStatus.MISSING,
}

_RESULTSDB_REVERSE = {v: k for k, v in _RESULTSDB_MAP.items()}


def to_resultsdb(status: Flag) -> int:
    name = status.name
    if name in _RESULTSDB_MAP:
        return int(_RESULTSDB_MAP[name])
    return -1


def from_resultsdb(code: int, status_type: type) -> Flag:
    try:
        rdb = ResultsDBStatus(code)
    except ValueError:
        raise ValueError('Unknown ResultsDB code: {:#x}'.format(code))
    name = _RESULTSDB_REVERSE.get(rdb)
    if name is None:
        raise ValueError('No mapping for ResultsDB code: {:#x}'.format(code))
    try:
        return status_type[name]
    except KeyError:
        raise ValueError('{} has no member {!r}'.format(status_type.__name__, name))


def sort_key(status: Flag) -> int:
    """Severity key for ``status`` — use with ``sorted(..., key=sort_key)``.

    When ``status`` is a combined Flag value (multiple members set), the key is
    the most severe constituent. Unknown statuses sort after all known ones.
    """
    members = [m for m in type(status) if m in status] if status is not None else []
    if not members:
        return 2**31 - 1
    return min(to_resultsdb(m) if to_resultsdb(m) >= 0 else 2**30 for m in members)


def sorted_by_severity(statuses: Iterable[Flag]) -> list:
    return sorted(statuses, key=sort_key)
