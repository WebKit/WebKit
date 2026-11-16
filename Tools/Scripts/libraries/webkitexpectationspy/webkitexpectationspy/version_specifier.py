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

"""Version specifier handling for test expectations.

Supports both version names (e.g. "Sequoia") and version numbers (e.g. "15.0").
When a version_name_map is provided, comparisons use version numbers for
precision; otherwise falls back to position in version_order.
"""

import enum
import re


_VERSION_NUMBER_RE = re.compile(r'^\d+(\.\d+)*$')

_MAX_VERSION_PARTS = 3


def _normalize_version(version_tuple, parts=_MAX_VERSION_PARTS):
    """Pad a version tuple to a fixed length for consistent comparison."""
    return version_tuple + (0,) * (parts - len(version_tuple))


class VersionSpecifier:
    """Represents a version constraint with optional range/modifier.

    Supports:
        - EXACT: 'Sonoma' or '14.0' (this version only)
        - AND_LATER: 'Sonoma+' or '14.0+' (this and later versions)
        - AND_EARLIER: 'Sonoma-' or '14.0-' (this and earlier versions)
        - RANGE: 'Ventura-Sequoia' or '13.0-15.0' (inclusive range)
    """

    class Type(enum.Enum):
        EXACT = 'exact'
        AND_LATER = 'and_later'
        AND_EARLIER = 'and_earlier'
        RANGE = 'range'

    def __init__(self, base_version, end_version=None, specifier_type=None):
        self.base_version = base_version.lower()
        self.end_version = end_version.lower() if end_version else None
        self.type = specifier_type or self.Type.EXACT
        self._original = None

    @classmethod
    def parse(cls, token):
        """Parse a version token into a VersionSpecifier.

        Returns a VersionSpecifier instance, or None if the token is a plain
        version name (which should be handled as EXACT by the caller).
        """
        if '-' in token and not token.endswith('-'):
            parts = token.split('-', 1)
            if len(parts) == 2:
                spec = cls(parts[0], parts[1], cls.Type.RANGE)
                spec._original = token
                return spec
        elif token.endswith('+'):
            spec = cls(token[:-1], specifier_type=cls.Type.AND_LATER)
            spec._original = token
            return spec
        elif token.endswith('-'):
            spec = cls(token[:-1], specifier_type=cls.Type.AND_EARLIER)
            spec._original = token
            return spec
        return None

    def matches(self, current_version, version_order, version_name_map=None):
        """Check if current_version satisfies this specifier.

        Args:
            current_version: The version string to check (will be lowercased).
            version_order: List of version strings in order from oldest to newest.
            version_name_map: Optional dict mapping version names to version
                number tuples (e.g. {"sequoia": (15,), "sonoma": (14,)}).
                When provided, enables numeric comparison for precision.

        Returns:
            True if the current version matches this specifier.
        """
        current_version = current_version.lower()

        if version_name_map:
            current_num = self._resolve_version_number(current_version, version_name_map)
            base_num = self._resolve_version_number(self.base_version, version_name_map)
            if current_num is not None and base_num is not None:
                return self._matches_numeric(current_num, base_num, version_name_map)

        # Fallback: position-based comparison
        try:
            current_idx = version_order.index(current_version)
            base_idx = version_order.index(self.base_version)
        except ValueError:
            return False

        if self.type == self.Type.EXACT:
            return current_version == self.base_version
        elif self.type == self.Type.AND_LATER:
            return current_idx >= base_idx
        elif self.type == self.Type.AND_EARLIER:
            return current_idx <= base_idx
        elif self.type == self.Type.RANGE:
            try:
                end_idx = version_order.index(self.end_version)
            except ValueError:
                return False
            return base_idx <= current_idx <= end_idx
        return False

    def _resolve_version_number(self, version_str, version_name_map):
        if version_str in version_name_map:
            return version_name_map[version_str]
        if _VERSION_NUMBER_RE.match(version_str):
            return tuple(int(x) for x in version_str.split('.'))
        return None

    def _matches_numeric(self, current_num, base_num, version_name_map):
        current_num = _normalize_version(current_num)
        base_num = _normalize_version(base_num)
        if self.type == self.Type.EXACT:
            return current_num == base_num
        elif self.type == self.Type.AND_LATER:
            return current_num >= base_num
        elif self.type == self.Type.AND_EARLIER:
            return current_num <= base_num
        elif self.type == self.Type.RANGE:
            end_num = self._resolve_version_number(self.end_version, version_name_map) if self.end_version else None
            if end_num is None:
                return False
            end_num = _normalize_version(end_num)
            return base_num <= current_num <= end_num
        return False

    def __repr__(self):
        if self._original:
            return 'VersionSpecifier({!r})'.format(self._original)
        return 'VersionSpecifier({!r}, type={})'.format(self.base_version, self.type.value)

    def __eq__(self, other):
        if not isinstance(other, VersionSpecifier):
            return False
        return (self.base_version == other.base_version and
                self.end_version == other.end_version and
                self.type == other.type)

    def __hash__(self):
        return hash((self.base_version, self.end_version, self.type))
