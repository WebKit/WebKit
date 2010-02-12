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
import re
import StringIO
import sys

from optparse import make_option

import webkitpy.steps as steps

from webkitpy.bugzilla import parse_bug_id
from webkitpy.commands.abstractsequencedcommand import AbstractSequencedCommand
from webkitpy.comments import bug_comment_from_svn_revision
from webkitpy.committers import CommitterList
from webkitpy.grammar import pluralize, join_with_separators
from webkitpy.webkit_logging import error, log
from webkitpy.mock import Mock
from webkitpy.multicommandtool import AbstractDeclarativeCommand
from webkitpy.user import User

class CommitMessageForCurrentDiff(AbstractDeclarativeCommand):
    name = "commit-message"
    help_text = "Print a commit message suitable for the uncommitted changes"

    def execute(self, options, args, tool):
        os.chdir(tool.scm().checkout_root)
        print "%s" % tool.scm().commit_message_for_this_commit().message()

class CleanPendingCommit(AbstractDeclarativeCommand):
    name = "clean-pending-commit"
    help_text = "Clear r+ on obsolete patches so they do not appear in the pending-commit list."

    # NOTE: This was designed to be generic, but right now we're only processing patches from the pending-commit list, so only r+ matters.
    def _flags_to_clear_on_patch(self, patch):
        if not patch.is_obsolete():
            return None
        what_was_cleared = []
        if patch.review() == "+":
            if patch.reviewer():
                what_was_cleared.append("%s's review+" % patch.reviewer().full_name)
            else:
                what_was_cleared.append("review+")
        return join_with_separators(what_was_cleared)

    def execute(self, options, args, tool):
        committers = CommitterList()
        for bug_id in tool.bugs.queries.fetch_bug_ids_from_pending_commit_list():
            bug = self.tool.bugs.fetch_bug(bug_id)
            patches = bug.patches(include_obsolete=True)
            for patch in patches:
                flags_to_clear = self._flags_to_clear_on_patch(patch)
                if not flags_to_clear:
                    continue
                message = "Cleared %s from obsolete attachment %s so that this bug does not appear in http://webkit.org/pending-commit." % (flags_to_clear, patch.id())
                self.tool.bugs.obsolete_attachment(patch.id(), message)


class AssignToCommitter(AbstractDeclarativeCommand):
    name = "assign-to-committer"
    help_text = "Assign bug to whoever attached the most recent r+'d patch"

    def _patches_have_commiters(self, reviewed_patches):
        for patch in reviewed_patches:
            if not patch.committer():
                return False
        return True

    def _assign_bug_to_last_patch_attacher(self, bug_id):
        committers = CommitterList()
        bug = self.tool.bugs.fetch_bug(bug_id)
        assigned_to_email = bug.assigned_to_email()
        if assigned_to_email != self.tool.bugs.unassigned_email:
            log("Bug %s is already assigned to %s (%s)." % (bug_id, assigned_to_email, committers.committer_by_email(assigned_to_email)))
            return

        reviewed_patches = bug.reviewed_patches()
        if not reviewed_patches:
            log("Bug %s has no non-obsolete patches, ignoring." % bug_id)
            return

        # We only need to do anything with this bug if one of the r+'d patches does not have a valid committer (cq+ set).
        if self._patches_have_commiters(reviewed_patches):
            log("All reviewed patches on bug %s already have commit-queue+, ignoring." % bug_id)
            return

        latest_patch = reviewed_patches[-1]
        attacher_email = latest_patch.attacher_email()
        committer = committers.committer_by_email(attacher_email)
        if not committer:
            log("Attacher %s is not a committer.  Bug %s likely needs commit-queue+." % (attacher_email, bug_id))
            return

        reassign_message = "Attachment %s was posted by a committer and has review+, assigning to %s for commit." % (latest_patch.id(), committer.full_name)
        self.tool.bugs.reassign_bug(bug_id, committer.bugzilla_email(), reassign_message)

    def execute(self, options, args, tool):
        for bug_id in tool.bugs.queries.fetch_bug_ids_from_pending_commit_list():
            self._assign_bug_to_last_patch_attacher(bug_id)


class ObsoleteAttachments(AbstractSequencedCommand):
    name = "obsolete-attachments"
    help_text = "Mark all attachments on a bug as obsolete"
    argument_names = "BUGID"
    steps = [
        steps.ObsoletePatches,
    ]

    def _prepare_state(self, options, args, tool):
        return { "bug_id" : args[0] }


class AbstractPatchUploadingCommand(AbstractSequencedCommand):
    def _bug_id(self, args, tool, state):
        # Perfer a bug id passed as an argument over a bug url in the diff (i.e. ChangeLogs).
        bug_id = args and args[0]
        if not bug_id:
            state["diff"] = tool.scm().create_patch()
            bug_id = parse_bug_id(state["diff"])
        return bug_id

    def _prepare_state(self, options, args, tool):
        state = {}
        state["bug_id"] = self._bug_id(args, tool, state)
        if not state["bug_id"]:
            error("No bug id passed and no bug url found in diff.")
        return state


class Post(AbstractPatchUploadingCommand):
    name = "post"
    help_text = "Attach the current working directory diff to a bug as a patch file"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        steps.CheckStyle,
        steps.ConfirmDiff,
        steps.ObsoletePatches,
        steps.PostDiff,
    ]


class LandSafely(AbstractPatchUploadingCommand):
    name = "land-safely"
    help_text = "Land the current diff via the commit-queue (Experimental)"
    argument_names = "[BUGID]"
    steps = [
        steps.UpdateChangeLogsWithReviewer,
        steps.ObsoletePatches,
        steps.PostDiffForCommit,
    ]


class Prepare(AbstractSequencedCommand):
    name = "prepare"
    help_text = "Creates a bug (or prompts for an existing bug) and prepares the ChangeLogs"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        steps.PromptForBugOrTitle,
        steps.CreateBug,
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
        steps.CheckStyle,
        steps.PromptForBugOrTitle,
        steps.CreateBug,
        steps.PrepareChangeLog,
        steps.EditChangeLog,
        steps.ConfirmDiff,
        steps.ObsoletePatches,
        steps.PostDiff,
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
        state["bug_id"] = self._bug_id(args, tool, state)
        return state


class EditChangeLogs(AbstractSequencedCommand):
    name = "edit-changelogs"
    help_text = "Opens modified ChangeLogs in $EDITOR"
    show_in_main_help = True
    steps = [
        steps.EditChangeLog,
    ]


class PostCommits(AbstractDeclarativeCommand):
    name = "post-commits"
    help_text = "Attach a range of local commits to bugs as patch files"
    argument_names = "COMMITISH"

    def __init__(self):
        options = [
            make_option("-b", "--bug-id", action="store", type="string", dest="bug_id", help="Specify bug id if no URL is provided in the commit log."),
            make_option("--add-log-as-comment", action="store_true", dest="add_log_as_comment", default=False, help="Add commit log message as a comment when uploading the patch."),
            make_option("-m", "--description", action="store", type="string", dest="description", help="Description string for the attachment (default: description from commit message)"),
            steps.Options.obsolete_patches,
            steps.Options.review,
            steps.Options.request_commit,
        ]
        AbstractDeclarativeCommand.__init__(self, options=options, requires_local_commits=True)

    def _comment_text_for_commit(self, options, commit_message, tool, commit_id):
        comment_text = None
        if (options.add_log_as_comment):
            comment_text = commit_message.body(lstrip=True)
            comment_text += "---\n"
            comment_text += tool.scm().files_changed_summary_for_commit(commit_id)
        return comment_text

    def _diff_file_for_commit(self, tool, commit_id):
        diff = tool.scm().create_patch_from_local_commit(commit_id)
        return StringIO.StringIO(diff) # add_patch_to_bug expects a file-like object

    def execute(self, options, args, tool):
        commit_ids = tool.scm().commit_ids_from_commitish_arguments(args)
        if len(commit_ids) > 10: # We could lower this limit, 10 is too many for one bug as-is.
            error("webkit-patch does not support attaching %s at once.  Are you sure you passed the right commit range?" % (pluralize("patch", len(commit_ids))))

        have_obsoleted_patches = set()
        for commit_id in commit_ids:
            commit_message = tool.scm().commit_message_for_local_commit(commit_id)

            # Prefer --bug-id=, then a bug url in the commit message, then a bug url in the entire commit diff (i.e. ChangeLogs).
            bug_id = options.bug_id or parse_bug_id(commit_message.message()) or parse_bug_id(tool.scm().create_patch_from_local_commit(commit_id))
            if not bug_id:
                log("Skipping %s: No bug id found in commit or specified with --bug-id." % commit_id)
                continue

            if options.obsolete_patches and bug_id not in have_obsoleted_patches:
                state = { "bug_id": bug_id }
                steps.ObsoletePatches(tool, options).run(state)
                have_obsoleted_patches.add(bug_id)

            diff_file = self._diff_file_for_commit(tool, commit_id)
            description = options.description or commit_message.description(lstrip=True, strip_url=True)
            comment_text = self._comment_text_for_commit(options, commit_message, tool, commit_id)
            tool.bugs.add_patch_to_bug(bug_id, diff_file, description, comment_text, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)


class MarkBugFixed(AbstractDeclarativeCommand):
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
        AbstractDeclarativeCommand.__init__(self, options=options)

    def _fetch_commit_log(self, tool, svn_revision):
        if not svn_revision:
            return tool.scm().last_svn_commit_log()
        return tool.scm().svn_commit_log(svn_revision)

    def _determine_bug_id_and_svn_revision(self, tool, bug_id, svn_revision):
        commit_log = self._fetch_commit_log(tool, svn_revision)

        if not bug_id:
            bug_id = parse_bug_id(commit_log)

        if not svn_revision:
            match = re.search("^r(?P<svn_revision>\d+) \|", commit_log, re.MULTILINE)
            if match:
                svn_revision = match.group('svn_revision')

        if not bug_id or not svn_revision:
            not_found = []
            if not bug_id:
                not_found.append("bug id")
            if not svn_revision:
                not_found.append("svn revision")
            error("Could not find %s on command-line or in %s."
                  % (" or ".join(not_found), "r%s" % svn_revision if svn_revision else "last commit"))

        return (bug_id, svn_revision)

    def execute(self, options, args, tool):
        bug_id = options.bug_id

        svn_revision = args and args[0]
        if svn_revision:
            if re.match("^r[0-9]+$", svn_revision, re.IGNORECASE):
                svn_revision = svn_revision[1:]
            if not re.match("^[0-9]+$", svn_revision):
                error("Invalid svn revision: '%s'" % svn_revision)

        needs_prompt = False
        if not bug_id or not svn_revision:
            needs_prompt = True
            (bug_id, svn_revision) = self._determine_bug_id_and_svn_revision(tool, bug_id, svn_revision)

        log("Bug: <%s> %s" % (tool.bugs.bug_url_for_bug_id(bug_id), tool.bugs.fetch_bug_dictionary(bug_id)["title"]))
        log("Revision: %s" % svn_revision)

        if options.open_bug:
            tool.user.open_url(tool.bugs.bug_url_for_bug_id(bug_id))

        if needs_prompt:
            if not tool.user.confirm("Is this correct?"):
                exit(1)

        bug_comment = bug_comment_from_svn_revision(svn_revision)
        if options.comment:
            bug_comment = "%s\n\n%s" % (options.comment, bug_comment)

        if options.update_only:
            log("Adding comment to Bug %s." % bug_id)
            tool.bugs.post_comment_to_bug(bug_id, bug_comment)
        else:
            log("Adding comment to Bug %s and marking as Resolved/Fixed." % bug_id)
            tool.bugs.close_bug_as_fixed(bug_id, bug_comment)


# FIXME: Requires unit test.  Blocking issue: too complex for now.
class CreateBug(AbstractDeclarativeCommand):
    name = "create-bug"
    help_text = "Create a bug from local changes or local commits"
    argument_names = "[COMMITISH]"

    def __init__(self):
        options = [
            steps.Options.cc,
            steps.Options.component,
            make_option("--no-prompt", action="store_false", dest="prompt", default=True, help="Do not prompt for bug title and comment; use commit log instead."),
            make_option("--no-review", action="store_false", dest="review", default=True, help="Do not mark the patch for review."),
            make_option("--request-commit", action="store_true", dest="request_commit", default=False, help="Mark the patch as needing auto-commit after review."),
        ]
        AbstractDeclarativeCommand.__init__(self, options=options)

    def create_bug_from_commit(self, options, args, tool):
        commit_ids = tool.scm().commit_ids_from_commitish_arguments(args)
        if len(commit_ids) > 3:
            error("Are you sure you want to create one bug with %s patches?" % len(commit_ids))

        commit_id = commit_ids[0]

        bug_title = ""
        comment_text = ""
        if options.prompt:
            (bug_title, comment_text) = self.prompt_for_bug_title_and_comment()
        else:
            commit_message = tool.scm().commit_message_for_local_commit(commit_id)
            bug_title = commit_message.description(lstrip=True, strip_url=True)
            comment_text = commit_message.body(lstrip=True)
            comment_text += "---\n"
            comment_text += tool.scm().files_changed_summary_for_commit(commit_id)

        diff = tool.scm().create_patch_from_local_commit(commit_id)
        diff_file = StringIO.StringIO(diff) # create_bug expects a file-like object
        bug_id = tool.bugs.create_bug(bug_title, comment_text, options.component, diff_file, "Patch", cc=options.cc, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)

        if bug_id and len(commit_ids) > 1:
            options.bug_id = bug_id
            options.obsolete_patches = False
            # FIXME: We should pass through --no-comment switch as well.
            PostCommits.execute(self, options, commit_ids[1:], tool)

    def create_bug_from_patch(self, options, args, tool):
        bug_title = ""
        comment_text = ""
        if options.prompt:
            (bug_title, comment_text) = self.prompt_for_bug_title_and_comment()
        else:
            commit_message = tool.scm().commit_message_for_this_commit()
            bug_title = commit_message.description(lstrip=True, strip_url=True)
            comment_text = commit_message.body(lstrip=True)

        diff = tool.scm().create_patch()
        diff_file = StringIO.StringIO(diff) # create_bug expects a file-like object
        bug_id = tool.bugs.create_bug(bug_title, comment_text, options.component, diff_file, "Patch", cc=options.cc, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)

    def prompt_for_bug_title_and_comment(self):
        bug_title = User.prompt("Bug title: ")
        print "Bug comment (hit ^D on blank line to end):"
        lines = sys.stdin.readlines()
        try:
            sys.stdin.seek(0, os.SEEK_END)
        except IOError:
            # Cygwin raises an Illegal Seek (errno 29) exception when the above
            # seek() call is made. Ignoring it seems to cause no harm.
            # FIXME: Figure out a way to get avoid the exception in the first
            # place.
            pass
        comment_text = "".join(lines)
        return (bug_title, comment_text)

    def execute(self, options, args, tool):
        if len(args):
            if (not tool.scm().supports_local_commits()):
                error("Extra arguments not supported; patch is taken from working directory.")
            self.create_bug_from_commit(options, args, tool)
        else:
            self.create_bug_from_patch(options, args, tool)
