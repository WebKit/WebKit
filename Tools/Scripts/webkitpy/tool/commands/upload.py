# Copyright (c) 2009, 2010 Google Inc. All rights reserved.
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

import logging
import os
import re
import sys

from optparse import make_option

from webkitpy.tool import steps

from webkitcorepy import string_utils
from webkitscmpy import local

from webkitpy.common.checkout.changelog import parse_bug_id_from_changelog
from webkitpy.common.config.committers import CommitterList
from webkitpy.common.system.user import User
from webkitpy.tool.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.tool.comments import bug_comment_from_svn_revision
from webkitpy.tool.multicommandtool import Command

_log = logging.getLogger(__name__)




class AbstractPatchUploadingCommand(AbstractSequencedCommand):
    def _bug_id(self, options, args, tool, state):
        # Perfer a bug id passed as an argument over a bug url in the diff (i.e. ChangeLogs).
        bug_id = args and args[0]
        if not bug_id:
            changed_files = self._tool.scm().changed_files(options.git_commit)
            state["changed_files"] = changed_files
            bug_id = tool.checkout().bug_id_for_this_commit(options.git_commit, changed_files)
        return bug_id

    def _issues(self, options, args, tool, state):
        issues = getattr(options, 'issues', None)
        if issues is not None:
            return issues

        repository = local.Scm.from_path(self._tool.scm().checkout_root)
        if not repository:
            return None

        commit = repository.find(options.git_commit or 'HEAD')
        if not commit:
            return None
        return commit.issues


    def _prepare_state(self, options, args, tool):
        state = {}
        state["bug_id"] = self._bug_id(options, args, tool, state)
        if not state["bug_id"]:
            _log.error("No bug id passed and no bug url found in ChangeLogs.")
            sys.exit(1)

        state['issues'] = self._issues(options, args, tool, state)
        return state


class Post(AbstractPatchUploadingCommand):
    name = "post"
    help_text = "Attach the current working directory diff to a bug as a patch file"
    argument_names = "[BUGID]"
    steps = [
        steps.CheckForRedactedIssue,
        steps.ValidateChangeLogs,
        steps.CheckStyle,
        steps.ConfirmDiff,
        steps.ObsoletePatches,
        steps.SuggestReviewers,
        steps.EnsureBugIsOpenAndAssigned,
        steps.PostDiff,
        steps.SubmitToEWS,
    ]


class LandSafely(AbstractPatchUploadingCommand):
    name = "land-safely"
    help_text = "Land the current diff via the commit-queue"
    argument_names = "[BUGID]"
    long_help = """land-safely updates the ChangeLog with the reviewer listed
    in bugs.webkit.org for BUGID (or the bug ID detected from the ChangeLog).
    The command then uploads the current diff to the bug and marks it for
    commit by the commit-queue."""
    show_in_main_help = False
    steps = [
        steps.CheckForRedactedIssue,
        steps.UpdateChangeLogsWithReviewer,
        steps.ValidateChangeLogs,
        steps.ObsoletePatches,
        steps.EnsureBugIsOpenAndAssigned,
        steps.PostDiffForCommit,
    ]


class Land(LandSafely):
    name = "land"
    show_in_main_help = True


class Prepare(AbstractSequencedCommand):
    name = "prepare"
    help_text = "Creates a bug (or prompts for an existing bug) and prepares the ChangeLogs"
    argument_names = "[BUGID]"
    steps = [
        steps.PromptForBugOrTitle,
        steps.CreateBug,
        steps.SortXcodeProjectFiles,
        steps.PrepareChangeLog,
    ]

    def _prepare_state(self, options, args, tool):
        bug_id = args and args[0]
        return { "bug_id" : bug_id }


class Upload(AbstractPatchUploadingCommand):
    name = "upload"
    help_text = "Automates the process of uploading a patch for review"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        steps.CheckForRedactedIssue,
        steps.ValidateChangeLogs,
        steps.CheckStyle,
        steps.PromptForBugOrTitle,
        steps.CreateBug,
        steps.SortXcodeProjectFiles,
        steps.PrepareChangeLog,
        steps.EditChangeLog,
        steps.ConfirmDiff,
        steps.ObsoletePatches,
        steps.SuggestReviewers,
        steps.EnsureBugIsOpenAndAssigned,
        steps.PostDiff,
        steps.AddRadar,
        steps.SubmitToEWS,
        steps.WPTChangeExport,
    ]
    long_help = """upload uploads the current diff to bugs.webkit.org.
    If no bug id is provided, upload will create a bug.
    If the current diff does not have a ChangeLog, upload
    will prepare a ChangeLog.  Once a patch is read, upload
    will open the ChangeLogs for editing using the command in the
    EDITOR environment variable and will display the diff using the
    command in the PAGER environment variable."""

    def _prepare_state(self, options, args, tool):
        state = {}
        state["bug_id"] = self._bug_id(options, args, tool, state)
        state['issues'] = self._issues(options, args, tool, state)
        return state


class EditChangeLogs(AbstractSequencedCommand):
    name = "edit-changelogs"
    help_text = "Opens modified ChangeLogs in $EDITOR"
    show_in_main_help = True
    steps = [
        steps.EditChangeLog,
    ]



# FIXME: This command needs to be brought into the modern age with steps and CommitInfo.
class MarkBugFixed(Command):
    name = "mark-bug-fixed"
    help_text = "Mark the specified bug as fixed"
    argument_names = "[SVN_REVISION]"

    def __init__(self):
        options = [
            make_option("--bug-id", action="store", type="string", dest="bug_id", help="Specify bug id if no URL is provided in the commit log."),
            make_option("--comment", action="store", type="string", dest="comment", help="Text to include in bug comment."),
            make_option("--open", action="store_true", default=False, dest="open_bug", help="Open bug in default web browser (Mac only)."),
            make_option("--update-only", action="store_true", default=False, dest="update_only", help="Add comment to the bug, but do not close it."),
        ]
        Command.__init__(self, options=options)

    # FIXME: We should be using checkout().changelog_entries_for_revision(...) instead here.
    def _fetch_commit_log(self, tool, svn_revision):
        if not svn_revision:
            return tool.scm().last_svn_commit_log()
        return tool.scm().svn_commit_log(svn_revision)

    def _determine_bug_id_and_svn_revision(self, tool, bug_id, svn_revision):
        commit_log = self._fetch_commit_log(tool, svn_revision)

        if not bug_id:
            bug_id = parse_bug_id_from_changelog(commit_log)

        if not svn_revision:
            match = re.search(r"^r(?P<svn_revision>\d+) \|", commit_log, re.MULTILINE)
            if match:
                svn_revision = match.group('svn_revision')

        if not bug_id or not svn_revision:
            not_found = []
            if not bug_id:
                not_found.append("bug id")
            if not svn_revision:
                not_found.append("svn revision")
            _log.error("Could not find %s on command-line or in %s."
                  % (" or ".join(not_found), "r%s" % svn_revision if svn_revision else "last commit"))
            sys.exit(1)

        return (bug_id, svn_revision)

    def execute(self, options, args, tool):
        bug_id = options.bug_id

        svn_revision = args and args[0]
        if svn_revision:
            if re.match("^r[0-9]+$", svn_revision, re.IGNORECASE):
                svn_revision = svn_revision[1:]
            if not re.match("^[0-9]+$", svn_revision):
                _log.error("Invalid svn revision: '%s'" % svn_revision)
                sys.exit(1)

        needs_prompt = False
        if not bug_id or not svn_revision:
            needs_prompt = True
            (bug_id, svn_revision) = self._determine_bug_id_and_svn_revision(tool, bug_id, svn_revision)

        _log.info("Bug: <%s> %s" % (tool.bugs.bug_url_for_bug_id(bug_id), tool.bugs.fetch_bug_dictionary(bug_id)["title"]))
        _log.info("Revision: %s" % svn_revision)

        if options.open_bug:
            tool.user.open_url(tool.bugs.bug_url_for_bug_id(bug_id))

        if needs_prompt:
            if not tool.user.confirm("Is this correct?"):
                self._exit(1)

        bug_comment = bug_comment_from_svn_revision(svn_revision)
        if options.comment:
            bug_comment = "%s\n\n%s" % (options.comment, bug_comment)

        if options.update_only:
            _log.info("Adding comment to Bug %s." % bug_id)
            tool.bugs.post_comment_to_bug(bug_id, bug_comment)
        else:
            _log.info("Adding comment to Bug %s and marking as Resolved/Fixed." % bug_id)
            tool.bugs.close_bug_as_fixed(bug_id, bug_comment)
