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
    def _update_status_for_script_error(cls, tool, state, script_error, is_error=False):
        message = script_error.message
        if is_error:
            message = "Error: %s" % message
        output = script_error.message_with_output(output_limit=5*1024*1024) # 5MB
        return tool.status_server.update_status(cls.name, message, state["patch"], StringIO(output))


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

    def _can_build_and_test(self):
        try:
            self.run_webkit_patch(["build-and-test", "--force-clean", "--non-interactive", "--build-style=both", "--quiet"])
        except ScriptError, e:
            self._update_status("Unabled to successfully build and test", None)
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
        if not self._can_build_and_test():
            return False
        if not self._builders_are_green():
            return False
        self._update_status("Landing patch", patch)
        return True

    def process_work_item(self, patch):
        try:
            self._cc_watchers(patch.bug_id())
            # We pass --no-update here because we've already validated
            # that the current revision actually builds and passes the tests.
            # If we update, we risk moving to a revision that doesn't!
            self.run_webkit_patch(["land-attachment", "--force-clean", "--non-interactive", "--no-update", "--parent-command=commit-queue", "--build-style=both", "--quiet", patch.id()])
            self._did_pass(patch)
        except ScriptError, e:
            self._did_fail(patch)
            raise e

    def handle_unexpected_error(self, patch, message):
        self.committer_validator.reject_patch_from_commit_queue(patch.id(), message)

    # StepSequenceErrorHandler methods

    @staticmethod
    def _error_message_for_bug(tool, status_id, script_error):
        if not script_error.output:
            return script_error.message_with_output()
        results_link = tool.status_server.results_url_for_status(status_id)
        return "%s\nFull output: %s" % (script_error.message_with_output(), results_link)

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        status_id = cls._update_status_for_script_error(tool, state, script_error)
        validator = CommitterValidator(tool.bugs)
        validator.reject_patch_from_commit_queue(state["patch"].id(), cls._error_message_for_bug(tool, status_id, script_error))


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

    def is_terminal_status(self, status):
        return status == "Pass" or status == "Fail" or status.startswith("Error:")

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
        try:
            self._review_patch(patch)
            self._did_pass(patch)
        except ScriptError, e:
            if e.exit_code != QueueEngine.handled_error_code:
                self._did_fail(patch)
            raise e

    def handle_unexpected_error(self, patch, message):
        log(message)

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
    def handle_script_error(cls, tool, state, script_error):
        is_svn_apply = script_error.command_name() == "svn-apply"
        status_id = cls._update_status_for_script_error(tool, state, script_error, is_error=is_svn_apply)
        if is_svn_apply:
            QueueEngine.exit_after_handled_error(script_error)
        message = "Attachment %s did not pass %s:\n\n%s\n\nIf any of these errors are false positives, please file a bug against check-webkit-style." % (state["patch"].id(), cls.name, script_error.message_with_output(output_limit=3*1024))
        tool.bugs.post_comment_to_bug(state["patch"].bug_id(), message, cc=cls.watchers)
        exit(1)
