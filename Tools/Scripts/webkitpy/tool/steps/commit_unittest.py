# Copyright (C) 2012 Google Inc. All rights reserved.
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

import os
import unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.tool.mocktool import MockOptions, MockTool
from webkitpy.tool.steps.commit import Commit


class CommitTest(unittest.TestCase):
    def test_check_test_expectations(self):
        capture = OutputCapture()
        options = MockOptions()
        options.git_commit = ""

        tool = MockTool()
        step = Commit(tool, options)
        state = {
            "changed_files": ["test_expectations.txtXXX"],
        }

        tool.executive = MockExecutive(should_log=True, should_throw_when_run=False)
        capture.assert_outputs(self, step.run, [state], expected_stderr="Committed r49824: <http://trac.webkit.org/changeset/49824>\n")

        state = {
            "changed_files": ["platform/chromium/test_expectations.txt"],
        }
        capture.assert_outputs(self, step.run, [state], expected_stderr="MOCK run_and_throw_if_fail: ['mock-check-webkit-style', '--diff-files', 'platform/chromium/test_expectations.txt'], cwd=/mock-checkout\nCommitted r49824: <http://trac.webkit.org/changeset/49824>\n")

        tool.executive = MockExecutive(should_log=True, should_throw_when_run=set(["platform/chromium/test_expectations.txt"]))
        self.assertRaises(SystemExit, capture.assert_outputs, self, step.run, [state])
