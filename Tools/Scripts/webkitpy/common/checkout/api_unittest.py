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

from __future__ import with_statement

import codecs
import os
import shutil
import tempfile
import unittest

from webkitpy.common.checkout.api import Checkout
from webkitpy.common.checkout.changelog import ChangeLogEntry
from webkitpy.common.checkout.scm import detect_scm_system, CommitMessage
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.executive import ScriptError
from webkitpy.thirdparty.mock import Mock


# FIXME: Copied from scm_unittest.py
def write_into_file_at_path(file_path, contents, encoding="utf-8"):
    with codecs.open(file_path, "w", encoding) as file:
        file.write(contents)


_changelog1entry1 = u"""2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage
"""
_changelog1entry2 = u"""2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py:
"""
_changelog1 = u"\n".join([_changelog1entry1, _changelog1entry2])
_changelog2 = u"""2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Second part of this complicated change.

        * Path/To/Complicated/File: Added.

2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Filler change.
"""

class CommitMessageForThisCommitTest(unittest.TestCase):
    expected_commit_message = u"""2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

        Unreviewed build fix to un-break webkit-patch land.

        Move commit_message_for_this_commit from scm to checkout
        https://bugs.webkit.org/show_bug.cgi?id=36629

        * Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage
2010-03-25  Tor Arne Vestb\u00f8  <vestbo@webkit.org>

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
        checkout.modified_changelogs = lambda git_commit, changed_files=None: ["ChangeLog1", "ChangeLog2"]
        output = OutputCapture()
        expected_stderr = "Parsing ChangeLog: ChangeLog1\nParsing ChangeLog: ChangeLog2\n"
        commit_message = output.assert_outputs(self, checkout.commit_message_for_this_commit,
            kwargs={"git_commit": None}, expected_stderr=expected_stderr)
        self.assertEqual(commit_message.message(), self.expected_commit_message)


class CheckoutTest(unittest.TestCase):
    def test_latest_entry_for_changelog_at_revision(self):
        scm = Mock()
        def mock_contents_at_revision(changelog_path, revision):
            self.assertEqual(changelog_path, "foo")
            self.assertEqual(revision, "bar")
            # contents_at_revision is expected to return a byte array (str)
            # so we encode our unicode ChangeLog down to a utf-8 stream.
            # The ChangeLog utf-8 decoding should ignore invalid codepoints.
            invalid_utf8 = "\255"
            return _changelog1.encode("utf-8") + invalid_utf8
        scm.contents_at_revision = mock_contents_at_revision
        checkout = Checkout(scm)
        entry = checkout._latest_entry_for_changelog_at_revision("foo", "bar")
        self.assertEqual(entry.contents(), _changelog1entry1)

    # FIXME: This tests a hack around our current changed_files handling.
    # Right now changelog_entries_for_revision tries to fetch deleted files
    # from revisions, resulting in a ScriptError exception.  Test that we
    # recover from those and still return the other ChangeLog entries.
    def test_changelog_entries_for_revision(self):
        scm = Mock()
        scm.changed_files_for_revision = lambda revision: ['foo/ChangeLog', 'bar/ChangeLog']
        checkout = Checkout(scm)

        def mock_latest_entry_for_changelog_at_revision(path, revision):
            if path == "foo/ChangeLog":
                return 'foo'
            raise ScriptError()

        checkout._latest_entry_for_changelog_at_revision = mock_latest_entry_for_changelog_at_revision

        # Even though fetching one of the entries failed, the other should succeed.
        entries = checkout.changelog_entries_for_revision(1)
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0], 'foo')

    def test_commit_info_for_revision(self):
        scm = Mock()
        scm.committer_email_for_revision = lambda revision: "committer@example.com"
        checkout = Checkout(scm)
        checkout.changelog_entries_for_revision = lambda revision: [ChangeLogEntry(_changelog1entry1)]
        commitinfo = checkout.commit_info_for_revision(4)
        self.assertEqual(commitinfo.bug_id(), 36629)
        self.assertEqual(commitinfo.author_name(), u"Tor Arne Vestb\u00f8")
        self.assertEqual(commitinfo.author_email(), "vestbo@webkit.org")
        self.assertEqual(commitinfo.reviewer_text(), None)
        self.assertEqual(commitinfo.reviewer(), None)
        self.assertEqual(commitinfo.committer_email(), "committer@example.com")
        self.assertEqual(commitinfo.committer(), None)

        checkout.changelog_entries_for_revision = lambda revision: []
        self.assertEqual(checkout.commit_info_for_revision(1), None)

    def test_bug_id_for_revision(self):
        scm = Mock()
        scm.committer_email_for_revision = lambda revision: "committer@example.com"
        checkout = Checkout(scm)
        checkout.changelog_entries_for_revision = lambda revision: [ChangeLogEntry(_changelog1entry1)]
        self.assertEqual(checkout.bug_id_for_revision(4), 36629)

    def test_bug_id_for_this_commit(self):
        scm = Mock()
        checkout = Checkout(scm)
        checkout.commit_message_for_this_commit = lambda git_commit, changed_files=None: CommitMessage(ChangeLogEntry(_changelog1entry1).contents().splitlines())
        self.assertEqual(checkout.bug_id_for_this_commit(git_commit=None), 36629)

    def test_modified_changelogs(self):
        scm = Mock()
        scm.checkout_root = "/foo/bar"
        scm.changed_files = lambda git_commit: ["file1", "ChangeLog", "relative/path/ChangeLog"]
        checkout = Checkout(scm)
        expected_changlogs = ["/foo/bar/ChangeLog", "/foo/bar/relative/path/ChangeLog"]
        self.assertEqual(checkout.modified_changelogs(git_commit=None), expected_changlogs)

    def test_suggested_reviewers(self):
        def mock_changelog_entries_for_revision(revision):
            if revision % 2 == 0:
                return [ChangeLogEntry(_changelog1entry1)]
            return [ChangeLogEntry(_changelog1entry2)]

        def mock_revisions_changing_file(path, limit=5):
            if path.endswith("ChangeLog"):
                return [3]
            return [4, 8]

        scm = Mock()
        scm.checkout_root = "/foo/bar"
        scm.changed_files = lambda git_commit: ["file1", "file2", "relative/path/ChangeLog"]
        scm.revisions_changing_file = mock_revisions_changing_file
        checkout = Checkout(scm)
        checkout.changelog_entries_for_revision = mock_changelog_entries_for_revision
        reviewers = checkout.suggested_reviewers(git_commit=None)
        reviewer_names = [reviewer.full_name for reviewer in reviewers]
        self.assertEqual(reviewer_names, [u'Tor Arne Vestb\xf8'])
