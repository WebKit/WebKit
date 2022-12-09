# Copyright (C) 2022 Sony Interactive Entertainment
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Matches feature defines in source code."""

import re

from functools import total_ordering


@total_ordering
class FeatureDefine:
    def __init__(self, flag, value=False, description="", files=None):
        self.flag = flag
        self.value = value
        self.description = description

        self.files = files if files else set()

    def __hash__(self):
        return hash(self.flag)

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.flag == other.flag

        return NotImplemented

    def __lt__(self, other):
        if isinstance(other, self.__class__):
            return self.flag < other.flag

        return NotImplemented


class _FeatureDefineMatcher:
    def __init__(self, pattern, prefix=''):
        self._regex = re.compile(pattern)
        self._prefix = prefix

    def __call__(self, line):
        match = self._regex.search(line)

        if not match:
            return None

        groups = match.groupdict()

        flag = self._prefix + groups['flag']
        description = groups.get('description', '')
        value = groups.get('value', '') in ['1', 'ON']

        return FeatureDefine(flag, value=value, description=description)


def usage_matcher(macro='ENABLE'):
    """ Matches MACRO(FLAG) """
    pattern = macro + r'\((?P<flag>[0-9A-Z_]+)\)'
    prefix = macro + '_'

    return _FeatureDefineMatcher(pattern, prefix=prefix)


def flag_matcher(macro='ENABLE'):
    """ Matches MACRO_FLAG """
    pattern = r'(?P<flag>' + macro + r'_[0-9A-Z_]+)'

    return _FeatureDefineMatcher(pattern)


def declaration_matcher(macro='ENABLE'):
    """ Matches #define MACRO_FLAG VALUE """
    pattern = r'#define (?P<flag>' + macro + r'_[0-9A-Z_]+) (?P<value>[0,1])'

    return _FeatureDefineMatcher(pattern)


def idl_usage_matcher():
    """ Matches Conditional=(FLAG) """
    pattern = r'Conditional=(?P<flag>[0-9A-Z_]+)'

    return _FeatureDefineMatcher(pattern, prefix='ENABLE_')


def cmake_options_matcher(macro='ENABLE'):
    """ Matches WEBKIT_OPTION_DEFINE(MACRO_FLAG) """
    pattern = r'WEBKIT_OPTION_DEFINE\((?P<flag>' + macro + r'_[0-9A-Z_]+) "(?P<description>.*)" (PUBLIC|PRIVATE) (?P<value>.*)\)'

    return _FeatureDefineMatcher(pattern)
