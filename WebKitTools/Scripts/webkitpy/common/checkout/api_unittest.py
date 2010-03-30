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

import os
import shutil
import tempfile
import unittest

from webkitpy.common.checkout.api import Checkout
from webkitpy.common.checkout.scm import detect_scm_system
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock

# FIXME: Copied from scm_unittest.py
def write_into_file_at_path(file_path, contents):
    new_file = open(file_path, 'w')
    new_file.write(contents)
    new_file.close()


_changelog1entry1 = """2010-03-25  Eric Seidel  <eric@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage
"""
_changelog1entry2 = """2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py:
"""
_changelog1 = "\n".join([_changelog1entry1, _changelog1entry2])
_changelog2 = """2010-03-25  Eric Seidel  <eric@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Second part of this complicated change.

        * Path/To/Complicated/File: Added.

2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Filler change.
"""

class CommitMessageForThisCommitTest(unittest.TestCase):
    expected_commit_message = """2010-03-25  Eric Seidel  <eric@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage
2010-03-25  Eric Seidel  <eric@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Second part of this complicated change.

        * Path/To/Complicated/File: Added.
"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(suffix="changelogs")
        self.old_cwd = os.getcwd()
        os.chdir(self.temp_dir)
        write_into_file_at_path("ChangeLog1", _changelog1)
        write_into_file_at_path("ChangeLog2", _changelog2)

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)
        os.chdir(self.old_cwd)

    # FIXME: This should not need to touch the file system, however
    # ChangeLog is difficult to mock at current.
    def test_commit_message_for_this_commit(self):
        checkout = Checkout(None)
        checkout.modified_changelogs = lambda: ["ChangeLog1", "ChangeLog2"]
        output = OutputCapture()
        expected_stderr = "Parsing ChangeLog: ChangeLog1\nParsing ChangeLog: ChangeLog2\n"
        commit_message = output.assert_outputs(self, checkout.commit_message_for_this_commit, expected_stderr=expected_stderr)
        self.assertEqual(commit_message.message(), self.expected_commit_message)


class CheckoutTest(unittest.TestCase):
    def test_latest_entry_for_changelog_at_revision(self):
        scm = Mock()
        def mock_contents_at_revision(changelog_path, revision):
            self.assertEqual(changelog_path, "foo")
            self.assertEqual(revision, "bar")
            return _changelog1
        scm.contents_at_revision = mock_contents_at_revision
        checkout = Checkout(scm)
        entry = checkout._latest_entry_for_changelog_at_revision("foo", "bar")
        self.assertEqual(entry.contents(), _changelog1entry1)

    def test_commit_info_for_revision(self):
        scm = Mock()
        scm.committer_email_for_revision = lambda revision: "committer@example.com"
        checkout = Checkout(scm)
        checkout.changelog_entries_for_revision = lambda revision: ChangeLogEntry(_changelog1entry1)
        commitinfo = checkout.commit_info_for_revision(4)
        self.assertEqual(commitinfo.bug_id(), 36629)
        self.assertEqual(commitinfo.author_name(), "Eric Seidel")
        self.assertEqual(commitinfo.author_email(), "eric@webkit.org")
        self.assertEqual(commitinfo.reviewer_text(), None)
        self.assertEqual(commitinfo.reviewer(), None)
        self.assertEqual(commitinfo.committer_email(), "committer@example.com")
        self.assertEqual(commitinfo.committer(), None)
