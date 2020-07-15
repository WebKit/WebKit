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

import collections
import re

from functools import total_ordering


class Version(object):
    MATCHES_RE = re.compile(r'(?P<operator>==|!=|>|<|>=|<=)?(?P<version>\d+(\.\d+)?(\.\d+)?(\.\*)?)')

    @staticmethod
    def from_string(string):
        assert isinstance(string, str) or isinstance(string, unicode)
        return Version.from_iterable(string.split('.'))

    @staticmethod
    def from_iterable(val):
        result = Version()
        for i in range(len(val)):
            result[i] = int(val[i])
        return result

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
            raise ValueError('Version key must be between 0 and 4')
        elif isinstance(key, str):
            if hasattr(self, key):
                return getattr(self, key)
            raise ValueError('Version key must be major, minor, tiny, micro or nano')
        raise ValueError('Expected version key to be string or integer')

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
            raise ValueError('Version key must be between 0 and 4')
        elif isinstance(key, str):
            if hasattr(self, key):
                return setattr(self, key, value)
            raise ValueError('Version key must be major, minor, tiny, micro or nano')
        raise ValueError('Expected version key to be string or integer')

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

    # 11.2 is in 11, but 11 is not in 11.2
    def __contains__(self, version):
        assert isinstance(version, Version)
        does_match = True
        for i in range(len(version)):
            if self[i] != version[i]:
                does_match = False
            if not does_match and self[i] != 0:
                return False
        return True

    def __repr__(self):
        len_to_print = 1
        for i in range(len(self)):
            if self[i]:
                len_to_print = i + 1
        result = str(self.major)
        for i in range(len_to_print - 1):
            result += '.{}'.format(self[i + 1])
        return result

    def __hash__(self):
        result = 0
        for index in range(len(self)):
            result *= 1000
            result += self[index]
        return result

    def __cmp__(self, other):
        if other is None:
            return 1
        for i in range(len(self)):
            diff = self[i] - other[i]
            if diff:
                return diff
        return 0

    def __eq__(self, other):
        return self.__cmp__(other) == 0

    def __ne__(self, other):
        return self.__cmp__(other) != 0

    def __lt__(self, other):
        return self.__cmp__(other) < 0

    def __le__(self, other):
        return self.__cmp__(other) <= 0

    def __gt__(self, other):
        return self.__cmp__(other) > 0

    def __ge__(self, other):
        return self.__cmp__(other) >= 0
