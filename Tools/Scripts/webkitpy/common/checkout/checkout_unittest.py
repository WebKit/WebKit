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
import unittest

from webkitcorepy import string_utils

from webkitpy.common.checkout.checkout import Checkout
from webkitpy.common.checkout.changelog import ChangeLogEntry
from webkitpy.common.checkout.scm import CommitMessage, SCMDetector
from webkitpy.common.checkout.scm.scm_mock import MockSCM
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem import FileSystem  # FIXME: This should not be needed.
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.thirdparty.mock import Mock


_changelog1entry1 = u"""2010-03-25  Fr\u00e9d\u00e9ric Wang  <fred.wang@free.fr>

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
_changelog2 = u"""2010-03-25  Fr\u00e9d\u00e9ric Wang  <fred.wang@free.fr>

        Unreviewed build fix to un-break webkit-patch land.

        Second part of this complicated change by me, Fr\u00e9d\u00e9ric Wang!

        * Path/To/Complicated/File: Added.

2010-03-25  Adam Barth  <abarth@webkit.org>

        Reviewed by Eric Seidel.

        Filler change.
"""

_changelog3 = u"""2014-07-17  David Kilzer  <ddkilzer@apple.com>

        SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
        <http://webkit.org/b/135006>

        Reviewed by Darin Adler.

        * WebKit.xcodeproj/project.pbxproj: Remove references to unused
        WebKit.xcconfig file.
"""

_changelog4 = u"""2014-07-17  David Kilzer  <ddkilzer@apple.com>

        SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
        <http://webkit.org/b/135006>

        Reviewed by Darin Adler.

        * Path/To/Complicated/File: Added.
"""

_changelog5 = u"""2014-07-17  David Kilzer  <ddkilzer@apple.com>

        SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
        <http://webkit.org/b/135006>

        Reviewed by Darin Adler.

        Filler change.

        * Path/To/Complicated/File: Added.
"""

_changelog6 = u"""2014-06-23  Daniel Bates  <dabates@apple.com>

        Reverting r170340 and r170339.

        Changeset r170339 broke the Apple Windows Debug and Release builds.

        Reverted changesets:

        "[Win] Build fix after r134209"
        https://trac.webkit.org/changeset/170340

        "[Win] Clean up and refactor WinLauncher"
        https://bugs.webkit.org/show_bug.cgi?id=134209
        https://trac.webkit.org/changeset/170339
"""


class CommitMessageForThisCommitTest(unittest.TestCase):
    def setUp(self):
        # FIXME: This should not need to touch the filesystem, however
        # ChangeLog is difficult to mock at current.
        self.filesystem = FileSystem()
        self.temp_dir = str(self.filesystem.mkdtemp(suffix="changelogs"))
        self.old_cwd = self.filesystem.getcwd()
        self.filesystem.chdir(self.temp_dir)
        self.webkit_base = WebKitFinder(self.filesystem).webkit_base()

        # Trick commit-log-editor into thinking we're in a Subversion working copy so it won't
        # complain about not being able to figure out what SCM is in use.
        # FIXME: VCSTools.pm is no longer so easily fooled.  It logs because "svn info" doesn't
        # treat a bare .svn directory being part of an svn checkout.
        self.filesystem.maybe_make_directory(".svn")

    def mock_changelog(self, changelogs):
        for path, contents in zip(self.changelog_paths, changelogs):
            self.filesystem.maybe_make_directory(self.filesystem.dirname(path))
            self.filesystem.write_text_file(path, contents)

    def tearDown(self):
        self.filesystem.rmtree(self.temp_dir)
        self.filesystem.chdir(self.old_cwd)

    def mock_checkout_for_test(self):
        executive = Executive()

        def mock_run(*args, **kwargs):
            # Note that we use a real Executive here, not a MockExecutive, so we can test that we're
            # invoking commit-log-editor correctly.
            env = os.environ.copy()
            env['CHANGE_LOG_EMAIL_ADDRESS'] = 'fred.wang@free.fr'
            kwargs['env'] = env
            return executive.run_command(*args, **kwargs)

        detector = SCMDetector(self.filesystem, executive)
        real_scm = detector.detect_scm_system(self.webkit_base)

        mock_scm = MockSCM()
        mock_scm.run = mock_run

        real_checkout = Checkout(real_scm)
        checkout = Checkout(mock_scm)
        checkout.script_path = real_checkout.script_path
        checkout.modified_changelogs = lambda git_commit, changed_files=None: self.changelog_paths

        return checkout

    def test_commit_message_for_unreviewed_changelogs_with_different_messages(self):
        expected_commit_message = u"""Unreviewed build fix to un-break webkit-patch land.

Tools:

Move commit_message_for_this_commit from scm to checkout
https://bugs.webkit.org/show_bug.cgi?id=36629

* Scripts/webkitpy/common/checkout/api.py: import scm.CommitMessage

LayoutTests:

Second part of this complicated change by me, Fr\u00e9d\u00e9ric Wang!

* Path/To/Complicated/File: Added.
"""

        self.changelog_paths = list(map(self.filesystem.abspath, (self.filesystem.join("Tools", "ChangeLog"), self.filesystem.join("LayoutTests", "ChangeLog"))))

        self.mock_changelog((_changelog1, _changelog2))
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)

    def test_commit_message_for_one_reviewed_changelog(self):
        expected_commit_message = u"""SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
<http://webkit.org/b/135006>

Patch by David Kilzer <ddkilzer@apple.com> on 2014-07-17
Reviewed by Darin Adler.

* WebKit.xcodeproj/project.pbxproj: Remove references to unused
WebKit.xcconfig file.
"""

        self.changelog_paths = list(map(self.filesystem.abspath, [self.filesystem.join("Source/WebKitLegacy", "ChangeLog")]))

        self.mock_changelog([_changelog3])
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)

    def test_commit_message_for_changelogs_with_same_messages(self):
        expected_commit_message = u"""SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
<http://webkit.org/b/135006>

Patch by David Kilzer <ddkilzer@apple.com> on 2014-07-17
Reviewed by Darin Adler.

Source/WebKitLegacy:

* WebKit.xcodeproj/project.pbxproj: Remove references to unused
WebKit.xcconfig file.

LayoutTests:

* Path/To/Complicated/File: Added.
"""

        self.changelog_paths = list(map(self.filesystem.abspath, (self.filesystem.join("Source/WebKitLegacy", "ChangeLog"), self.filesystem.join("LayoutTests", "ChangeLog"))))

        self.mock_changelog((_changelog3, _changelog4))
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)

    def test_commit_message_for_changelogs_with_different_messages(self):
        expected_commit_message = u"""SECTORDER_FLAGS should be defined in target's xcconfig file, not Base.xcconfig
<http://webkit.org/b/135006>

Patch by David Kilzer <ddkilzer@apple.com> on 2014-07-17
Reviewed by Darin Adler.

Source/WebKitLegacy:

* WebKit.xcodeproj/project.pbxproj: Remove references to unused
WebKit.xcconfig file.

LayoutTests:

Filler change.

* Path/To/Complicated/File: Added.
"""

        self.changelog_paths = list(map(self.filesystem.abspath, (self.filesystem.join("Source/WebKitLegacy", "ChangeLog"), self.filesystem.join("LayoutTests", "ChangeLog"))))

        self.mock_changelog((_changelog3, _changelog5))
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)

    def test_commit_message_for_one_revert_changelog(self):
        expected_commit_message = u"""Reverting r170340 and r170339.

Changeset r170339 broke the Apple Windows Debug and Release builds.

Reverted changesets:

"[Win] Build fix after r134209"
https://trac.webkit.org/changeset/170340

"[Win] Clean up and refactor WinLauncher"
https://bugs.webkit.org/show_bug.cgi?id=134209
https://trac.webkit.org/changeset/170339

Patch by Daniel Bates <dabates@apple.com> on 2014-06-23
"""

        self.changelog_paths = list(map(self.filesystem.abspath, [self.filesystem.join("Tools", "ChangeLog")]))

        self.mock_changelog([_changelog6])
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)

    def test_commit_message_for_revert_changelogs_with_different_directories(self):
        expected_commit_message = u"""Reverting r170340 and r170339.

Changeset r170339 broke the Apple Windows Debug and Release builds.

Reverted changesets:

"[Win] Build fix after r134209"
https://trac.webkit.org/changeset/170340

"[Win] Clean up and refactor WinLauncher"
https://bugs.webkit.org/show_bug.cgi?id=134209
https://trac.webkit.org/changeset/170339

Patch by Daniel Bates <dabates@apple.com> on 2014-06-23
"""

        self.changelog_paths = list(map(self.filesystem.abspath, (self.filesystem.join("Tools", "ChangeLog"), self.filesystem.join("Source/WebCore", "ChangeLog"))))

        self.mock_changelog((_changelog6, _changelog6))
        checkout = self.mock_checkout_for_test()
        commit_message = checkout.commit_message_for_this_commit(git_commit=None, return_stderr=True)
        # Throw away the first line - a warning about unknown VCS root.
        commit_message.message_lines = commit_message.message_lines[1:]
        self.assertMultiLineEqual(commit_message.message(), expected_commit_message)


class CheckoutTest(unittest.TestCase):
    def _make_checkout(self):
        return Checkout(scm=MockSCM(), filesystem=MockFileSystem(), executive=MockExecutive())

    def test_latest_entry_for_changelog_at_revision(self):
        def mock_contents_at_revision(changelog_path, revision):
            self.assertEqual(changelog_path, "foo")
            self.assertEqual(revision, "bar")
            # contents_at_revision is expected to return a byte array (str)
            # so we encode our unicode ChangeLog down to a utf-8 stream.
            # The ChangeLog utf-8 decoding should ignore invalid codepoints.
            invalid_utf8 = b'\255'
            return string_utils.encode(_changelog1) + invalid_utf8
        checkout = self._make_checkout()
        checkout._scm.contents_at_revision = mock_contents_at_revision
        entry = checkout._latest_entry_for_changelog_at_revision("foo", "bar")
        self.assertMultiLineEqual(entry.contents(), _changelog1entry1)  # Pylint is confused about this line, pylint: disable=E1101

    # FIXME: This tests a hack around our current changed_files handling.
    # Right now changelog_entries_for_revision tries to fetch deleted files
    # from revisions, resulting in a ScriptError exception.  Test that we
    # recover from those and still return the other ChangeLog entries.
    def test_changelog_entries_for_revision(self):
        checkout = self._make_checkout()
        checkout._scm.changed_files_for_revision = lambda revision: ['foo/ChangeLog', 'bar/ChangeLog']

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
        checkout = self._make_checkout()
        checkout._scm.changed_files_for_revision = lambda revision: ['path/to/file', 'another/file']
        checkout._scm.committer_email_for_revision = lambda revision, changed_files=None: "committer@example.com"
        checkout.changelog_entries_for_revision = lambda revision, changed_files=None: [ChangeLogEntry(_changelog1entry1)]
        commitinfo = checkout.commit_info_for_revision(4)
        self.assertEqual(commitinfo.bug_id(), 36629)
        self.assertEqual(commitinfo.bug_description(), "Unreviewed build fix to un-break webkit-patch land.")
        self.assertEqual(commitinfo.author_name(), u"Fr\u00e9d\u00e9ric Wang")
        self.assertEqual(commitinfo.author_email(), "fred.wang@free.fr")
        self.assertIsNone(commitinfo.reviewer_text())
        self.assertIsNone(commitinfo.reviewer())
        self.assertEqual(commitinfo.committer_email(), "committer@example.com")
        self.assertIsNone(commitinfo.committer())
        self.assertEqual(commitinfo.to_json(), {
            'bug_id': 36629,
            'author_email': 'fred.wang@free.fr',
            'changed_files': [
                'path/to/file',
                'another/file',
            ],
            'reviewer_text': None,
            'author_name': u'Fr\u00e9d\u00e9ric Wang',
            'bug_description': 'Unreviewed build fix to un-break webkit-patch land.',
        })

        checkout.changelog_entries_for_revision = lambda revision, changed_files=None: []
        self.assertIsNone(checkout.commit_info_for_revision(1))

    def test_bug_id_for_revision(self):
        checkout = self._make_checkout()
        checkout._scm.committer_email_for_revision = lambda revision: "committer@example.com"
        checkout.changelog_entries_for_revision = lambda revision, changed_files=None: [ChangeLogEntry(_changelog1entry1)]
        self.assertEqual(checkout.bug_id_for_revision(4), 36629)

    def test_bug_id_for_this_commit(self):
        checkout = self._make_checkout()
        checkout.commit_message_for_this_commit = lambda git_commit, changed_files=None, return_stderr=False: CommitMessage(ChangeLogEntry(_changelog1entry1).contents().splitlines())
        self.assertEqual(checkout.bug_id_for_this_commit(git_commit=None), 36629)

    def test_modified_changelogs(self):
        checkout = self._make_checkout()
        checkout._scm.checkout_root = "/foo/bar"
        checkout._scm.changed_files = lambda git_commit: ["file1", "ChangeLog", "relative/path/ChangeLog"]
        expected_changlogs = ["/foo/bar/ChangeLog", "/foo/bar/relative/path/ChangeLog"]
        self.assertEqual(checkout.modified_changelogs(git_commit=None), expected_changlogs)

    def test_suggested_reviewers(self):
        def mock_changelog_entries_for_revision(revision, changed_files=None):
            if revision == 27:
                return []
            if revision % 2 == 0:
                return [ChangeLogEntry(_changelog1entry1)]
            return [ChangeLogEntry(_changelog1entry2)]

        def mock_revisions_changing_file(path, limit=5):
            if path.endswith('ChangeLog'):
                return [3]
            if path.endswith('file_with_empty_changelog'):
                return [27]
            return [4, 8]

        checkout = self._make_checkout()
        checkout._scm.checkout_root = '/foo/bar'
        checkout._scm.changed_files = lambda git_commit: ['file1', 'file2', 'relative/path/ChangeLog', 'file_with_empty_changelog']
        checkout._scm.revisions_changing_file = mock_revisions_changing_file
        checkout.changelog_entries_for_revision = mock_changelog_entries_for_revision
        reviewers = checkout.suggested_reviewers(git_commit=None)
        reviewer_names = [reviewer.full_name for reviewer in reviewers]
        self.assertEqual(reviewer_names, [u'Fr\u00e9d\u00e9ric Wang'])

    def test_apply_patch(self):
        checkout = self._make_checkout()
        checkout._executive = MockExecutive(should_log=True)
        checkout.script_path = lambda script: script
        mock_patch = Mock()
        mock_patch.contents = lambda: "foo"
        mock_patch.reviewer = lambda: None
        expected_logs = "MOCK run_command: ['svn-apply', '--force'], cwd=/mock-checkout, input=foo\n"
        OutputCapture().assert_outputs(self, checkout.apply_patch, [mock_patch], expected_logs=expected_logs)
