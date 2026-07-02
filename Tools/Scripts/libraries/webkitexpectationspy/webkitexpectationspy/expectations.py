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

"""Core expectation classes for test expectations.

This module provides:
- ResultStatus: Generic Flag enum for result statuses (PASS, FAIL, CRASH, TIMEOUT, SKIP)
- Expectation: Frozen dataclass combining expected results, modifiers, and configuration
"""

from dataclasses import dataclass, field
from enum import Flag, auto
from typing import FrozenSet, Optional, Set, Tuple


from webkitexpectationspy.modifiers import ModifiersBase


class ResultStatus(Flag):
    """Generic result status Flag enum for test suites.

    The member declaration order defines the canonical serialization order.
    """
    PASS = auto()
    FAIL = auto()
    CRASH = auto()
    TIMEOUT = auto()
    SKIP = auto()

    def __str__(self):
        return self.name.title() if self.name else str(self.value)

    @classmethod
    def from_string(cls, name):
        """Case-insensitive lookup by name."""
        for member in cls:
            if member.name.lower() == name.lower():
                return member
        raise ValueError('{!r} is not a valid {}'.format(name, cls.__name__))


def _flag_members(flag_value):
    """Return a frozenset of individual Flag members in a combined Flag value."""
    if flag_value is None:
        return frozenset()
    return frozenset(m for m in type(flag_value) if m in flag_value)


@dataclass(frozen=True)
class Expectation:
    test_pattern: str
    expected: Flag = None
    modifiers: ModifiersBase = field(default_factory=ModifiersBase)
    configurations: FrozenSet[str] = field(default_factory=frozenset)
    version_specifiers: tuple = ()
    bug_ids: Tuple[str, ...] = ()
    filename: Optional[str] = None
    line_number: Optional[int] = None

    def __post_init__(self):
        if self.expected is None:
            object.__setattr__(self, 'expected', ResultStatus.PASS)
        elif isinstance(self.expected, set):
            combined = None
            for flag in self.expected:
                combined = flag if combined is None else (combined | flag)
            object.__setattr__(self, 'expected', combined if combined is not None else ResultStatus.PASS)
        if isinstance(self.configurations, set):
            object.__setattr__(self, 'configurations', frozenset(self.configurations))
        if isinstance(self.version_specifiers, list):
            object.__setattr__(self, 'version_specifiers', tuple(self.version_specifiers))
        if isinstance(self.bug_ids, list):
            object.__setattr__(self, 'bug_ids', tuple(self.bug_ids))

    @property
    def skip(self) -> bool:
        return self.modifiers.skip

    @property
    def slow(self) -> bool:
        return self.modifiers.slow is not None

    @property
    def flaky(self) -> bool:
        return len(_flag_members(self.expected)) > 1

    @property
    def slow_timeout(self) -> Optional[float]:
        return self.modifiers.slow

    def is_skip(self) -> bool:
        return self.modifiers.skip

    def is_slow(self) -> bool:
        return self.modifiers.slow is not None

    def is_flaky(self) -> bool:
        return self.flaky

    def is_wontfix(self) -> bool:
        return getattr(self.modifiers, 'wontfix', False)

    def get_timeout(self, default_timeout: int, multiplier: int = 5) -> int:
        if self.modifiers.slow is not None:
            if isinstance(self.modifiers.slow, (int, float)) and self.modifiers.slow > 1:
                return int(self.modifiers.slow)
            return default_timeout * multiplier
        return default_timeout

    def matches_test(self, test_name: str) -> bool:
        if self.test_pattern.endswith('*'):
            return test_name.startswith(self.test_pattern[:-1])
        return test_name == self.test_pattern

    def matches_configuration(self, current_config: Set[str],
                              current_version: Optional[str] = None,
                              version_order=None) -> bool:
        if self.configurations and not self.configurations.issubset(current_config):
            return False
        if self.version_specifiers:
            if not current_version or not version_order:
                return False
            for spec in self.version_specifiers:
                if not spec.matches(current_version, version_order):
                    return False
        return True

    def result_is_expected(self, actual_status) -> bool:
        return actual_status in self.expected

    def __gt__(self, other):
        if not isinstance(other, Expectation):
            return NotImplemented
        return _flag_members(other.expected) < _flag_members(self.expected)

    def __lt__(self, other):
        if not isinstance(other, Expectation):
            return NotImplemented
        return _flag_members(self.expected) < _flag_members(other.expected)

    def __repr__(self) -> str:
        result_names = sorted(str(e) for e in _flag_members(self.expected))
        return 'Expectation({!r}, results={}, config={})'.format(
            self.test_pattern, result_names,
            set(self.configurations) or 'None')
