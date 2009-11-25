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

# FIXME: Trim down this import list once we have unit tests.
import os
import re
import StringIO
import subprocess
import sys
import time

from datetime import datetime, timedelta
from optparse import make_option

from modules.bugzilla import Bugzilla, parse_bug_id
from modules.buildbot import BuildBot
from modules.changelogs import ChangeLog
from modules.comments import bug_comment_from_commit_text
from modules.grammar import pluralize
from modules.landingsequence import LandingSequence, ConditionalLandingSequence
from modules.logging import error, log, tee
from modules.multicommandtool import MultiCommandTool, Command
from modules.patchcollection import PatchCollection
from modules.scm import CommitMessage, detect_scm_system, ScriptError, CheckoutNeedsUpdate
from modules.statusbot import StatusBot
from modules.webkitlandingscripts import WebKitLandingScripts, commit_message_for_this_commit
from modules.webkitport import WebKitPort
from modules.workqueue import WorkQueue, WorkQueueDelegate

class CommitMessageForCurrentDiff(Command):
    name = "commit-message"
    def __init__(self):
        Command.__init__(self, "Print a commit message suitable for the uncommitted changes")

    def execute(self, options, args, tool):
        os.chdir(tool.scm().checkout_root)
        print "%s" % commit_message_for_this_commit(tool.scm()).message()


class ObsoleteAttachments(Command):
    name = "obsolete-attachments"
    def __init__(self):
        Command.__init__(self, "Mark all attachments on a bug as obsolete", "BUGID")

    def execute(self, options, args, tool):
        bug_id = args[0]
        attachments = tool.bugs.fetch_attachments_from_bug(bug_id)
        for attachment in attachments:
            if not attachment["is_obsolete"]:
                tool.bugs.obsolete_attachment(attachment["id"])


class PostDiff(Command):
    name = "post-diff"
    def __init__(self):
        options = [
            make_option("-m", "--description", action="store", type="string", dest="description", help="Description string for the attachment (default: \"patch\")"),
        ]
        options += self.posting_options()
        Command.__init__(self, "Attach the current working directory diff to a bug as a patch file", "[BUGID]", options=options)

    @staticmethod
    def posting_options():
        return [
            make_option("--no-obsolete", action="store_false", dest="obsolete_patches", default=True, help="Do not obsolete old patches before posting this one."),
            make_option("--no-review", action="store_false", dest="review", default=True, help="Do not mark the patch for review."),
            make_option("--request-commit", action="store_true", dest="request_commit", default=False, help="Mark the patch as needing auto-commit after review."),
        ]

    @staticmethod
    def obsolete_patches_on_bug(bug_id, bugs):
        patches = bugs.fetch_patches_from_bug(bug_id)
        if len(patches):
            log("Obsoleting %s on bug %s" % (pluralize("old patch", len(patches)), bug_id))
            for patch in patches:
                bugs.obsolete_attachment(patch["id"])

    def execute(self, options, args, tool):
        # Perfer a bug id passed as an argument over a bug url in the diff (i.e. ChangeLogs).
        bug_id = (args and args[0]) or parse_bug_id(tool.scm().create_patch())
        if not bug_id:
            error("No bug id passed and no bug url found in diff, can't post.")

        if options.obsolete_patches:
            self.obsolete_patches_on_bug(bug_id, tool.bugs)

        diff = tool.scm().create_patch()
        diff_file = StringIO.StringIO(diff) # add_patch_to_bug expects a file-like object

        description = options.description or "Patch"
        tool.bugs.add_patch_to_bug(bug_id, diff_file, description, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)


class PostCommits(Command):
    name = "post-commits"
    def __init__(self):
        options = [
            make_option("-b", "--bug-id", action="store", type="string", dest="bug_id", help="Specify bug id if no URL is provided in the commit log."),
            make_option("--add-log-as-comment", action="store_true", dest="add_log_as_comment", default=False, help="Add commit log message as a comment when uploading the patch."),
            make_option("-m", "--description", action="store", type="string", dest="description", help="Description string for the attachment (default: description from commit message)"),
        ]
        options += PostDiff.posting_options()
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
                PostDiff.obsolete_patches_on_bug(bug_id, tool.bugs)
                have_obsoleted_patches.add(bug_id)

            diff_file = self._diff_file_for_commit(tool, commit_id)
            description = options.description or commit_message.description(lstrip=True, strip_url=True)
            comment_text = self._comment_text_for_commit(options, commit_message, tool, commit_id)
            tool.bugs.add_patch_to_bug(bug_id, diff_file, description, comment_text, mark_for_review=options.review, mark_for_commit_queue=options.request_commit)


class CreateBug(Command):
    name = "create-bug"
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
            commit_message = commit_message_for_this_commit(tool.scm())
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
