# Copyright (c) 2010 Google Inc. All rights reserved.
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

import os
import StringIO

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.checkout.scm import CommitMessage
from webkitpy.common.memoized import memoized
from webkitpy.common.net.bugzilla import parse_bug_id
from webkitpy.common.system.executive import Executive, run_command, ScriptError
from webkitpy.common.system.deprecated_logging import log


# This class represents the WebKit-specific parts of the checkout (like
# ChangeLogs).
# FIXME: Move a bunch of ChangeLog-specific processing from SCM to this object.
class Checkout(object):
    def __init__(self, scm):
        self._scm = scm

    def _is_path_to_changelog(self, path):
        return os.path.basename(path) == "ChangeLog"

    def _latest_entry_for_changelog_at_revision(self, changelog_path, revision):
        changelog_contents = self._scm.contents_at_revision(changelog_path, revision)
        # contents_at_revision returns a byte array (str()), but we know
        # that ChangeLog files are utf-8.  parse_latest_entry_from_file
        # expects a file-like object which vends unicode(), so we decode here.
        changelog_file = StringIO.StringIO(changelog_contents.decode("utf-8"))
        return ChangeLog.parse_latest_entry_from_file(changelog_file)

    def changelog_entries_for_revision(self, revision):
        changed_files = self._scm.changed_files_for_revision(revision)
        return [self._latest_entry_for_changelog_at_revision(path, revision) for path in changed_files if self._is_path_to_changelog(path)]

    @memoized
    def commit_info_for_revision(self, revision):
        committer_email = self._scm.committer_email_for_revision(revision)
        changelog_entries = self.changelog_entries_for_revision(revision)
        # Assume for now that the first entry has everything we need:
        # FIXME: This will throw an exception if there were no ChangeLogs.
        if not len(changelog_entries):
            return None
        changelog_entry = changelog_entries[0]
        changelog_data = {
            "bug_id": parse_bug_id(changelog_entry.contents()),
            "author_name": changelog_entry.author_name(),
            "author_email": changelog_entry.author_email(),
            "author": changelog_entry.author(),
            "reviewer_text": changelog_entry.reviewer_text(),
            "reviewer": changelog_entry.reviewer(),
        }
        # We could pass the changelog_entry instead of a dictionary here, but that makes
        # mocking slightly more involved, and would make aggregating data from multiple
        # entries more difficult to wire in if we need to do that in the future.
        return CommitInfo(revision, committer_email, changelog_data)

    def bug_id_for_revision(self, revision):
        return self.commit_info_for_revision(revision).bug_id()

    def _modified_files_matching_predicate(self, git_commit, predicate, changed_files=None):
        # SCM returns paths relative to scm.checkout_root
        # Callers (especially those using the ChangeLog class) may
        # expect absolute paths, so this method returns absolute paths.
        if not changed_files:
            changed_files = self._scm.changed_files(git_commit)
        absolute_paths = [os.path.join(self._scm.checkout_root, path) for path in changed_files]
        return [path for path in absolute_paths if predicate(path)]

    def modified_changelogs(self, git_commit, changed_files=None):
        return self._modified_files_matching_predicate(git_commit, self._is_path_to_changelog, changed_files=changed_files)

    def modified_non_changelogs(self, git_commit, changed_files=None):
        return self._modified_files_matching_predicate(git_commit, lambda path: not self._is_path_to_changelog(path), changed_files=changed_files)

    def commit_message_for_this_commit(self, git_commit, changed_files=None):
        changelog_paths = self.modified_changelogs(git_commit, changed_files)
        if not len(changelog_paths):
            raise ScriptError(message="Found no modified ChangeLogs, cannot create a commit message.\n"
                              "All changes require a ChangeLog.  See:\n"
                              "http://webkit.org/coding/contributing.html")

        changelog_messages = []
        for changelog_path in changelog_paths:
            log("Parsing ChangeLog: %s" % changelog_path)
            changelog_entry = ChangeLog(changelog_path).latest_entry()
            if not changelog_entry:
                raise ScriptError(message="Failed to parse ChangeLog: %s" % os.path.abspath(changelog_path))
            changelog_messages.append(changelog_entry.contents())

        # FIXME: We should sort and label the ChangeLog messages like commit-log-editor does.
        return CommitMessage("".join(changelog_messages).splitlines())

    def recent_commit_infos_for_files(self, paths):
        revisions = set(sum(map(self._scm.revisions_changing_file, paths), []))
        return set(map(self.commit_info_for_revision, revisions))

    def suggested_reviewers(self, git_commit, changed_files=None):
        changed_files = self.modified_non_changelogs(git_commit, changed_files)
        commit_infos = self.recent_commit_infos_for_files(changed_files)
        reviewers = [commit_info.reviewer() for commit_info in commit_infos if commit_info.reviewer()]
        reviewers.extend([commit_info.author() for commit_info in commit_infos if commit_info.author() and commit_info.author().can_review])
        return sorted(set(reviewers))

    def bug_id_for_this_commit(self, git_commit, changed_files=None):
        try:
            return parse_bug_id(self.commit_message_for_this_commit(git_commit, changed_files).message())
        except ScriptError, e:
            pass # We might not have ChangeLogs.

    def apply_patch(self, patch, force=False):
        # It's possible that the patch was not made from the root directory.
        # We should detect and handle that case.
        # FIXME: Move _scm.script_path here once we get rid of all the dependencies.
        args = [self._scm.script_path('svn-apply')]
        if patch.reviewer():
            args += ['--reviewer', patch.reviewer().full_name]
        if force:
            args.append('--force')
        run_command(args, input=patch.contents())

    def apply_reverse_diff(self, revision):
        self._scm.apply_reverse_diff(revision)

        # We revert the ChangeLogs because removing lines from a ChangeLog
        # doesn't make sense.  ChangeLogs are append only.
        changelog_paths = self.modified_changelogs(git_commit=None)
        if len(changelog_paths):
            self._scm.revert_files(changelog_paths)

        conflicts = self._scm.conflicted_files()
        if len(conflicts):
            raise ScriptError(message="Failed to apply reverse diff for revision %s because of the following conflicts:\n%s" % (revision, "\n".join(conflicts)))
