# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2022 Apple Inc. All rights reserved.
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

import re
import sys

from webkitcorepy import StringIO, string_utils
from webkitscmpy import local

from webkitpy.common.config import urls
from webkitpy.common.checkout.changelog import ChangeLog, ChangeLogEntry, parse_bug_id_from_changelog
from webkitpy.common.checkout.commitinfo import CommitInfo
from webkitpy.common.checkout.scm import CommitMessage
from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.checkout.scm import Git

from functools import reduce


# This class represents the WebKit-specific parts of the checkout (like ChangeLogs).
# FIXME: Move a bunch of ChangeLog-specific processing from SCM to this object.
# NOTE: All paths returned from this class should be absolute.
class Checkout(object):
    COMMIT_SUBJECT_RE = re.compile(br'Subject: \[PATCH ?(\d+\/\d+)?] (.+)')
    FILTER_BRANCH_PROGRAM = r'''import re
import sys

lines = [l for l in sys.stdin]
for s in re.split(r' (Need the bug URL \(OOPS!\).)|(\S+:\/\/\S+)', lines[0].rstrip()):
    if s and s != ' ':
        print(s)
for l in lines[1:]:
    sys.stdout.write(l)
'''

    @classmethod
    def filter_patch_content(cls, content, reviewer=None):
        new_content = b''
        for line in string_utils.encode(content).splitlines():
            if b'Reviewed by NOBODY (OOPS!).' == line and reviewer:
                line = b'Reviewed by ' + string_utils.encode(reviewer) + b'.'
            new_content += line + b'\n'
        return new_content

    def __init__(self, scm, executive=None, filesystem=None):
        self._scm = scm
        # FIXME: We shouldn't be grabbing at private members on scm.
        self._executive = executive or self._scm._executive
        self._filesystem = filesystem or self._scm._filesystem

    def scripts_directory(self):
        return self._filesystem.join(self._scm.checkout_root, "Tools", "Scripts")

    def script_path(self, script_name):
        return self._filesystem.join(self.scripts_directory(), script_name)

    def is_path_to_changelog(self, path):
        return self._filesystem.basename(path) == "ChangeLog"

    def _latest_entry_for_changelog_at_revision(self, changelog_path, revision):
        changelog_contents = self._scm.contents_at_revision(changelog_path, revision)
        # contents_at_revision returns a byte array (str()), but we know
        # that ChangeLog files are utf-8.  parse_latest_entry_from_file
        # expects a file-like object which vends unicode(), so we decode here.
        # Old revisions of Sources/WebKit/wx/ChangeLog have some invalid utf8 characters.
        changelog_file = StringIO(changelog_contents.decode("utf-8", "ignore"))
        return ChangeLog.parse_latest_entry_from_file(changelog_file)

    def _changelog_data_for_revision(self, revision):
        repo = local.Scm.from_path(self._scm.checkout_root)
        commit = repo.find(revision)

        return {
            "bug_id": parse_bug_id_from_changelog(commit.message),
            "author_name": commit.author.name,
            "author_email": commit.author.email,
            "author": commit.author,
            "contents": commit.message,
            "reviewer": ChangeLogEntry._parse_reviewer_text(commit.message)[0],
            "bug_description": commit.message.splitlines()[0],
            "changed_files": self._scm.changed_files_for_revision(revision),
        }

    @memoized
    def commit_info_for_revision(self, revision):
        committer_email = self._scm.committer_email_for_revision(revision)
        changelog_data = self._changelog_data_for_revision(revision)
        if not changelog_data:
            return None
        return CommitInfo(revision, committer_email, changelog_data)

    def bug_id_for_revision(self, revision):
        return self.commit_info_for_revision(revision).bug_id()

    def _modified_files_matching_predicate(self, git_commit, predicate, changed_files=None):
        # SCM returns paths relative to scm.checkout_root
        # Callers (especially those using the ChangeLog class) may
        # expect absolute paths, so this method returns absolute paths.
        if not changed_files:
            changed_files = self._scm.changed_files(git_commit)
        return list(filter(predicate, list(map(self._scm.absolute_path, changed_files))))

    def modified_changelogs(self, git_commit, changed_files=None):
        return self._modified_files_matching_predicate(git_commit, self.is_path_to_changelog, changed_files=changed_files)

    def modified_non_changelogs(self, git_commit, changed_files=None):
        return self._modified_files_matching_predicate(git_commit, lambda path: not self.is_path_to_changelog(path), changed_files=changed_files)

    def commit_message_for_this_commit(self, git_commit, changed_files=None, return_stderr=False):
        changelog_paths = self.modified_changelogs(git_commit, changed_files)
        if not len(changelog_paths) and isinstance(self._scm, Git):
            if self._scm.merge_base(None) != self._scm.rev_parse('HEAD'):
                return self._scm.commit_message_for_local_commit(git_commit or 'HEAD')
        if not len(changelog_paths):
            raise ScriptError(message="Found no modified ChangeLogs, cannot create a commit message.\n"
                              "All changes require a ChangeLog.  See:\n %s" % urls.contribution_guidelines)

        message_text = self._scm.run([self.script_path('commit-log-editor'), '--print-log'] + changelog_paths, return_stderr=return_stderr)
        return CommitMessage(message_text.splitlines())

    def recent_commit_infos_for_files(self, paths):
        revisions = set(sum(map(self._scm.revisions_changing_file, paths), []))
        # Remove a None entry from the set. This can happen if a revision does have an associated ChangeLog entry (e.g. r185745).
        return set(map(self.commit_info_for_revision, revisions)) - set([None])

    def suggested_reviewers(self, git_commit, changed_files=None):
        changed_files = self.modified_non_changelogs(git_commit, changed_files)
        commit_infos = sorted(self.recent_commit_infos_for_files(changed_files), key=lambda info: info.revision(), reverse=True)
        reviewers = filter(lambda person: person and person.can_review, sum(map(lambda info: [info.reviewer(), info.author()], commit_infos), []))
        unique_reviewers = reduce(lambda suggestions, reviewer: suggestions + [reviewer if reviewer not in suggestions else None], reviewers, [])
        return list(filter(lambda reviewer: reviewer, unique_reviewers))

    def bug_id_for_this_commit(self, git_commit, changed_files=None):
        try:
            return parse_bug_id_from_changelog(self.commit_message_for_this_commit(git_commit, changed_files).message())
        except ScriptError:
            pass  # We might not have ChangeLogs.

    def apply_patch(self, patch):
        # FIXME: Move _scm.script_path here once we get rid of all the dependencies.
        # --force (continue after errors) is the common case, so we always use it.
        from webkitpy.common.checkout.scm import Git

        encoded_patch = string_utils.encode(patch.contents())
        num_commits = len(self.COMMIT_SUBJECT_RE.findall(encoded_patch))
        self._executive.run_command(
            ['git', 'am', '--keep-non-patch'],
            input=self.filter_patch_content(encoded_patch, reviewer=patch.reviewer().full_name if patch.reviewer() else None),
            cwd=self._scm.checkout_root,
        )
        self._executive.run_command(
            ['git', 'filter-branch', '-f', '--msg-filter', '{} -c "{}"'.format(sys.executable, self.FILTER_BRANCH_PROGRAM), 'HEAD...HEAD~{}'.format(num_commits)],
            cwd=self._scm.checkout_root,
        )

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

    def apply_reverse_diffs(self, revision_list):
        for revision in sorted(revision_list, reverse=True):
            self.apply_reverse_diff(revision)
