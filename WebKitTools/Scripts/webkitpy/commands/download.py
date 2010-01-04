#!/usr/bin/env python
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

from webkitpy.bugzilla import parse_bug_id
# We could instead use from modules import buildsteps and then prefix every buildstep with "buildsteps."
from webkitpy.buildsteps import *
from webkitpy.changelogs import ChangeLog
from webkitpy.comments import bug_comment_from_commit_text
from webkitpy.executive import ScriptError
from webkitpy.grammar import pluralize
from webkitpy.webkit_logging import error, log
from webkitpy.multicommandtool import AbstractDeclarativeCommmand, Command
from webkitpy.stepsequence import StepSequence


# FIXME: Move this to a more general location.
class AbstractSequencedCommmand(AbstractDeclarativeCommmand):
    steps = None
    def __init__(self):
        self._sequence = StepSequence(self.steps)
        AbstractDeclarativeCommmand.__init__(self, self._sequence.options())

    def _prepare_state(self, options, args, tool):
        return None

    def execute(self, options, args, tool):
        self._sequence.run_and_handle_errors(tool, options, self._prepare_state(options, args, tool))


class Build(AbstractSequencedCommmand):
    name = "build"
    help_text = "Update working copy and build"
    steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        BuildStep,
    ]


class BuildAndTest(AbstractSequencedCommmand):
    name = "build-and-test"
    help_text = "Update working copy, build, and run the tests"
    steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        BuildStep,
        RunTestsStep,
    ]


class LandDiff(AbstractSequencedCommmand):
    name = "land-diff"
    help_text = "Land the current working directory diff and updates the associated bug if any"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        EnsureBuildersAreGreenStep,
        UpdateChangeLogsWithReviewerStep,
        EnsureBuildersAreGreenStep,
        BuildStep,
        RunTestsStep,
        CommitStep,
        CloseBugForLandDiffStep,
    ]

    def _prepare_state(self, options, args, tool):
        return {
            "patch" : {
                "id" : None,
                "bug_id" : (args and args[0]) or parse_bug_id(tool.scm().create_patch()),
            }
        }


class AbstractPatchProcessingCommand(AbstractDeclarativeCommmand):
    # Subclasses must implement the methods below.  We don't declare them here
    # because we want to be able to implement them with mix-ins.
    #
    # def _fetch_list_of_patches_to_process(self, options, args, tool):
    # def _prepare_to_process(self, options, args, tool):

    @staticmethod
    def _collect_patches_by_bug(patches):
        bugs_to_patches = {}
        for patch in patches:
            bug_id = patch["bug_id"]
            bugs_to_patches[bug_id] = bugs_to_patches.get(bug_id, []) + [patch]
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
            patches = tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
            log("%s found on bug %s." % (pluralize("reviewed patch", len(patches)), bug_id))
            all_patches += patches
        return all_patches


class CheckStyle(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "check-style"
    help_text = "Run check-webkit-style on the specified attachments"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        ApplyPatchStep,
        CheckStyleStep,
    ]


class BuildAttachment(AbstractPatchSequencingCommand, ProcessAttachmentsMixin):
    name = "build-attachment"
    help_text = "Apply and build patches from bugzilla"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    main_steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        ApplyPatchStep,
        BuildStep,
    ]


class AbstractPatchApplyingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        EnsureLocalCommitIfNeeded,
        CleanWorkingDirectoryWithLocalCommitsStep,
        UpdateStep,
    ]
    main_steps = [
        ApplyPatchWithLocalCommitStep,
    ]


class ApplyAttachment(AbstractPatchApplyingCommand, ProcessAttachmentsMixin):
    name = "apply-attachment"
    help_text = "Apply an attachment to the local working directory"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    show_in_main_help = True


class ApplyPatches(AbstractPatchApplyingCommand, ProcessBugsMixin):
    name = "apply-patches"
    help_text = "Apply reviewed patches from provided bugs to the local working directory"
    argument_names = "BUGID [BUGIDS]"
    show_in_main_help = True


class AbstractPatchLandingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        EnsureBuildersAreGreenStep,
    ]
    main_steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        ApplyPatchStep,
        EnsureBuildersAreGreenStep,
        BuildStep,
        RunTestsStep,
        CommitStep,
        ClosePatchStep,
        CloseBugStep,
    ]


class LandAttachment(AbstractPatchLandingCommand, ProcessAttachmentsMixin):
    name = "land-attachment"
    help_text = "Land patches from bugzilla, optionally building and testing them first"
    argument_names = "ATTACHMENT_ID [ATTACHMENT_IDS]"
    show_in_main_help = True


class LandPatches(AbstractPatchLandingCommand, ProcessBugsMixin):
    name = "land-patches"
    help_text = "Land all patches on the given bugs, optionally building and testing them first"
    argument_names = "BUGID [BUGIDS]"
    show_in_main_help = True


# FIXME: Make Rollout more declarative.
class Rollout(Command):
    name = "rollout"
    show_in_main_help = True
    def __init__(self):
        self._sequence = StepSequence([
            CleanWorkingDirectoryStep,
            UpdateStep,
            RevertRevisionStep,
            PrepareChangeLogForRevertStep,
            CompleteRollout,
        ])
        Command.__init__(self, "Revert the given revision in the working copy and optionally commit the revert and re-open the original bug", "REVISION [BUGID]", options=self._sequence.options())

    @staticmethod
    def _parse_bug_id_from_revision_diff(tool, revision):
        original_diff = tool.scm().diff_for_revision(revision)
        return parse_bug_id(original_diff)

    @staticmethod
    def _reopen_bug_after_rollout(tool, bug_id, comment_text):
        if bug_id:
            tool.bugs.reopen_bug(bug_id, comment_text)
        else:
            log(comment_text)
            log("No bugs were updated or re-opened to reflect this rollout.")

    def execute(self, options, args, tool):
        revision = args[0]
        bug_id = self._parse_bug_id_from_revision_diff(tool, revision)
        if options.complete_rollout:
            if bug_id:
                log("Will re-open bug %s after rollout." % bug_id)
            else:
                log("Failed to parse bug number from diff.  No bugs will be updated/reopened after the rollout.")

        state = {
            "revision": revision,
            "bug_id": bug_id,
        }
        self._sequence.run_and_handle_errors(tool, options, state)
