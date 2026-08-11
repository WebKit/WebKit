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
from typing import Dict, Set, Optional, List, Type, Tuple

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
        return {'skip', 'slow', 'wontfix', 'rebaseline'}

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
        return {
            'monterey', 'ventura', 'sonoma', 'sequoia',
            'ios16', 'ios17', 'ios18',
            'bigsur', 'catalina', 'mojave',
        }

    @property
    def version_order(self) -> List[str]:
        return [
            'mojave', 'catalina', 'bigsur', 'monterey', 'ventura', 'sonoma', 'sequoia',
            'ios16', 'ios17', 'ios18',
        ]

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
