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

import traceback
import os

from datetime import datetime
from optparse import make_option
from StringIO import StringIO

from webkitpy.bugzilla import CommitterValidator
from webkitpy.executive import ScriptError
from webkitpy.grammar import pluralize
from webkitpy.webkit_logging import error, log
from webkitpy.multicommandtool import Command
from webkitpy.patchcollection import PersistentPatchCollection, PersistentPatchCollectionDelegate
from webkitpy.statusserver import StatusServer
from webkitpy.stepsequence import StepSequenceErrorHandler
from webkitpy.queueengine import QueueEngine, QueueEngineDelegate

class AbstractQueue(Command, QueueEngineDelegate):
    watchers = [
        "webkit-bot-watchers@googlegroups.com",
    ]

    _pass_status = "Pass"
    _fail_status = "Fail"
    _error_status = "Error"

    def __init__(self, options=None): # Default values should never be collections (like []) as default values are shared between invocations
        options_list = (options or []) + [
            make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Do not ask the user for confirmation before running the queue.  Dangerous!"),
        ]
        Command.__init__(self, "Run the %s" % self.name, options=options_list)

    def _cc_watchers(self, bug_id):
        try:
            self.tool.bugs.add_cc_to_bug(bug_id, self.watchers)
        except Exception, e:
            traceback.print_exc()
            log("Failed to CC watchers.")

    def _update_status(self, message, patch=None, results_file=None):
        self.tool.status_server.update_status(self.name, message, patch, results_file)

    def _did_pass(self, patch):
        self._update_status(self._pass_status, patch)

    def _did_fail(self, patch):
        self._update_status(self._fail_status, patch)

    def _did_error(self, patch, reason):
        message = "%s: %s" % (self._error_status, reason)
        self._update_status(message, patch)

    def queue_log_path(self):
        return "%s.log" % self.name

    def work_item_log_path(self, patch):
        return os.path.join("%s-logs" % self.name, "%s.log" % patch.bug_id())

    def begin_work_queue(self):
        log("CAUTION: %s will discard all local changes in \"%s\"" % (self.name, self.tool.scm().checkout_root))
        if self.options.confirm:
            response = self.tool.user.prompt("Are you sure?  Type \"yes\" to continue: ")
            if (response != "yes"):
                error("User declined.")
        log("Running WebKit %s." % self.name)

    def should_continue_work_queue(self):
        return True

    def next_work_item(self):
        raise NotImplementedError, "subclasses must implement"

    def should_proceed_with_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    # Subclasses are expected to post the message somewhere it can be read by a human.
    # They should also prevent the queue from processing this patch again.
    def handle_unexpected_error(self, work_item, message):
        raise NotImplementedError, "subclasses must implement"

    def run_webkit_patch(self, args):
        webkit_patch_args = [self.tool.path()]
        # FIXME: This is a hack, we should have a more general way to pass global options.
        webkit_patch_args += ["--status-host=%s" % self.tool.status_server.host]
        webkit_patch_args += map(str, args)
        self.tool.executive.run_and_throw_if_fail(webkit_patch_args)

    def log_progress(self, patch_ids):
        log("%s in %s [%s]" % (pluralize("patch", len(patch_ids)), self.name, ", ".join(map(str, patch_ids))))

    def execute(self, options, args, tool, engine=QueueEngine):
        self.options = options
        self.tool = tool
        return engine(self.name, self).run()

    @classmethod
    def _update_status_for_script_error(cls, tool, state, script_error, patch_has_failed_this_queue=True):
        message = script_error.message
        # Error: turns the status bubble purple and means that the error was such that we can't tell if the patch fails or not.
        if not patch_has_failed_this_queue:
            message = "Error: %s" % message
        status_id = tool.status_server.update_status(cls.name, message, state["patch"], StringIO(script_error.output))
        # In the case we know that this patch actually failed the queue, we make a second status entry (mostly because the server doesn't
        # understand messages prefixed with "Fail:" to mean failures, and thus would leave the status bubble yellow instead of red).
        # FIXME: Fail vs. Error should really be stored on a separate field from the message on the server.
        if patch_has_failed_this_queue:
            tool.status_server.update_status(cls.name, cls._fail_status)
        return status_id # Return the status id to the original message with all the script output.

    @classmethod
    def _format_script_error_output_for_bug(tool, status_id, script_error):
        if not script_error.output:
            return script_error.message_with_output()
        results_link = tool.status_server.results_url_for_status(status_id)
        return "%s\nFull output: %s" % (script_error.message_with_output(), results_link)


class CommitQueue(AbstractQueue, StepSequenceErrorHandler):
    name = "commit-queue"
    def __init__(self):
        AbstractQueue.__init__(self)

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.committer_validator = CommitterValidator(self.tool.bugs)

    def _validate_patches_in_commit_queue(self):
        # Not using BugzillaQueries.fetch_patches_from_commit_queue() so we can reject patches with invalid committers/reviewers.
        bug_ids = self.tool.bugs.queries.fetch_bug_ids_from_commit_queue()
        all_patches = sum([self.tool.bugs.fetch_bug(bug_id).commit_queued_patches(include_invalid=True) for bug_id in bug_ids], [])
        return self.committer_validator.patches_after_rejecting_invalid_commiters_and_reviewers(all_patches)

    def next_work_item(self):
        patches = self._validate_patches_in_commit_queue()
        # FIXME: We could sort the patches in a specific order here, was suggested by https://bugs.webkit.org/show_bug.cgi?id=33395
        if not patches:
            self._update_status("Empty queue")
            return None
        # Only bother logging if we have patches in the queue.
        self.log_progress([patch.id() for patch in patches])
        return patches[0]

    def _current_checkout_builds_and_passes_tests(self):
        try:
            self.run_webkit_patch(["build-and-test", "--force-clean", "--non-interactive", "--build-style=both", "--quiet"])
        except ScriptError, e:
            return False
        return True

    def _builders_are_green(self):
        red_builders_names = self.tool.buildbot.red_core_builders_names()
        if red_builders_names:
            red_builders_names = map(lambda name: "\"%s\"" % name, red_builders_names) # Add quotes around the names.
            self._update_status("Builders [%s] are red. See http://build.webkit.org" % ", ".join(red_builders_names), None)
            return False
        return True

    def should_proceed_with_work_item(self, patch):
        if not self._builders_are_green():
            return False
        current_revision = self.tool.scm().checkout_revision()
        self._update_status("Building and testing r%s before landing patch" % current_revision, patch)
        if not self._current_checkout_builds_and_passes_tests():
            self._update_status("Failed to build and test r%s, will try landing later" % current_revision, patch)
            return False
        if not self._builders_are_green():
            return False
        self._update_status("Landing patch", patch)
        return True

    # FIXME: This should be unified with AbstractReviewQueue's implementation.  (Mostly _review_patch just needs a rename.)
    def process_work_item(self, patch):
        # We pass --no-update here because we've already validated
        # that the current revision actually builds and passes the tests.
        # If we update, we risk moving to a revision that doesn't!
        self.run_webkit_patch(["land-attachment", "--force-clean", "--non-interactive", "--no-update", "--parent-command=commit-queue", "--build-style=both", "--quiet", patch.id()])
        self._did_pass(patch)

    def handle_unexpected_error(self, patch, message):
        self._did_fail(patch)
        self._cc_watchers(patch.bug_id())
        self.committer_validator.reject_patch_from_commit_queue(patch.id(), message) # Should instead pass cls.watchers as a CC list here.

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        status_id = cls._update_status_for_script_error(tool, state, script_error, patch_has_failed_this_queue=True)
        script_error_output = cls._format_script_error_output_for_bug(tool, status_id, script_error)
        self._cc_watchers(patch.bug_id())
        CommitterValidator(tool.bugs).reject_patch_from_commit_queue(state["patch"].id(), script_error_output) # Should instead pass cls.watchers as a CC list here.

class AbstractReviewQueue(AbstractQueue, PersistentPatchCollectionDelegate, StepSequenceErrorHandler):
    def __init__(self, options=None):
        AbstractQueue.__init__(self, options)

    def _review_patch(self, patch):
        raise NotImplementedError, "subclasses must implement"

    # PersistentPatchCollectionDelegate methods

    def collection_name(self):
        return self.name

    def fetch_potential_patch_ids(self):
        return self.tool.bugs.queries.fetch_attachment_ids_from_review_queue()

    def status_server(self):
        return self.tool.status_server

    # AbstractQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self._patches = PersistentPatchCollection(self)

    def next_work_item(self):
        patch_id = self._patches.next()
        if patch_id:
            return self.tool.bugs.fetch_attachment(patch_id)
        self._update_status("Empty queue")

    def should_proceed_with_work_item(self, patch):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, patch):
        self._review_patch(patch)
        self._did_pass(patch)

    def handle_unexpected_error(self, patch, message):
        log(message) # Unit tests expect us to log, we could eventually remove this since post_comment_to_bug will log as well.
        self._did_fail(patch)
        self.tool.bugs.post_comment_to_bug(patch.bug_id(), message, cc=self.watchers)

    @classmethod
    def _report_patch_failure(cls, tool, patch, script_error):
        raise NotImplementedError, "subclasses must implement"

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        patch_has_failed = script_error.command_name() != "svn-apply" # svn-apply failures do not necessarily mean the patch would fail to build.
        status_id = cls._update_status_for_script_error(tool, state, script_error, patch_has_failed_this_queue=patch_has_failed)
        if patch_has_failed:
            return # No need to update the bug for svn-apply failures.
        cls._report_patch_failure(tool, patch, script_error)

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        log(script_error.message_with_output())


class StyleQueue(AbstractReviewQueue):
    name = "style-queue"
    def __init__(self):
        AbstractReviewQueue.__init__(self)

    def should_proceed_with_work_item(self, patch):
        self._update_status("Checking style", patch)
        return True

    def _review_patch(self, patch):
        self.run_webkit_patch(["check-style", "--force-clean", "--non-interactive", "--parent-command=style-queue", patch.id()])

    @classmethod
    def _style_error_message_for_bug(cls, patch_id, script_error_output):
        check_style_command = "check-webkit-style"
        message_header = "Attachment %s did not pass %s:" % (patch_id, cls.name)
        message_footer = "If any of these errors are false positives, please file a bug against %s." % check_style_command
        return "%s\n\n%s\n\n%s" % (message_header, script_error_output, message_footer)

    @classmethod
    def _report_patch_failure(cls, tool, patch, script_error):
        script_error_output = self._format_script_error_output_for_bug(tool, status_id, script_error)
        bug_message = cls._style_error_message_for_bug(patch.id(), script_error_output)
        tool.bugs.post_comment_to_bug(patch.bug_id(), message, cc=cls.watchers)
