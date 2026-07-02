# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

from webkitcorepy import OutputCapture, mocks

from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.closebugforlanddiff import CloseBugForLandDiff


class CloseBugForLandDiffTest(unittest.TestCase):
    def test_empty_state(self):
        with mocks.Requests('commits.webkit.org', **{
            'r49824/json': mocks.Response.fromJson(dict(
                identifier='5@main',
                revision=49824,
            )),
        }):
            step = CloseBugForLandDiff(MockTool(), MockOptions())
            with OutputCapture(level=logging.INFO) as captured:
                step.run(dict(commit_text='Mock commit text'))
            self.assertEqual(
                captured.root.log.getvalue(),
                'Committed r49824 (5@main): <https://commits.webkit.org/5@main>\nNo bug id provided.\n'
            )
