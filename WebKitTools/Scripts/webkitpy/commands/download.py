# Copyright (c) 2009, Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
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

from optparse import make_option

import webkitpy.steps as steps

from webkitpy.bugzilla import parse_bug_id
# We could instead use from modules import buildsteps and then prefix every buildstep with "buildsteps."
from webkitpy.changelogs import ChangeLog
from webkitpy.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.comments import bug_comment_from_commit_text
from webkitpy.executive import ScriptError
from webkitpy.grammar import pluralize
from webkitpy.webkit_logging import error, log
from webkitpy.multicommandtool import AbstractDeclarativeCommand
from webkitpy.stepsequence import StepSequence


class Build(AbstractSequencedCommand):
    name = "build"
    help_text = "Update working copy and build"
    steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.Build,
    ]


class BuildAndTest(AbstractSequencedCommand):
    name = "build-and-test"
    help_text = "Update working copy, build, and run the tests"
    steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.Build,
        steps.RunTests,
    ]


class Land(AbstractSequencedCommand):
    name = "land"
    help_text = "Land the current working directory diff and updates the associated bug if any"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        steps.EnsureBuildersAreGreen,
        steps.UpdateChangeLogsWithReviewer,
        steps.EnsureBuildersAreGreen,
        steps.Build,
        steps.RunTests,
        steps.Commit,
        steps.CloseBugForLandDiff,
    ]
    long_help = """land commits the current working copy diff (just as svn or git commit would).
land will build and run the tests before committing.
If a bug id is provided, or one can be found in the ChangeLog land will update the bug after committing."""

    def _prepare_state(self, options, args, tool):
        return {
            "bug_id" : (args and args[0]) or parse_bug_id(tool.scm().create_patch()),
        }


class AbstractPatchProcessingCommand(AbstractDeclarativeCommand):
    # Subclasses must implement the methods below.  We don't declare them here
    # because we want to be able to implement them with mix-ins.
    #
    # def _fetch_list_of_patches_to_process(self, options, args, tool):
    # def _prepare_to_process(self, options, args, tool):

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
        log("Processing %s from %s." % (pluralize("patch", len(patches)), pluralize("bug", len(bugs_to_patches))))

        for patch in patches:
            self._process_patch(patch, options, args, tool)


class AbstractPatchSequencingCommand(AbstractPatchProcessingCommand):
    prepare_steps = None
    main_steps = None

    def __init__(self):
        options = []
        self._prepare_sequence = StepSequence(self.prepare_steps)
        self._main_sequence = StepSequence(self.main_steps)
        options = sorted(set(self._prepare_sequence.options() + self._main_sequence.options()))
        AbstractPatchProcessingCommand.__init__(self, options)

    def _prepare_to_process(self, options, args, tool):
        self._prepare_sequence.run_and_handle_errors(tool, options)

    def _process_patch(self, patch, options, args, tool):
        state = { "patch" : patch }
        self._main_sequence.run_and_handle_errors(tool, options, state)


class ProcessAttachmentsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class ProcessBugsMixin(object):
    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for bug_id in args:
            patches = tool.bugs.fetch_bug(bug_id).reviewed_patches()
            log("%s found on bug %s." % (pluralize("reviewed patch", len(patches)), bug_id))
            all_patches += patches
        return all_patches


class CheckStyle(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "check-style"
    help_text = "Run check-webkit-style on the specified attachments"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.ApplyPatch,
        steps.CheckStyle,
    ]


class BuildAttachment(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "build-attachment"
    help_text = "Apply and build patches from bugzilla"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.ApplyPatch,
        steps.Build,
    ]


class AbstractPatchApplyingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        steps.EnsureLocalCommitIfNeeded,
        steps.CleanWorkingDirectoryWithLocalCommits,
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
    prepare_steps = [
        steps.EnsureBuildersAreGreen,
    ]
    main_steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.ApplyPatch,
        steps.EnsureBuildersAreGreen,
        steps.Build,
        steps.RunTests,
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


class Rollout(AbstractSequencedCommand):
    name = "rollout"
    show_in_main_help = True
    help_text = "Revert the given revision in the working copy and optionally commit the revert and re-open the original bug"
    argument_names = "REVISION REASON"
    long_help = """Updates the working copy.
Applies the inverse diff for the provided revision.
Creates an appropriate rollout ChangeLog, including a trac link and bug link.
Opens the generated ChangeLogs in $EDITOR.
Shows the prepared diff for confirmation.
Commits the revert and updates the bug (including re-opening the bug if necessary)."""
    steps = [
        steps.CleanWorkingDirectory,
        steps.Update,
        steps.RevertRevision,
        steps.PrepareChangeLogForRevert,
        steps.EditChangeLog,
        steps.ConfirmDiff,
        steps.CompleteRollout,
    ]

    @staticmethod
    def _parse_bug_id_from_revision_diff(tool, revision):
        original_diff = tool.scm().diff_for_revision(revision)
        return parse_bug_id(original_diff)

    def execute(self, options, args, tool):
        revision = args[0]
        reason = args[1]
        bug_id = self._parse_bug_id_from_revision_diff(tool, revision)
        if options.complete_rollout:
            if bug_id:
                log("Will re-open bug %s after rollout." % bug_id)
            else:
                log("Failed to parse bug number from diff.  No bugs will be updated/reopened after the rollout.")

        state = {
            "revision" : revision,
            "bug_id" : bug_id,
            "reason" : reason,
        }
        self._sequence.run_and_handle_errors(tool, options, state)
