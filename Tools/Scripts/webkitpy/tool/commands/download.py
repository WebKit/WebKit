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

from webkitpy.tool import steps

from webkitcorepy.string_utils import pluralize

from webkitpy.common.checkout.changelog import ChangeLog
from webkitpy.common.config import urls
from webkitpy.common.net.bugzilla import Bugzilla
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.tool.commands.deprecatedcommand import DeprecatedCommand
from webkitpy.tool.commands.stepsequence import StepSequence
from webkitpy.tool.comments import bug_comment_from_commit_text
from webkitpy.tool.multicommandtool import Command

_log = logging.getLogger(__name__)


class Clean(AbstractSequencedCommand):
    name = "clean"
    help_text = "Clean the working copy"
    steps = [
        steps.DiscardLocalChanges,
    ]

    def _prepare_state(self, options, args, tool):
        options.force_clean = True


class LandUnsafe(AbstractSequencedCommand):
    name = "land-unsafe"
    help_text = "Land the current working directory diff and updates the associated bug if any"
    argument_names = "[BUGID]"
    show_in_main_help = False
    steps = [
        steps.UpdateChangeLogsWithReviewer,
        steps.ValidateReviewer,
        steps.ValidateChangeLogs,  # We do this after UpdateChangeLogsWithReviewer to avoid not having to cache the diff twice.
        steps.Build,
        steps.Commit,
        steps.CloseBugForLandDiff,
    ]
    long_help = """land commits the current working copy diff (just as svn or git commit would).
land will NOT build and run the tests before committing, but you can use the --build option for that.
If a bug id is provided, or one can be found in the ChangeLog land will update the bug after committing."""

    def _prepare_state(self, options, args, tool):
        changed_files = self._tool.scm().changed_files(options.git_commit)
        return {
            "changed_files": changed_files,
            "bug_id": (args and args[0]) or tool.checkout().bug_id_for_this_commit(options.git_commit, changed_files),
        }


@DeprecatedCommand
class LandCowhand(AbstractSequencedCommand):
    # Gender-blind term for cowboy, see: http://en.wiktionary.org/wiki/cowhand
    name = "land-cowhand"
    help_text = "Prepares a ChangeLog and lands the current working directory diff."
    steps = [
        steps.SortXcodeProjectFiles,
        steps.PrepareChangeLog,
        steps.EditChangeLog,
        steps.CheckStyle,
        steps.ConfirmDiff,
        steps.Build,
        steps.Commit,
        steps.CloseBugForLandDiff,
    ]

    def _prepare_state(self, options, args, tool):
        options.check_style_filter = "-changelog"


class CheckStyleLocal(AbstractSequencedCommand):
    name = "check-style-local"
    help_text = "Run check-webkit-style on the current working directory diff"
    steps = [
        steps.CheckStyle,
    ]


class AbstractPatchProcessingCommand(Command):
    # Subclasses must implement the methods below.  We don't declare them here
    # because we want to be able to implement them with mix-ins.
    #
    # pylint: disable=E1101
    # def _fetch_list_of_patches_to_process(self, options, args, tool):
    # def _prepare_to_process(self, options, args, tool):
    # def _process_patch(self, options, args, tool):

    @staticmethod
    def _collect_patches_by_bug(patches):
        bugs_to_patches = {}
        for patch in patches:
            bugs_to_patches[patch.bug_id()] = bugs_to_patches.get(patch.bug_id(), []) + [patch]
        return bugs_to_patches

    def execute(self, options, args, tool):
        self._prepare_to_process(options, args, tool)
        patches = self._fetch_list_of_patches_to_process(options, args, tool)

        # It's nice to print out total statistics.
        bugs_to_patches = self._collect_patches_by_bug(patches)
        _log.info("Processing %s from %s." % (pluralize(len(patches), 'patch', plural='patches'), pluralize(len(bugs_to_patches), "bug")))

        for patch in patches:
            self._process_patch(patch, options, args, tool)


class AbstractPatchSequencingCommand(AbstractPatchProcessingCommand):
    prepare_steps = None
    main_steps = None

    def __init__(self):
        self._prepare_sequence = StepSequence(self.prepare_steps)
        self._main_sequence = StepSequence(self.main_steps)
        options = sorted(set(self._prepare_sequence.options() + self._main_sequence.options()), key=lambda option: option.dest)
        AbstractPatchProcessingCommand.__init__(self, options)

    def _prepare_to_process(self, options, args, tool):
        try:
            self.state = self._prepare_state(options, args, tool)
        except ScriptError as e:
            _log.error(e.message_with_output(output_limit=5000))
            self._exit(e.exit_code or 2)
        self._prepare_sequence.run_and_handle_errors(tool, options, self.state)

    def _process_patch(self, patch, options, args, tool):
        state = {}
        state.update(self.state or {})
        state["patch"] = patch
        self._main_sequence.run_and_handle_errors(tool, options, state)

    def _prepare_state(self, options, args, tool):
        return None


class ProcessAttachmentsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return list(map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args))


class ProcessBugsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for bug_id in args:
            patches = tool.bugs.fetch_bug(bug_id).reviewed_patches()
            _log.info("%s found on bug %s." % (pluralize(len(patches), 'reviewed patch', plural='reviewed patches'), bug_id))
            all_patches += patches
        if not all_patches:
            _log.info("No reviewed patches found, looking for unreviewed patches.")
            for bug_id in args:
                patches = tool.bugs.fetch_bug(bug_id).patches()
                _log.info("%s found on bug %s." % (pluralize(len(patches), 'patch', plural='patches'), bug_id))
                all_patches += patches
        return all_patches


class ProcessURLsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for url in args:
            bug_id = urls.parse_bug_id(url)
            if bug_id:
                patches = tool.bugs.fetch_bug(bug_id).patches()
                _log.info("%s found on bug %s." % (pluralize(len(patches), 'patch', plural='patches'), bug_id))
                all_patches += patches

            attachment_id = urls.parse_attachment_id(url)
            if attachment_id:
                all_patches += tool.bugs.fetch_attachment(attachment_id)

        return all_patches


class CheckStyle(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "check-style"
    help_text = "Run check-webkit-style on the specified attachments"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.CheckStyle,
    ]


class AbstractPatchApplyingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        steps.EnsureLocalCommitIfNeeded,
        steps.CleanWorkingDirectory,
        steps.Update,
    ]
    main_steps = [
        steps.ApplyPatchWithLocalCommit,
    ]
    long_help = """Updates the working copy.
Downloads and applies the patches, creating local commits if necessary."""


class ApplyAttachment(AbstractPatchApplyingCommand, ProcessAttachmentsMixin):
    name = "apply-attachment"
    help_text = "Apply an attachment to the local working directory"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    show_in_main_help = True


class ApplyFromBug(AbstractPatchApplyingCommand, ProcessBugsMixin):
    name = "apply-from-bug"
    help_text = "Apply reviewed patches from provided bugs to the local working directory"
    argument_names = "BUGID [BUGIDS]"
    show_in_main_help = True


class AbstractPatchLandingCommand(AbstractPatchSequencingCommand):
    main_steps = [
        steps.DiscardLocalChanges,
        steps.Update,
        steps.ApplyPatch,
        steps.ValidateChangeLogs,
        steps.ValidateReviewer,
        steps.Build,
        steps.Commit,
        steps.ClosePatch,
        steps.CloseBug,
    ]
    long_help = """Checks to make sure builders are green.
Updates the working copy.
Applies the patch.
Builds.
Runs the layout tests.
Commits the patch.
Clears the flags on the patch.
Closes the bug if no patches are marked for review."""


class LandAttachment(AbstractPatchLandingCommand, ProcessAttachmentsMixin):
    name = "land-attachment"
    help_text = "Land patches from bugzilla, optionally building and testing them first"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    show_in_main_help = True


class LandFromBug(AbstractPatchLandingCommand, ProcessBugsMixin):
    name = "land-from-bug"
    help_text = "Land all patches on the given bugs, optionally building and testing them first"
    argument_names = "BUGID [BUGIDS]"
    show_in_main_help = True


class ValidateChangelog(AbstractSequencedCommand):
    name = "validate-changelog"
    help_text = "Validate that the ChangeLogs and reviewers look reasonable"
    long_help = """Examines the current diff to see whether the ChangeLogs
and the reviewers listed in the ChangeLogs look reasonable.
"""
    steps = [
        steps.ValidateChangeLogs,
        steps.ValidateReviewer,
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
