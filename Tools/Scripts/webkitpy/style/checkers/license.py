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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Flags new files that use the BSD license naming Google."""

import re


categories = {'legal/license'}


class LicenseChecker(object):
    """Flags new files whose license has the "Neither the name of Google Inc."
    clause. That license is copied from Google's files, but new code uses the
    two-clause BSD license.
    """

    CATEGORY = 'legal/license'

    _GOOGLE_CLAUSE_RE = re.compile(r'Neither the name of Google')

    def __init__(self, handle_style_error):
        self._handle_style_error = handle_style_error

    def check(self, lines, line_numbers=None):
        # Only a patch tells whether the file is new.
        if line_numbers is None:
            return
        added_line_numbers = set(line_numbers)
        # Splitting on newlines leaves an empty string after the final one.
        line_count = len(lines) - 1 if lines and not lines[-1] else len(lines)
        if any(line_number not in added_line_numbers for line_number in range(1, line_count + 1)):
            return
        for line_number, line in enumerate(lines, start=1):
            if self._GOOGLE_CLAUSE_RE.search(line):
                self._handle_style_error(
                    line_number,
                    self.CATEGORY,
                    5,
                    'This license names Google; you likely copied it from the wrong place. '
                    'New files should use the two-clause BSD license in Source/WebCore/LICENSE-APPLE.',
                )
                return
