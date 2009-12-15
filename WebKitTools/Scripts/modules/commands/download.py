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

from modules.bugzilla import parse_bug_id
# FIXME: This list is rediculous.  We need to learn the ways of __all__.
from modules.buildsteps import CommandOptions, EnsureBuildersAreGreenStep, UpdateChangelogsWithReviewerStep, CleanWorkingDirectoryStep, CleanWorkingDirectoryWithLocalCommitsStep, UpdateStep, ApplyPatchStep, ApplyPatchWithLocalCommitStep, BuildStep, CheckStyleStep, RunTestsStep, CommitStep, ClosePatchStep, CloseBugStep, CloseBugForLandDiffStep, PrepareChangelogStep, PrepareChangelogForRevertStep, RevertRevisionStep, CompleteRollout
from modules.changelogs import ChangeLog
from modules.comments import bug_comment_from_commit_text
from modules.executive import ScriptError
from modules.grammar import pluralize
from modules.logging import error, log
from modules.multicommandtool import Command
from modules.stepsequence import StepSequence


class Build(Command):
    name = "build"
    show_in_main_help = False
    def __init__(self):
        self._sequence = StepSequence([
            CleanWorkingDirectoryStep,
            UpdateStep,
            BuildStep
        ])
        Command.__init__(self, "Update working copy and build", "", self._sequence.options())

    def execute(self, options, args, tool):
        self._sequence.run_and_handle_errors(tool, options)


class LandDiff(Command):
    name = "land-diff"
    show_in_main_help = True
    def __init__(self):
        self._sequence = StepSequence([
            EnsureBuildersAreGreenStep,
            UpdateChangelogsWithReviewerStep,
            EnsureBuildersAreGreenStep,
            BuildStep,
            RunTestsStep,
            CommitStep,
            CloseBugForLandDiffStep,
        ])
        Command.__init__(self, "Land the current working directory diff and updates the associated bug if any", "[BUGID]", options=self._sequence.options())

    def execute(self, options, args, tool):
        bug_id = (args and args[0]) or parse_bug_id(tool.scm().create_patch())
        fake_patch = {
            "id": None,
            "bug_id": bug_id,
        }
        state = {"patch": fake_patch}
        self._sequence.run_and_handle_errors(tool, options, state)


class AbstractPatchProcessingCommand(Command):
    def __init__(self, help_text, args_description, options):
        Command.__init__(self, help_text, args_description, options=options)

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        raise NotImplementedError, "subclasses must implement"

    def _prepare_to_process(self, options, args, tool):
        raise NotImplementedError, "subclasses must implement"

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

    @staticmethod
    def _create_step_sequence(steps):
        if not steps:
            return None, []
        step_sequence = StepSequence(steps)
        return step_sequence, step_sequence.options()

    def __init__(self, help_text, args_description):
        options = []
        self._prepare_sequence, prepare_options = self._create_step_sequence(self.prepare_steps)
        self._main_sequence, main_options = self._create_step_sequence(self.main_steps)
        options = sorted(set(prepare_options + main_options))
        AbstractPatchProcessingCommand.__init__(self, help_text, args_description, options)

    def _prepare_to_process(self, options, args, tool):
        if self._prepare_sequence:
            self._prepare_sequence.run_and_handle_errors(tool, options)

    def _process_patch(self, patch, options, args, tool):
        if self._main_sequence:
            state = {"patch": patch}
            self._main_sequence.run_and_handle_errors(tool, options, state)


class CheckStyle(AbstractPatchSequencingCommand):
    name = "check-style"
    show_in_main_help = False
    main_steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        ApplyPatchStep,
        CheckStyleStep,
    ]
    def __init__(self):
        AbstractPatchSequencingCommand.__init__(self, "Run check-webkit-style on the specified attachments", "ATTACHMENT_ID [ATTACHMENT_IDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class BuildAttachment(AbstractPatchSequencingCommand):
    name = "build-attachment"
    show_in_main_help = False
    main_steps = [
        CleanWorkingDirectoryStep,
        UpdateStep,
        ApplyPatchStep,
        BuildStep,
    ]
    def __init__(self):
        AbstractPatchSequencingCommand.__init__(self, "Apply and build patches from bugzilla", "ATTACHMENT_ID [ATTACHMENT_IDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class AbstractPatchApplyingCommand(AbstractPatchSequencingCommand):
    prepare_steps = [
        CleanWorkingDirectoryWithLocalCommitsStep,
        UpdateStep,
    ]
    main_steps = [
        ApplyPatchWithLocalCommitStep,
    ]

    def _prepare_to_process(self, options, args, tool):
        if options.local_commit and not tool.scm().supports_local_commits():
            error("--local-commit passed, but %s does not support local commits" % scm.display_name())
        AbstractPatchSequencingCommand._prepare_to_process(self, options, args, tool)


class ApplyAttachment(AbstractPatchApplyingCommand):
    name = "apply-attachment"
    show_in_main_help = True
    def __init__(self):
        AbstractPatchApplyingCommand.__init__(self, "Apply an attachment to the local working directory", "ATTACHMENT_ID [ATTACHMENT_IDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class ApplyPatches(AbstractPatchApplyingCommand):
    name = "apply-patches"
    show_in_main_help = True
    def __init__(self):
        AbstractPatchApplyingCommand.__init__(self, "Apply reviewed patches from provided bugs to the local working directory", "BUGID [BUGIDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for bug_id in args:
            patches = tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
            log("%s found on bug %s." % (pluralize("reviewed patch", len(patches)), bug_id))
            all_patches += patches
        return all_patches


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


class LandAttachment(AbstractPatchLandingCommand):
    name = "land-attachment"
    show_in_main_help = True
    def __init__(self):
        AbstractPatchLandingCommand.__init__(self, "Land patches from bugzilla, optionally building and testing them first", "ATTACHMENT_ID [ATTACHMENT_IDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)


class LandPatches(AbstractPatchLandingCommand):
    name = "land-patches"
    show_in_main_help = True
    def __init__(self):
        AbstractPatchLandingCommand.__init__(self, "Land all patches on the given bugs, optionally building and testing them first", "BUGID [BUGIDS]")

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        all_patches = []
        for bug_id in args:
            patches = tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
            log("%s found on bug %s." % (pluralize("reviewed patch", len(patches)), bug_id))
            all_patches += patches
        return all_patches


class Rollout(Command):
    name = "rollout"
    show_in_main_help = True
    def __init__(self):
        self._sequence = StepSequence([
            CleanWorkingDirectoryStep,
            UpdateStep,
            RevertRevisionStep,
            PrepareChangelogForRevertStep,
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
