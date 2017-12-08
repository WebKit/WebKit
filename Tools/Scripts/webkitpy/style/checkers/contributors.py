# Copyright (C) 2016 Apple Inc. All rights reserved.
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

"""Checks WebKit style for the contributors.json file."""

import difflib
import json
import re
from sets import Set
from jsonchecker import JSONChecker
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.system.filesystem import FileSystem


class ContributorsChecker(JSONChecker):
    """Processes contributors.json lines"""

    def __init__(self, file_path, handle_style_error):
        super(ContributorsChecker, self).__init__(file_path, handle_style_error)
        self._file_path = file_path

    def check(self, lines):
        super(ContributorsChecker, self).check(lines)
        canonicalized = CommitterList().as_json()
        actual = FileSystem().read_text_file(self._file_path)
        diff = self._unidiff_output(actual, canonicalized)
        if diff:
            self._handle_style_error(0, 'json/syntax', 5, 'contributors.json differs from the canonical format. Use "validate-committer-lists --canonicalize" to reformat it.')
            print(diff)

    def _unidiff_output(self, expected, actual):
        expected = expected.splitlines(1)
        actual = actual.splitlines(1)
        diff = difflib.unified_diff(expected, actual)
        return ''.join(diff)
