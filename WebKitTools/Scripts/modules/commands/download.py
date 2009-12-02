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
from modules.buildsteps import BuildSteps
from modules.changelogs import ChangeLog
from modules.comments import bug_comment_from_commit_text
from modules.grammar import pluralize
from modules.landingsequence import LandingSequence, ConditionalLandingSequence
from modules.logging import error, log
from modules.multicommandtool import Command
from modules.scm import ScriptError


class BuildSequence(ConditionalLandingSequence):
    def __init__(self, options, tool):
        ConditionalLandingSequence.__init__(self, None, options, tool)

    def run(self):
        self.clean()
        self.update()
        self.build()


class Build(Command):
    name = "build"
    show_in_main_help = False
    def __init__(self):
        options = BuildSteps.cleaning_options()
        options += BuildSteps.build_options()
        options += BuildSteps.land_options()
        Command.__init__(self, "Update working copy and build", "", options)

    def execute(self, options, args, tool):
        sequence = BuildSequence(options, tool)
        sequence.run_and_handle_errors()


class ApplyAttachment(Command):
    name = "apply-attachment"
    show_in_main_help = True
    def __init__(self):
        options = WebKitApplyingScripts.apply_options()
        options += BuildSteps.cleaning_options()
        Command.__init__(self, "Apply an attachment to the local working directory", "ATTACHMENT_ID", options=options)

    def execute(self, options, args, tool):
        WebKitApplyingScripts.setup_for_patch_apply(tool, options)
        attachment_id = args[0]
        attachment = tool.bugs.fetch_attachment(attachment_id)
        WebKitApplyingScripts.apply_patches_with_options(tool.scm(), [attachment], options)


class ApplyPatches(Command):
    name = "apply-patches"
    show_in_main_help = True
    def __init__(self):
        options = WebKitApplyingScripts.apply_options()
        options += BuildSteps.cleaning_options()
        Command.__init__(self, "Apply reviewed patches from provided bugs to the local working directory", "BUGID", options=options)

    def execute(self, options, args, tool):
        WebKitApplyingScripts.setup_for_patch_apply(tool, options)
        bug_id = args[0]
        patches = tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
        WebKitApplyingScripts.apply_patches_with_options(tool.scm(), patches, options)


class WebKitApplyingScripts:
    @staticmethod
    def apply_options():
        return [
            make_option("--no-update", action="store_false", dest="update", default=True, help="Don't update the working directory before applying patches"),
            make_option("--local-commit", action="store_true", dest="local_commit", default=False, help="Make a local commit for each applied patch"),
        ]

    @staticmethod
    def setup_for_patch_apply(tool, options):
        tool.steps.clean_working_directory(tool.scm(), options, allow_local_commits=True)
        if options.update:
            tool.scm().update_webkit()

    @staticmethod
    def apply_patches_with_options(scm, patches, options):
        if options.local_commit and not scm.supports_local_commits():
            error("--local-commit passed, but %s does not support local commits" % scm.display_name())

        for patch in patches:
            log("Applying attachment %s from bug %s" % (patch["id"], patch["bug_id"]))
            scm.apply_patch(patch)
            if options.local_commit:
                commit_message = scm.commit_message_for_this_commit()
                scm.commit_locally_with_message(commit_message.message() or patch["name"])


class LandDiffSequence(ConditionalLandingSequence):
    def __init__(self, patch, options, tool):
        ConditionalLandingSequence.__init__(self, patch, options, tool)

    def run(self):
        self.check_builders()
        self.build()
        self.test()
        commit_log = self.commit()
        self.close_bug(commit_log)

    def close_bug(self, commit_log):
        comment_test = bug_comment_from_commit_text(self._tool.scm(), commit_log)
        bug_id = self._patch["bug_id"]
        if bug_id:
            log("Updating bug %s" % bug_id)
            if self._options.close_bug:
                self._tool.bugs.close_bug_as_fixed(bug_id, comment_test)
            else:
                # FIXME: We should a smart way to figure out if the patch is attached
                # to the bug, and if so obsolete it.
                self._tool.bugs.post_comment_to_bug(bug_id, comment_test)
        else:
            log(comment_test)
            log("No bug id provided.")


class LandDiff(Command):
    name = "land-diff"
    show_in_main_help = True
    def __init__(self):
        options = [
            make_option("-r", "--reviewer", action="store", type="string", dest="reviewer", help="Update ChangeLogs to say Reviewed by REVIEWER."),
        ]
        options += BuildSteps.build_options()
        options += BuildSteps.land_options()
        Command.__init__(self, "Land the current working directory diff and updates the associated bug if any", "[BUGID]", options=options)

    def guess_reviewer_from_bug(self, bugs, bug_id):
        patches = bugs.fetch_reviewed_patches_from_bug(bug_id)
        if len(patches) != 1:
            log("%s on bug %s, cannot infer reviewer." % (pluralize("reviewed patch", len(patches)), bug_id))
            return None
        patch = patches[0]
        reviewer = patch["reviewer"]
        log("Guessing \"%s\" as reviewer from attachment %s on bug %s." % (reviewer, patch["id"], bug_id))
        return reviewer

    def update_changelogs_with_reviewer(self, reviewer, bug_id, tool):
        if not reviewer:
            if not bug_id:
                log("No bug id provided and --reviewer= not provided.  Not updating ChangeLogs with reviewer.")
                return
            reviewer = self.guess_reviewer_from_bug(tool.bugs, bug_id)

        if not reviewer:
            log("Failed to guess reviewer from bug %s and --reviewer= not provided.  Not updating ChangeLogs with reviewer." % bug_id)
            return

        for changelog_path in tool.scm().modified_changelogs():
            ChangeLog(changelog_path).set_reviewer(reviewer)

    def execute(self, options, args, tool):
        bug_id = (args and args[0]) or parse_bug_id(tool.scm().create_patch())

        tool.steps.ensure_builders_are_green(tool.buildbot, options)

        os.chdir(tool.scm().checkout_root)
        self.update_changelogs_with_reviewer(options.reviewer, bug_id, tool)

        fake_patch = {
            "id": None,
            "bug_id": bug_id
        }

        sequence = LandDiffSequence(fake_patch, options, tool)
        sequence.run()


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


class CheckStyleSequence(LandingSequence):
    def __init__(self, patch, options, tool):
        LandingSequence.__init__(self, patch, options, tool)

    def run(self):
        self.clean()
        self.update()
        self.apply_patch()
        self.build()

    def build(self):
        # Instead of building, we check style.
        self._tool.steps.check_style()


class CheckStyle(AbstractPatchProcessingCommand):
    name = "check-style"
    show_in_main_help = False
    def __init__(self):
        options = BuildSteps.cleaning_options()
        options += BuildSteps.build_options()
        AbstractPatchProcessingCommand.__init__(self, "Run check-webkit-style on the specified attachments", "ATTACHMENT_ID [ATTACHMENT_IDS]", options)

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)

    def _prepare_to_process(self, options, args, tool):
        pass

    def _process_patch(self, patch, options, args, tool):
        sequence = CheckStyleSequence(patch, options, tool)
        sequence.run_and_handle_errors()


class BuildAttachmentSequence(ConditionalLandingSequence):
    def __init__(self, patch, options, tool):
        LandingSequence.__init__(self, patch, options, tool)

    def run(self):
        self.clean()
        self.update()
        self.apply_patch()
        self.build()


class BuildAttachment(AbstractPatchProcessingCommand):
    name = "build-attachment"
    show_in_main_help = False
    def __init__(self):
        options = BuildSteps.cleaning_options()
        options += BuildSteps.build_options()
        options += BuildSteps.land_options()
        AbstractPatchProcessingCommand.__init__(self, "Apply and build patches from bugzilla", "ATTACHMENT_ID [ATTACHMENT_IDS]", options)

    def _fetch_list_of_patches_to_process(self, options, args, tool):
        return map(lambda patch_id: tool.bugs.fetch_attachment(patch_id), args)

    def _prepare_to_process(self, options, args, tool):
        # Check the tree status first so we can fail early.
        tool.steps.ensure_builders_are_green(tool.buildbot, options)

    def _process_patch(self, patch, options, args, tool):
        sequence = BuildAttachmentSequence(patch, options, tool)
        sequence.run_and_handle_errors()


class AbstractPatchLandingCommand(AbstractPatchProcessingCommand):
    def __init__(self, help_text, args_description):
        options = BuildSteps.cleaning_options()
        options += BuildSteps.build_options()
        options += BuildSteps.land_options()
        AbstractPatchProcessingCommand.__init__(self, help_text, args_description, options)

    def _prepare_to_process(self, options, args, tool):
        # Check the tree status first so we can fail early.
        tool.steps.ensure_builders_are_green(tool.buildbot, options)

    def _process_patch(self, patch, options, args, tool):
        sequence = ConditionalLandingSequence(patch, options, tool)
        sequence.run_and_handle_errors()


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


# FIXME: Requires unit test.
class Rollout(Command):
    name = "rollout"
    show_in_main_help = True
    def __init__(self):
        options = BuildSteps.cleaning_options()
        options += BuildSteps.build_options()
        options += BuildSteps.land_options()
        options.append(make_option("--complete-rollout", action="store_true", dest="complete_rollout", help="Commit the revert and re-open the original bug."))
        Command.__init__(self, "Revert the given revision in the working copy and optionally commit the revert and re-open the original bug", "REVISION [BUGID]", options=options)

    @staticmethod
    def _create_changelogs_for_revert(tool, revision):
        # First, discard the ChangeLog changes from the rollout.
        changelog_paths = tool.scm().modified_changelogs()
        tool.scm().revert_files(changelog_paths)

        # Second, make new ChangeLog entries for this rollout.
        # This could move to prepare-ChangeLog by adding a --revert= option.
        tool.steps.prepare_changelog()
        for changelog_path in changelog_paths:
            ChangeLog(changelog_path).update_for_revert(revision)

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

        tool.steps.clean_working_directory(tool.scm(), options)
        tool.scm().update_webkit()
        tool.scm().apply_reverse_diff(revision)
        self._create_changelogs_for_revert(tool, revision)

        # FIXME: Fully automated rollout is not 100% idiot-proof yet, so for now just log with instructions on how to complete the rollout.
        # Once we trust rollout we will remove this option.
        if not options.complete_rollout:
            log("\nNOTE: Rollout support is experimental.\nPlease verify the rollout diff and use \"bugzilla-tool land-diff %s\" to commit the rollout." % bug_id)
        else:
            # FIXME: This function does not exist!!
            # comment_text = WebKitLandingScripts.build_and_commit(tool.scm(), options)
            raise ScriptError("OOPS! This option is not implemented (yet).")
            self._reopen_bug_after_rollout(tool, bug_id, comment_text)
