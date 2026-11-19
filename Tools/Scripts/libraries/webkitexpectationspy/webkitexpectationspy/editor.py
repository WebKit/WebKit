# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import re
from typing import Iterable, Optional, Sequence

from webkitexpectationspy.expectations import Expectation
from webkitexpectationspy.linter import comment_groups
from webkitexpectationspy.parser import ExpectationParser
from webkitexpectationspy.suites.base import TestSuiteFormat

_BANNER_RE = re.compile(r'^\s*#\s*([#/=*-])\1{8,}')
_BRACKET_RE = re.compile(r'\[[^\]]*\]')


def remove_expectations(content: str, expectations: Iterable[Expectation]) -> str:
    lines = content.split('\n')
    removed = {_checked_line_number(lines, expectation) for expectation in expectations}
    entry_line_numbers = {number for number, line in enumerate(lines, 1) if line.strip() and not line.strip().startswith('#')}
    for group in comment_groups(lines, entry_line_numbers):
        header = group['comments']
        if not header or header[0] == 1 or not set(group['entries']) <= removed:
            continue
        if any(_BANNER_RE.match(lines[number - 1]) for number in header):
            continue
        removed.update(header)
    removed.update(_blank_lines_left_doubled(lines, removed))
    return '\n'.join(line for number, line in enumerate(lines, 1) if number not in removed)


def set_expectation_results(content: str, expectation: Expectation, results: Sequence[str], suite=None) -> str:
    if not results:
        raise ValueError(f'{expectation.filename}:{expectation.line_number} needs at least one result')
    lines = content.split('\n')
    number = _checked_line_number(lines, expectation)
    body, comment = _split_comment(lines[number - 1])
    modifiers = (suite or TestSuiteFormat()).modifier_map
    bracket = _BRACKET_RE.search(body, _token_match(body, expectation.test_pattern).end())
    kept = [token for token in bracket.group(0)[1:-1].split() if _is_modifier(token, modifiers)] if bracket else []
    replacement = f"[ {' '.join(list(results) + kept)} ]"
    if bracket:
        body = body[:bracket.start()] + replacement + body[bracket.end():]
    else:
        stripped = body.rstrip()
        body = f'{stripped} {replacement}{body[len(stripped):]}'
    lines[number - 1] = body + comment
    return '\n'.join(lines)


def _checked_line_number(lines: list[str], expectation: Expectation) -> int:
    number = expectation.line_number
    if not number or number > len(lines) or not _token_match(_split_comment(lines[number - 1])[0], expectation.test_pattern):
        raise ValueError(f'{expectation.filename}:{number} is not an entry for {expectation.test_pattern}')
    return number


def _split_comment(line: str) -> tuple[str, str]:
    index = line.find('#')
    return (line, '') if index == -1 else (line[:index], line[index:])


def _token_match(body: str, token: str) -> Optional[re.Match]:
    return re.search(rf'(?<!\S){re.escape(token)}(?!\S)', body)


def _is_modifier(token: str, modifiers: set[str]) -> bool:
    return token.lower() in modifiers or bool(ExpectationParser.SLOW_TIMEOUT_PATTERN.match(token))


def _blank_lines_left_doubled(lines: list[str], removed: set[int]) -> set[int]:
    last = len(lines) - 1 if lines and lines[-1] == '' else len(lines)

    def is_blank(number: int) -> bool:
        return 1 <= number <= last and not lines[number - 1].strip()

    extra = set()
    number = 1
    while number <= last:
        if number not in removed:
            number += 1
            continue
        start = number
        while number <= last and number in removed:
            number += 1
        before, after = start - 1, number
        if (before == 0 or is_blank(before)) and is_blank(after):
            extra.add(after)
        elif is_blank(before) and after > last:
            extra.add(before)
    return extra
