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

"""Layout test suite format with LayoutTestStatus enum."""

from enum import Flag, auto
from typing import Dict, FrozenSet, Set, Optional, List, Type, Tuple

from webkitexpectationspy.suites.base import TestSuiteFormat
from webkitexpectationspy.modifiers import ModifiersBase, LayoutTestModifiers


class LayoutTestStatus(Flag):
    """Result statuses for layout tests.

    The member declaration order defines the canonical serialization order.
    Supports case-insensitive lookup via ``LayoutTestStatus['fail']``.
    """
    PASS = auto()
    FAIL = auto()
    CRASH = auto()
    TIMEOUT = auto()
    SKIP = auto()
    IMAGE = auto()
    TEXT = auto()
    IMAGE_PLUS_TEXT = auto()
    AUDIO = auto()
    LEAK = auto()
    MISSING = auto()

    def __str__(self):
        _DISPLAY = {
            'IMAGE': 'ImageOnlyFailure',
            'IMAGE_PLUS_TEXT': 'ImagePlusText',
        }
        if self.name:
            return _DISPLAY.get(self.name, self.name.title())
        return str(self.value)

    @classmethod
    def from_string(cls, name):
        """Case-insensitive lookup by name."""
        for member in cls:
            if member.name.lower() == name.lower():
                return member
        raise ValueError('{!r} is not a valid {}'.format(name, cls.__name__))


class LayoutTestSuite(TestSuiteFormat):
    DEFAULT_VERSION_ORDER = (
        'mojave', 'catalina', 'bigsur', 'monterey', 'ventura', 'sonoma', 'sequoia', 'tahoe', 'goldengate',
        'ios16', 'ios17', 'ios18', 'ios26', 'ios27',
    )
    DEFAULT_FLAVOR_TOKENS = frozenset({
        'wk1', 'wk2', 'siteisolation', 'gpuprocess', 'lbse', 'webgl',
        'isolatedtree', 'wayland', 'gtk3', 'legacyapi',
    })

    def __init__(
        self,
        version_name_map: Optional[Dict[str, Tuple[int, ...]]] = None,
        version_tokens: Optional[Set[str]] = None,
        flavor_tokens: Optional[Set[str]] = None,
    ) -> None:
        """version_name_map maps the running port's version names, including internal aliases, to version numbers.
        version_tokens lists every version name any line may use, so names for other platforms still parse.
        """
        self._version_name_map = {name.lower(): tuple(number) for name, number in (version_name_map or {}).items()}
        self._version_tokens = {token.lower() for token in version_tokens} if version_tokens is not None else set(self.DEFAULT_VERSION_ORDER)
        self._flavor_tokens = frozenset(token.lower() for token in flavor_tokens) if flavor_tokens is not None else self.DEFAULT_FLAVOR_TOKENS

    @property
    def name(self) -> str:
        return 'layout-tests'

    @property
    def expectation_map(self) -> Dict:
        return {
            'pass': LayoutTestStatus.PASS,
            'fail': LayoutTestStatus.FAIL,
            'failure': LayoutTestStatus.FAIL,
            'crash': LayoutTestStatus.CRASH,
            'timeout': LayoutTestStatus.TIMEOUT,
            'imageonlyfailure': LayoutTestStatus.IMAGE,
            'image': LayoutTestStatus.IMAGE,
            'text': LayoutTestStatus.TEXT,
            'audio': LayoutTestStatus.AUDIO,
            'leak': LayoutTestStatus.LEAK,
            'missing': LayoutTestStatus.MISSING,
        }

    @property
    def modifier_map(self) -> Set[str]:
        return {'skip', 'slow', 'wontfix', 'rebaseline', 'dumpjsconsoleloginstderr'}

    @property
    def modifier_class(self) -> Type[ModifiersBase]:
        return LayoutTestModifiers

    @property
    def status_names(self) -> Dict:
        return {member: str(member) for member in LayoutTestStatus}

    @property
    def expectation_order(self) -> Tuple:
        return tuple(LayoutTestStatus)

    @property
    def version_tokens(self) -> Set[str]:
        return self._version_tokens | set(self._version_name_map)

    @property
    def version_order(self) -> List[str]:
        if self._version_name_map:
            return sorted(self._version_name_map, key=lambda name: (self._version_name_map[name], name))
        return list(self.DEFAULT_VERSION_ORDER)

    @property
    def version_name_map(self) -> Dict[str, Tuple[int, ...]]:
        return self._version_name_map

    @property
    def flavor_tokens(self) -> Optional[FrozenSet[str]]:
        return self._flavor_tokens

    def is_wildcard_pattern(self, pattern: str) -> bool:
        if pattern.endswith('*'):
            return True
        if pattern.endswith('/'):
            return True
        if '/' in pattern and '.' not in pattern.split('/')[-1]:
            return True
        return False

    def validate_test_pattern(self, pattern: str) -> Optional[str]:
        if not pattern:
            return None
        if pattern.startswith('/'):
            return None
        if '/' in pattern or '.' in pattern or pattern.endswith('*'):
            return pattern
        if pattern.replace('-', '').replace('_', '').isalnum():
            return pattern
        return pattern

    def matches_test(self, pattern: str, test_name: str) -> bool:
        if pattern == test_name:
            return True
        if pattern.endswith('/'):
            return test_name.startswith(pattern)
        if pattern.endswith('*'):
            return test_name.startswith(pattern[:-1])
        if '.' not in pattern.split('/')[-1]:
            if test_name.startswith(pattern + '/'):
                return True
        return False

    def result_was_expected(self, actual_result, expected, modifiers=None):
        if actual_result in expected:
            return True
        if actual_result in (LayoutTestStatus.TEXT, LayoutTestStatus.IMAGE_PLUS_TEXT, LayoutTestStatus.AUDIO):
            if LayoutTestStatus.FAIL in expected:
                return True
        if actual_result == LayoutTestStatus.MISSING:
            if modifiers and hasattr(modifiers, 'rebaseline') and modifiers.rebaseline:
                return True
        if actual_result == LayoutTestStatus.SKIP:
            if modifiers and modifiers.skip:
                return True
        return False

    def remove_pixel_failures(self, expected):
        members = set()
        for member in type(expected):
            if member in expected:
                members.add(member)
        members.discard(LayoutTestStatus.IMAGE)
        if LayoutTestStatus.IMAGE_PLUS_TEXT in members:
            members.discard(LayoutTestStatus.IMAGE_PLUS_TEXT)
            members.add(LayoutTestStatus.TEXT)
        combined = None
        for m in members:
            combined = m if combined is None else (combined | m)
        return combined

    def remove_leak_failures(self, expected):
        members = set()
        for member in type(expected):
            if member in expected and member != LayoutTestStatus.LEAK:
                members.add(member)
        combined = None
        for m in members:
            combined = m if combined is None else (combined | m)
        return combined

    def get_expectation_name(self, constant) -> str:
        return str(constant)
