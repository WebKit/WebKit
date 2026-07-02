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

"""Flags <script src="..."> includes of the deprecated js-test-pre.js / js-test-post.js / js-test-post-async.js."""

import re


categories = {'build/deprecated/js-test-helpers'}


class DeprecatedJSTestIncludesChecker(object):
    """Forbids including the deprecated js-test-pre.js, js-test-post.js, and
    js-test-post-async.js helpers.

    LayoutTests authors should use js-test.js instead, which combines the
    functionality of the old pre/post(/post-async) triad.
    """

    CATEGORY = 'build/deprecated/js-test-helpers'

    # Matches <script ... src="...js-test-pre.js">, the js-test-post.js
    # counterpart, and js-test-post-async.js, with either quote style and an
    # optional query string. Listing post-async before post in the alternation
    # ensures it wins the longest-match for post-async filenames.
    _DEPRECATED_INCLUDE_RE = re.compile(
        r'''<script\b[^>]*\bsrc\s*=\s*["'][^"']*\b(?P<filename>js-test-post-async\.js|js-test-pre\.js|js-test-post\.js)(?:\?[^"']*)?["']''',
        re.IGNORECASE,
    )

    def __init__(self, handle_style_error):
        self._handle_style_error = handle_style_error

    def check(self, lines, line_numbers=None):
        for line_number, line in enumerate(lines):
            match = self._DEPRECATED_INCLUDE_RE.search(line)
            if not match:
                continue
            deprecated = match.group('filename').lower()
            self._handle_style_error(
                line_number + 1,
                self.CATEGORY,
                5,
                '{} is deprecated; include LayoutTests/resources/js-test.js instead.'.format(deprecated),
            )
