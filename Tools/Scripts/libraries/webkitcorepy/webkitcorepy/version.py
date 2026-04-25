# Copyright (C) 2020 Apple Inc. All rights reserved.
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


class Version(object):
    MATCHES_RE = re.compile(r'(?P<operator>==|!=|>|<|>=|<=)? *(?P<version>\d+(\.\d+)?(\.\d+)?(\.\*)?)')

    @classmethod
    def from_string(cls, string):
        if not isinstance(string, str):
            raise TypeError('Version.from_string requires a str')

        # We want this outside of the try statement so it can raise its own TypeError.
        split = string.split('.')

        try:
            return cls(*split)
        except TypeError:
            # This occurs when we have too many arguments.
            raise ValueError("Invalid version string") from None

    @classmethod
    def from_iterable(cls, val):
        try:
            return cls(*val)
        except TypeError:
            # This occurs when we have too many arguments.
            raise ValueError("Too many iterable items") from None

    @staticmethod
    def from_name(name):
        from webkitpy.common.version_name_map import VersionNameMap
        return VersionNameMap.map().from_name(name)[1]

    def __init__(self, major=0, minor=0, tiny=0, micro=0, nano=0):
        self.major = int(major)
        self.minor = int(minor)
        self.tiny = int(tiny)
        self.micro = int(micro)
        self.nano = int(nano)

    def __len__(self):
        return 5

    def __getitem__(self, key):
        if isinstance(key, int):
            if key == 0:
                return self.major
            elif key == 1:
                return self.minor
            elif key == 2:
                return self.tiny
            elif key == 3:
                return self.micro
            elif key == 4:
                return self.nano
            raise IndexError('Version key must be between 0 and 4')
        elif isinstance(key, str):
            if key in ('major', 'minor', 'tiny', 'micro', 'nano'):
                return getattr(self, key)
            raise KeyError('Version key must be major, minor, tiny, micro or nano')
        raise TypeError('Expected version key to be string or integer')

    def __setitem__(self, key, value):
        if isinstance(key, int):
            if key == 0:
                self.major = int(value)
                return self.major
            elif key == 1:
                self.minor = int(value)
                return self.minor
            elif key == 2:
                self.tiny = int(value)
                return self.tiny
            elif key == 3:
                self.micro = int(value)
                return self.micro
            elif key == 4:
                self.nano = int(value)
                return self.nano
            raise IndexError('Version key must be between 0 and 4')
        elif isinstance(key, str):
            if key in ('major', 'minor', 'tiny', 'micro', 'nano'):
                return setattr(self, key, value)
            raise KeyError('Version key must be major, minor, tiny, micro or nano')
        raise TypeError('Expected version key to be string or integer')

    def matches(self, expressions):
        does_match = None
        for expression in expressions.split(','):
            match = self.MATCHES_RE.search(expression)
            if not match:
                continue
            operator = match.group('operator') or '=='
            if operator in ['==', '!=']:
                version_match = True
                split = match.group('version').split('.')
                for index in range(len(split)):
                    if '*' == split[index]:
                        continue
                    if str(self[index]) != split[index]:
                        version_match = False
                        break

                if operator == '==':
                    does_match = (does_match or False) | version_match
                if version_match and operator == '!=':
                    return False
                continue

            version = self.from_string(match.group('version').replace('*', '0'))
            does_match = (does_match or False) | {
                '>': lambda a, b: a > b,
                '<': lambda a, b: a < b,
                '>=': lambda a, b: a >= b,
                '<=': lambda a, b: a <= b,
            }[operator](self, version)

        return True if does_match is None else does_match

    def _strip_zeros(self):
        """Return the version components as a list with trailing zero components removed.

        Iterates from the end of the component list and records how many consecutive
        zeros appear before the first non-zero value (i), then slices them off.

        Examples:
            Version(1, 2, 3)       -> [1, 2, 3]   (no trailing zeros)
            Version(1, 2, 0)       -> [1, 2]       (one trailing zero stripped)
            Version(1, 2, 0, 0, 0) -> [1, 2]       (three trailing zeros stripped)
            Version(0, 0, 3)       -> [0, 0, 3]    (leading zeros preserved)
            Version(1, 0, 0, 0, 3) -> [1, 0, 0, 0, 3]  (no trailing zeros, nano is non-zero)
        """
        parts = list(self)
        for i, part in enumerate(reversed(parts)):
            if part != 0:
                break
        return parts[:len(parts) - i]

    # 11.2 is in 11, but 11 is not in 11.2
    def __contains__(self, version):
        if not isinstance(version, Version):
            raise TypeError('__contains__ requires a Version')
        stripped = tuple(self._strip_zeros())
        other = tuple(version)[:len(stripped)]
        return stripped == other

    def __str__(self):
        parts = self._strip_zeros()
        return '.'.join(map(str, parts))

    def __repr__(self):
        parts = self._strip_zeros()
        return f'Version({", ".join(map(str, parts))})'

    def __hash__(self):
        return hash(tuple(self))

    def __eq__(self, other):
        if other is None:
            return False
        return tuple(self) == tuple(other)

    def __lt__(self, other):
        if other is None:
            return False
        return tuple(self) < tuple(other)

    def __le__(self, other):
        if other is None:
            return False
        return tuple(self) <= tuple(other)

    def __gt__(self, other):
        if other is None:
            return True
        return tuple(self) > tuple(other)

    def __ge__(self, other):
        if other is None:
            return True
        return tuple(self) >= tuple(other)
