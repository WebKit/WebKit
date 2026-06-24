# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Flags #includes of the StyleComputedStyle getter/setter inline headers from outside the rendering engine."""

import re


categories = {'build/include/computed-style-inlines'}


class ComputedStyleInlineIncludesChecker(object):
    """Forbids #including StyleComputedStyle+GettersInlines.h and
    StyleComputedStyle+SettersInlines.h from source files outside the
    style/, rendering/, and layout/ directories.

    These headers expand the full RenderStyle getter/setter closure, which is
    expensive to compile. Only core rendering engine code should pay that cost;
    other consumers should reach for an out-of-line helper instead.
    """

    CATEGORY = 'build/include/computed-style-inlines'

    # Source files in these directories are the core rendering engine and are
    # allowed to include the headers.
    _EXEMPT_DIRECTORIES = frozenset(('style', 'rendering', 'layout'))

    _FORBIDDEN_HEADERS = frozenset((
        'StyleComputedStyle+GettersInlines.h',
        'StyleComputedStyle+SettersInlines.h',
    ))

    # Matches a #include of either quoted or angle-bracketed form, capturing the
    # included path so we can compare its basename against the forbidden set.
    _INCLUDE_RE = re.compile(r'^\s*#\s*include\s+["<](?P<path>[^">]+)[">]')

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def _is_exempt(self):
        components = re.split(r'[/\\]', self._file_path)
        return any(component in self._EXEMPT_DIRECTORIES for component in components)

    def check(self, lines, line_numbers=None):
        if self._is_exempt():
            return
        for line_number, line in enumerate(lines, start=1):
            if line_numbers is not None and line_number not in line_numbers:
                continue
            match = self._INCLUDE_RE.match(line)
            if not match:
                continue
            basename = re.split(r'[/\\]', match.group('path'))[-1]
            if basename not in self._FORBIDDEN_HEADERS:
                continue
            self._handle_style_error(
                line_number,
                self.CATEGORY,
                5,
                '{} adds 6 seconds of compile time per translation unit, and should '
                'only be used in core rendering engine code. Consider an out of line '
                'helper function instead.'.format(basename),
            )
