# Copyright (C) 2009 Google Inc. All rights reserved.
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

from webkitpy.common.net.bugzilla import Bugzilla
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.commands.queries import *
from webkitpy.tool.mocktool import MockTool


class QueryCommandsTest(CommandsTest):
    def test_bugs_to_commit(self):
        expected_stderr = "Warning, attachment 128 on bug 42 has invalid committer (non-committer@example.com)\n"
        self.assert_execute_outputs(BugsToCommit(), None, "42\n77\n", expected_stderr)

    def test_patches_in_commit_queue(self):
        expected_stdout = "http://example.com/197\nhttp://example.com/103\n"
        expected_stderr = "Warning, attachment 128 on bug 42 has invalid committer (non-committer@example.com)\nPatches in commit queue:\n"
        self.assert_execute_outputs(PatchesInCommitQueue(), None, expected_stdout, expected_stderr)

    def test_patches_to_commit_queue(self):
        expected_stdout = "http://example.com/104&action=edit\n"
        expected_stderr = "197 already has cq=+\n128 already has cq=+\n105 committer = \"Eric Seidel\" <eric@webkit.org>\n"
        options = Mock()
        options.bugs = False
        self.assert_execute_outputs(PatchesToCommitQueue(), None, expected_stdout, expected_stderr, options=options)

        expected_stdout = "http://example.com/77\n"
        options.bugs = True
        self.assert_execute_outputs(PatchesToCommitQueue(), None, expected_stdout, expected_stderr, options=options)

    def test_patches_to_review(self):
        expected_stdout = "103\n"
        expected_stderr = "Patches pending review:\n"
        self.assert_execute_outputs(PatchesToReview(), None, expected_stdout, expected_stderr)

    def test_tree_status(self):
        expected_stdout = "ok   : Builder1\nok   : Builder2\n"
        self.assert_execute_outputs(TreeStatus(), None, expected_stdout)

    def test_skipped_ports(self):
        expected_stdout = "Ports skipping test 'media/foo/bar.html': test_port1, test_port2\n"
        self.assert_execute_outputs(SkippedPorts(), ("media/foo/bar.html",), expected_stdout)

        expected_stdout = "Ports skipping test 'foo': test_port1\n"
        self.assert_execute_outputs(SkippedPorts(), ("foo",), expected_stdout)

        expected_stdout = "Test 'media' is not skipped by any port.\n"
        self.assert_execute_outputs(SkippedPorts(), ("media",), expected_stdout)


class FailureReasonTest(unittest.TestCase):
    def test_blame_line_for_revision(self):
        tool = MockTool()
        command = FailureReason()
        command.bind_to_tool(tool)
        # This is an artificial example, mostly to test the CommitInfo lookup failure case.
        self.assertEquals(command._blame_line_for_revision(None), "FAILED to fetch CommitInfo for rNone, likely missing ChangeLog")

        def raising_mock(self):
            raise Exception("MESSAGE")
        tool.checkout().commit_info_for_revision = raising_mock
        self.assertEquals(command._blame_line_for_revision(None), "FAILED to fetch CommitInfo for rNone, exception: MESSAGE")
