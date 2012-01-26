# Copyright (C) 2011 Google Inc. All rights reserved.
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

from .deps_mock import MockDEPS
from .commitinfo import CommitInfo

# FIXME: These imports are wrong, we should use a shared MockCommittersList.
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.net.bugzilla.bugzilla_mock import _mock_reviewers


class MockCommitMessage(object):
    def message(self):
        return "This is a fake commit message that is at least 50 characters."


class MockCheckout(object):

    # FIXME: This should move onto the Host object, and we should use a MockCommitterList for tests.
    _committer_list = CommitterList()

    def commit_info_for_revision(self, svn_revision):
        # The real Checkout would probably throw an exception, but this is the only way tests have to get None back at the moment.
        if not svn_revision:
            return None
        return CommitInfo(svn_revision, "eric@webkit.org", {
            "bug_id": 50000,
            "author_name": "Adam Barth",
            "author_email": "abarth@webkit.org",
            "author": self._committer_list.contributor_by_email("abarth@webkit.org"),
            "reviewer_text": "Darin Adler",
            "reviewer": self._committer_list.committer_by_name("Darin Adler"),
            "changed_files": [
                "path/to/file",
                "another/file",
            ],
        })

    def is_path_to_changelog(self, path):
        # FIXME: This should self._filesystem.basename.
        return os.path.basename(path) == "ChangeLog"

    def bug_id_for_revision(self, svn_revision):
        return 12345

    def recent_commit_infos_for_files(self, paths):
        return [self.commit_info_for_revision(32)]

    def modified_changelogs(self, git_commit, changed_files=None):
        # Ideally we'd return something more interesting here.  The problem is
        # that LandDiff will try to actually read the patch from disk!
        return []

    def commit_message_for_this_commit(self, git_commit, changed_files=None):
        return MockCommitMessage()

    def chromium_deps(self):
        return MockDEPS()

    def apply_patch(self, patch):
        pass

    def apply_reverse_diffs(self, revision):
        pass

    def suggested_reviewers(self, git_commit, changed_files=None):
        # FIXME: We should use a shared mock commiter list.
        return [_mock_reviewers[0]]
