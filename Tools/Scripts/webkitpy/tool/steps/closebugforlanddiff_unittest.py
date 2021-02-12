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
from webkitpy.tool.steps.closebugforlanddiff import CloseBugForLandDiff

from webkitcorepy import OutputCapture
from webkitscmpy import mocks, Commit


class CloseBugForLandDiffTest(unittest.TestCase):
    def test_empty_state(self):
        with mocks.remote.Svn('svn.webkit.org/repository/webkit') as repo:
            repo.commits['trunk'].append(Commit(
                author=dict(name='Dmitry Titov', emails=['dimich@chromium.org']),
                identifier='5@trunk',
                revision=49824,
                timestamp=1601668000,
                message=
                    'Manual Test for crash caused by JS accessing DOMWindow which is disconnected from the Frame.\n'
                    'https://bugs.webkit.org/show_bug.cgi?id=30544\n'
                    '\n'
                    'Reviewed by Darin Adler.\n'
                    '\n'
                    '    manual-tests/crash-on-accessing-domwindow-without-frame.html: Added.\n',
            ))

            step = CloseBugForLandDiff(MockTool(), MockOptions())
            with OutputCapture(level=logging.INFO) as captured:
                step.run(dict(commit_text='Mock commit text'))
            self.assertEqual(
                captured.root.log.getvalue(),
                'Committed r49824 (5@main): <https://commits.webkit.org/5@main>\nNo bug id provided.\n'
            )
