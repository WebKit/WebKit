# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import logging
import unittest

from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.suggestreviewers import SuggestReviewers

from webkitcorepy import OutputCapture


class SuggestReviewersTest(unittest.TestCase):
    def test_disabled(self):
        step = SuggestReviewers(MockTool(), MockOptions(suggest_reviewers=False))
        with OutputCapture(level=logging.INFO) as captured:
            step.run({})
        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_basic(self):
        capture = OutputCapture()
        step = SuggestReviewers(MockTool(), MockOptions(suggest_reviewers=True, git_commit=None))
        with OutputCapture(level=logging.INFO) as captured:
            step.run(dict(bug_id='123'))
        self.assertEqual(captured.stdout.getvalue(), 'The following reviewers have recently modified files in your patch:\nFoo Bar\n')
        self.assertEqual(captured.root.log.getvalue(), 'Would you like to CC them?\n')
