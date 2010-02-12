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

from webkitpy.commands.commandtest import CommandsTest
from webkitpy.commands.upload import *
from webkitpy.mock import Mock
from webkitpy.mock_bugzillatool import MockBugzillaTool

class UploadCommandsTest(CommandsTest):
    def test_commit_message_for_current_diff(self):
        tool = MockBugzillaTool()
        mock_commit_message_for_this_commit = Mock()
        mock_commit_message_for_this_commit.message = lambda: "Mock message"
        tool._scm.commit_message_for_this_commit = lambda: mock_commit_message_for_this_commit
        expected_stdout = "Mock message\n"
        self.assert_execute_outputs(CommitMessageForCurrentDiff(), [], expected_stdout=expected_stdout, tool=tool)

    def test_clean_pending_commit(self):
        self.assert_execute_outputs(CleanPendingCommit(), [])

    def test_assign_to_committer(self):
        tool = MockBugzillaTool()
        expected_stderr = "Warning, attachment 128 on bug 42 has invalid committer (non-committer@example.com)\nBug 77 is already assigned to foo@foo.com (None).\nBug 76 has no non-obsolete patches, ignoring.\n"
        self.assert_execute_outputs(AssignToCommitter(), [], expected_stderr=expected_stderr, tool=tool)
        tool.bugs.reassign_bug.assert_called_with(42, "eric@webkit.org", "Attachment 128 was posted by a committer and has review+, assigning to Eric Seidel for commit.")

    def test_obsolete_attachments(self):
        expected_stderr = "Obsoleting 2 old patches on bug 42\n"
        self.assert_execute_outputs(ObsoleteAttachments(), [42], expected_stderr=expected_stderr)

    def test_post(self):
        expected_stderr = "Running check-webkit-style\nObsoleting 2 old patches on bug 42\n"
        self.assert_execute_outputs(Post(), [42], expected_stderr=expected_stderr)

    def test_post(self):
        expected_stderr = "Obsoleting 2 old patches on bug 42\n"
        self.assert_execute_outputs(LandSafely(), [42], expected_stderr=expected_stderr)

    def test_prepare_diff_with_arg(self):
        self.assert_execute_outputs(Prepare(), [42])

    def test_prepare(self):
        self.assert_execute_outputs(Prepare(), [])

    def test_upload(self):
        expected_stderr = "Running check-webkit-style\nObsoleting 2 old patches on bug 42\nMOCK: user.open_url: http://example.com/42\n"
        self.assert_execute_outputs(Upload(), [42], expected_stderr=expected_stderr)

    def test_mark_bug_fixed(self):
        tool = MockBugzillaTool()
        tool._scm.last_svn_commit_log = lambda: "r9876 |"
        options = Mock()
        options.bug_id = 42
        expected_stderr = """Bug: <http://example.com/42> Bug with two r+'d and cq+'d patches, one of which has an invalid commit-queue setter.
Revision: 9876
MOCK: user.open_url: http://example.com/42
Adding comment to Bug 42.
"""
        self.assert_execute_outputs(MarkBugFixed(), [], expected_stderr=expected_stderr, tool=tool, options=options)

    def test_edit_changelog(self):
        self.assert_execute_outputs(EditChangeLogs(), [])
