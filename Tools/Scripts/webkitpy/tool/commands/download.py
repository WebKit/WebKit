# Copyright (c) 2009, 2011 Google Inc. All rights reserved.
# Copyright (c) 2009, 2017-2018 Apple Inc. All rights reserved.
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

import logging

from webkitpy.common.config import urls
from webkitpy.tool import steps
from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand

_log = logging.getLogger(__name__)


class Clean(AbstractSequencedCommand):
    name = "clean"
    help_text = "Clean the working copy"
    steps = [
        steps.DiscardLocalChanges,
    ]

    def _prepare_state(self, options, args, tool):
        options.force_clean = True


class CheckStyleLocal(AbstractSequencedCommand):
    name = "check-style-local"
    help_text = "Run check-webkit-style on the current working directory diff"
    steps = [
        steps.CheckStyle,
    ]


class AbstractRevertPrepCommand(AbstractSequencedCommand):
    argument_names = "REVISION [REVISIONS] REASON"

    def _commit_info(self, revision):
        commit_info = self._tool.checkout().commit_info_for_revision(revision)
        if commit_info and commit_info.bug_id():
            # Note: Don't print a bug URL here because it will confuse the
            #       SheriffBot because the SheriffBot just greps the output
            #       of create-revert for bug URLs.  It should do better
            #       parsing instead.
            _log.info("Preparing revert for bug %s." % commit_info.bug_id())
        else:
            _log.info("Unable to parse bug number from diff.")
        return commit_info

    def _prepare_state(self, options, args, tool):
        description_list = []
        bug_id_list = []

        revisions = []
        commits = []
        for revision in str(args[0]).split():
            if revision.isdigit():
                revisions.append(int(revision))
            elif revision.startswith('r') and revision[1:].isdigit():
                revisions.append(int(revision[1:]))
            else:
                commits.append(revision)
        revisions.sort()
        commits.sort()
        revision_list = ['r{}'.format(rev) for rev in revisions] + commits

        earliest_revision = revision_list[0]
        state = {
            "revision": earliest_revision,
            "revision_list": revision_list,
            "reason": ' '.join(args[1:]),
            "bug_id": None,
            "bug_id_list": bug_id_list,
            "description_list": description_list,
        }
        for revision in revision_list:
            commit_info = self._commit_info(revision)
            if commit_info:
                # We use the earliest revision for the bug info
                if revision == earliest_revision:
                    state["bug_blocked"] = commit_info.bug_id()
                    cc_list = sorted([party.email
                            for party in commit_info.responsible_parties()
                            if getattr(party, 'email', None)])
                    # FIXME: We should used the list as the canonical representation.
                    state["bug_cc"] = ",".join(cc_list)
                description_list.append(commit_info.bug_description())
                bug_id_list.append(commit_info.bug_id())
            else:
                description_list.append(None)
                bug_id_list.append(None)
        return state


class PrepareRevert(AbstractRevertPrepCommand):
    name = "prepare-revert"
    help_text = "Revert the given revision(s) in the working copy and prepare ChangeLogs with revert reason"
    long_help = """Updates the working copy.
Applies the inverse diff for the provided revision(s).
Creates an appropriate revert ChangeLog, including a trac link and bug link.
"""
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.RevertRevision,
        steps.PrepareChangeLogForRevert,
    ]


class CreateRevert(AbstractRevertPrepCommand):
    name = "create-revert"
    help_text = "Creates a bug to track the broken SVN revision(s) and uploads a revert patch."
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.RevertRevision,
        steps.CreateBug,
        steps.PrepareChangeLogForRevert,
        steps.PostDiffForRevert,
    ]

    def _prepare_state(self, options, args, tool):
        state = AbstractRevertPrepCommand._prepare_state(self, options, args, tool)
        state["bug_title"] = "REGRESSION(%s): %s" % (state["revision"], state["reason"])
        state["bug_description"] = "%s introduced a regression:\n%s" % (urls.view_revision_url(state["revision"]), state["reason"])
        # FIXME: If we had more context here, we could link to other open bugs
        #        that mention the test that regressed.
        if options.parent_command == "sheriff-bot":
            state["bug_description"] += """

This is an automatic bug report generated by webkitbot. If this bug
report was created because of a flaky test, please file a bug for the flaky
test (if we don't already have one on file) and dup this bug against that bug
so that we can track how often these flaky tests fail.
"""
        return state


class Revert(AbstractRevertPrepCommand):
    name = "revert"
    show_in_main_help = True
    help_text = "Revert the given revision(s) in the working copy and optionally commit the revert and re-open the original bug"
    long_help = """Updates the working copy.
Applies the inverse diff for the provided revision.
Creates an appropriate revert ChangeLog, including a trac link and bug link.
Opens the generated ChangeLogs in $EDITOR.
Shows the prepared diff for confirmation.
Commits the revert and updates the bug (including re-opening the bug if necessary)."""
    steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.RevertRevision,
        steps.PrepareChangeLogForRevert,
        steps.EditChangeLog,
        steps.ConfirmDiff,
        steps.Build,
        steps.Commit,
        steps.ReopenBugAfterRevert,
    ]
