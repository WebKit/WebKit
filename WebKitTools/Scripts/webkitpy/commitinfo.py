# Copyright (c) 2010, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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
#
# WebKit's python module for holding information on a commit

import StringIO

from webkitpy.bugzilla import parse_bug_id
from webkitpy.changelogs import ChangeLog, is_path_to_changelog
from webkitpy.committers import CommitterList


class CommitInfo(object):
    @classmethod
    def _latest_entry_for_changelog_at_revision(cls, scm, changelog_path, revision):
        changelog_contents = scm.contents_at_revision(changelog_path, revision)
        return ChangeLog.parse_latest_entry_from_file(StringIO.StringIO(changelog_contents))

    @classmethod
    def _changelog_entries_for_revision(cls, scm, revision):
        changed_files = scm.changed_files_for_revision(revision)
        return [cls._latest_entry_for_changelog_at_revision(scm, path, revision) for path in changed_files if is_path_to_changelog(path)]

    @classmethod
    def commit_info_for_revision(cls, scm, revision):
        committer_email = scm.committer_email_for_revision(revision)
        changelog_entries = cls._changelog_entries_for_revision(scm, revision)
        # Assume for now that the first entry has everything we need:
        changelog_entry = changelog_entries[0]
        changelog_data = {
            "bug_id" : parse_bug_id(changelog_entry.contents()),
            "author_name" : changelog_entry.author_name(),
            "author_email" : changelog_entry.author_email(),
            "reviewer_text" : changelog_entry.reviewer_text(),
        }
        # We could pass the changelog_entry instead of a dictionary here, but that makes
        # mocking slightly more involved, and would make aggregating data from multiple
        # entries more difficult to wire in if we need to do that in the future.
        return cls(revision, committer_email, changelog_data)

    def __init__(self, revision, committer_email, changelog_data, committer_list=CommitterList()):
        self._revision = revision
        self._committer_email = committer_email
        self._bug_id = changelog_data["bug_id"]
        self._author_name = changelog_data["author_name"]
        self._author_email = changelog_data["author_email"]
        self._reviewer_text = changelog_data["reviewer_text"]

        # Derived values:
        self._reviewer = committer_list.committer_by_name(self._reviewer_text)
        self._author = committer_list.committer_by_email(self._author_email) or committer_list.committer_by_name(self._author_name)
        self._committer = committer_list.committer_by_email(committer_email)

    def revision(self):
        return self._revision

    def committer(self):
        return self._committer # Should never be None

    def committer_email(self):
        return self._committer_email

    def bug_id(self):
        return self._bug_id # May be None

    def author(self):
        return self._author # May be None

    def author_name(self):
        return self._author_name

    def author_email(self):
        return self._author_email

    def reviewer(self):
        return self._reviewer # May be None

    def reviewer_text(self):
        return self._reviewer_text # May be None
