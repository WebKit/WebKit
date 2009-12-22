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

from modules.bugzilla import parse_bug_id
from modules.buildsteps import PrepareChangelogStep, CommandOptions, ObsoletePatchesOnBugStep, PostDiffToBugStep
from modules.commands.download import AbstractSequencedCommmand
from modules.comments import bug_comment_from_svn_revision
from modules.grammar import pluralize
from modules.logging import error, log
from modules.multicommandtool import Command

# FIXME: Requires unit test.
class CommitMessageForCurrentDiff(Command):
    name = "commit-message"
    def __init__(self):
        Command.__init__(self, "Print a commit message suitable for the uncommitted changes")

    def execute(self, options, args, tool):
        os.chdir(tool.scm().checkout_root)
        print "%s" % tool.scm().commit_message_for_this_commit().message()


class ObsoleteAttachments(AbstractSequencedCommmand):
    name = "obsolete-attachments"
    help_text = "Mark all attachments on a bug as obsolete"
    argument_names = "BUGID"
    steps = [
        ObsoletePatchesOnBugStep,
    ]

    def _prepare_state(self, options, args, tool):
        return { "bug_id" : args[0] }


class PostDiff(AbstractSequencedCommmand):
    name = "post-diff"
    help_text = "Attach the current working directory diff to a bug as a patch file"
    argument_names = "[BUGID]"
    show_in_main_help = True
    steps = [
        ObsoletePatchesOnBugStep,
        PostDiffToBugStep,
    ]

    def _prepare_state(self, options, args, tool):
        # Perfer a bug id passed as an argument over a bug url in the diff (i.e. ChangeLogs).
        bug_id = (args and args[0]) or parse_bug_id(tool.scm().create_patch())
        if not bug_id:
            error("No bug id passed and no bug url found in diff, can't post.")
        return { "bug_id" : bug_id }


class SubmitPatch(AbstractSequencedCommmand):
    name = "submit-patch"
    help_text = "Experimental.  Creates a patch from the current working copy and uploads bugzilla"
    argument_names = "BUGID"
    steps = [
        PrepareChangelogStep,
        # FIXME: Add a CreateBugStep!
        ObsoletePatchesOnBugStep,
        PostDiffToBugStep,
    ]

    def _prepare_state(self, options, args, tool):
        bug_id = args[0]
        return { "bug_id" : bug_id }


class PostCommits(Command):
    name = "post-commits"
    show_in_main_help = True
    def __init__(self):
        options = [
            make_option("-b", "--bug-id", action="store", type="string", dest="bug_id", help="Specify bug id if no URL is provided in the commit log."),
            make_option("--add-log-as-comment", action="store_true", dest="add_log_as_comment", default=False, help="Add commit log message as a comment when uploading the patch."),
            make_option("-m", "--description", action="store", type="string", dest="description", help="Description string for the attachment (default: description from commit message)"),
            CommandOptions.obsolete_patches,
            CommandOptions.review,
            CommandOptions.request_commit,
        ]
        Command.__init__(self, "Attach a range of local commits to bugs as patch files", "COMMITISH", options=options, requires_local_commits=True)

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
            error("bugzilla-tool does not support attaching %s at once.  Are you sure you passed the right commit range?" % (pluralize("patch", len(commit_ids))))

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
                ObsoletePatchesOnBugStep(tool, options).run(state)
                have_obsoleted_patches.add(bug_id)

            diff_file = self._diff_file_for_commit(tool, commit_id)
            description = options.description or commit_message.description(lstrip=True, strip_url=True)
            comment_text = self._comment_text_for_commit(options, commit_message, tool, commit_id)
            tool.bugs.add_patch_to_bug(bug_id, diff_file, description, comment_text, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)


# FIXME: Requires unit test.  Blocking issue: too complex for now.
class MarkBugFixed(Command):
    name = "mark-bug-fixed"
    show_in_main_help = True
    def __init__(self):
        options = [
            make_option("--bug-id", action="store", type="string", dest="bug_id", help="Specify bug id if no URL is provided in the commit log."),
            make_option("--comment", action="store", type="string", dest="comment", help="Text to include in bug comment."),
            make_option("--open", action="store_true", default=False, dest="open_bug", help="Open bug in default web browser (Mac only)."),
            make_option("--update-only", action="store_true", default=False, dest="update_only", help="Add comment to the bug, but do not close it."),
        ]
        Command.__init__(self, "Mark the specified bug as fixed", "[SVN_REVISION]", options=options)

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

    def _open_bug_in_web_browser(self, tool, bug_id):
        if sys.platform == "darwin":
            tool.executive.run_command(["open", tool.bugs.short_bug_url_for_bug_id(bug_id)])
            return
        log("WARNING: --open is only supported on Mac OS X.")

    def _prompt_user_for_correctness(self, bug_id, svn_revision):
        answer = raw_input("Is this correct (y/N)? ")
        if not re.match("^\s*y(es)?", answer, re.IGNORECASE):
            exit(1)

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

        log("Bug: <%s> %s" % (tool.bugs.short_bug_url_for_bug_id(bug_id), tool.bugs.fetch_title_from_bug(bug_id)))
        log("Revision: %s" % svn_revision)

        if options.open_bug:
            self._open_bug_in_web_browser(tool, bug_id)

        if needs_prompt:
            self._prompt_user_for_correctness(bug_id, svn_revision)

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
class CreateBug(Command):
    name = "create-bug"
    show_in_main_help = True
    def __init__(self):
        options = [
            make_option("--cc", action="store", type="string", dest="cc", help="Comma-separated list of email addresses to carbon-copy."),
            make_option("--component", action="store", type="string", dest="component", help="Component for the new bug."),
            make_option("--no-prompt", action="store_false", dest="prompt", default=True, help="Do not prompt for bug title and comment; use commit log instead."),
            make_option("--no-review", action="store_false", dest="review", default=True, help="Do not mark the patch for review."),
            make_option("--request-commit", action="store_true", dest="request_commit", default=False, help="Mark the patch as needing auto-commit after review."),
        ]
        Command.__init__(self, "Create a bug from local changes or local commits", "[COMMITISH]", options=options)

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
        diff_file = StringIO.StringIO(diff) # create_bug_with_patch expects a file-like object
        bug_id = tool.bugs.create_bug_with_patch(bug_title, comment_text, options.component, diff_file, "Patch", cc=options.cc, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)

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
        diff_file = StringIO.StringIO(diff) # create_bug_with_patch expects a file-like object
        bug_id = tool.bugs.create_bug_with_patch(bug_title, comment_text, options.component, diff_file, "Patch", cc=options.cc, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)

    def prompt_for_bug_title_and_comment(self):
        bug_title = raw_input("Bug title: ")
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
