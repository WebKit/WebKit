# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

import unittest

from webkitpy.common.checkout.changelog import ChangeLogEntry
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.validatereviewer import ValidateReviewer

class ValidateReviewerTest(unittest.TestCase):
    _boilerplate_entry = '''2009-08-19  Eric Seidel  <eric@webkit.org>

        REVIEW_LINE

        * Scripts/bugzilla-tool:
'''

    def _test_review_text(self, step, text, expected):
        contents = self._boilerplate_entry.replace("REVIEW_LINE", text)
        entry = ChangeLogEntry(contents)
        self.assertEqual(step._has_valid_reviewer(entry), expected)

    def test_has_valid_reviewer(self):
        step = ValidateReviewer(MockTool(), MockOptions())
        self._test_review_text(step, "Reviewed by Eric Seidel.", True)
        self._test_review_text(step, "Reviewed by Eric Seidel", True) # Not picky about the '.'
        self._test_review_text(step, "Reviewed by Eric.", False)
        self._test_review_text(step, "Reviewed by Eric C Seidel.", False)
        self._test_review_text(step, "Rubber-stamped by Eric.", True)
        self._test_review_text(step, "Rubber stamped by Eric.", True)
        self._test_review_text(step, "Unreviewed build fix.", True)
