# Copyright (c) 2009 Google Inc. All rights reserved.
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

from __future__ import with_statement

import codecs
import time
import traceback
import os

from datetime import datetime
from optparse import make_option
from StringIO import StringIO

from webkitpy.common.config.committervalidator import CommitterValidator
from webkitpy.common.net.bugzilla import Attachment
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.net.statusserver import StatusServer
from webkitpy.common.system.deprecated_logging import error, log
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.bot.botinfo import BotInfo
from webkitpy.tool.bot.commitqueuetask import CommitQueueTask, CommitQueueTaskDelegate
from webkitpy.tool.bot.feeders import CommitQueueFeeder, EWSFeeder
from webkitpy.tool.bot.queueengine import QueueEngine, QueueEngineDelegate
from webkitpy.tool.bot.flakytestreporter import FlakyTestReporter
from webkitpy.tool.commands.stepsequence import StepSequenceErrorHandler
from webkitpy.tool.steps.runtests import RunTests
from webkitpy.tool.multicommandtool import Command, TryAgain


class AbstractQueue(Command, QueueEngineDelegate):
    watchers = [
    ]

    _pass_status = "Pass"
    _fail_status = "Fail"
    _retry_status = "Retry"
    _error_status = "Error"

    def __init__(self, options=None): # Default values should never be collections (like []) as default values are shared between invocations
        options_list = (options or []) + [
            make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Do not ask the user for confirmation before running the queue.  Dangerous!"),
            make_option("--exit-after-iteration", action="store", type="int", dest="iterations", default=None, help="Stop running the queue after iterating this number of times."),
        ]
        Command.__init__(self, "Run the %s" % self.name, options=options_list)
        self._iteration_count = 0

    def _cc_watchers(self, bug_id):
        try:
            self._tool.bugs.add_cc_to_bug(bug_id, self.watchers)
        except Exception, e:
            traceback.print_exc()
            log("Failed to CC watchers.")

    def run_webkit_patch(self, args):
        webkit_patch_args = [self._tool.path()]
        # FIXME: This is a hack, we should have a more general way to pass global options.
        # FIXME: We must always pass global options and their value in one argument
        # because our global option code looks for the first argument which does
        # not begin with "-" and assumes that is the command name.
        webkit_patch_args += ["--status-host=%s" % self._tool.status_server.host]
        if self._tool.status_server.bot_id:
            webkit_patch_args += ["--bot-id=%s" % self._tool.status_server.bot_id]
        if self._options.port:
            webkit_patch_args += ["--port=%s" % self._options.port]
        webkit_patch_args.extend(args)
        # FIXME: There is probably no reason to use run_and_throw_if_fail anymore.
        # run_and_throw_if_fail was invented to support tee'd output
        # (where we write both to a log file and to the console at once),
        # but the queues don't need live-progress, a dump-of-output at the
        # end should be sufficient.
        return self._tool.executive.run_and_throw_if_fail(webkit_patch_args)

    def _log_directory(self):
        return os.path.join("..", "%s-logs" % self.name)

    # QueueEngineDelegate methods

    def queue_log_path(self):
        return os.path.join(self._log_directory(), "%s.log" % self.name)

    def work_item_log_path(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def begin_work_queue(self):
        log("CAUTION: %s will discard all local changes in \"%s\"" % (self.name, self._tool.scm().checkout_root))
        if self._options.confirm:
            response = self._tool.user.prompt("Are you sure?  Type \"yes\" to continue: ")
            if (response != "yes"):
                error("User declined.")
        log("Running WebKit %s." % self.name)
        self._tool.status_server.update_status(self.name, "Starting Queue")

    def stop_work_queue(self, reason):
        self._tool.status_server.update_status(self.name, "Stopping Queue, reason: %s" % reason)

    def should_continue_work_queue(self):
        self._iteration_count += 1
        return not self._options.iterations or self._iteration_count <= self._options.iterations

    def next_work_item(self):
        raise NotImplementedError, "subclasses must implement"

    def should_proceed_with_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def process_work_item(self, work_item):
        raise NotImplementedError, "subclasses must implement"

    def handle_unexpected_error(self, work_item, message):
        raise NotImplementedError, "subclasses must implement"

    # Command methods

    def execute(self, options, args, tool, engine=QueueEngine):
        self._options = options # FIXME: This code is wrong.  Command.options is a list, this assumes an Options element!
        self._tool = tool  # FIXME: This code is wrong too!  Command.bind_to_tool handles this!
        return engine(self.name, self, self._tool.wakeup_event).run()

    @classmethod
    def _log_from_script_error_for_upload(cls, script_error, output_limit=None):
        # We have seen request timeouts with app engine due to large
        # log uploads.  Trying only the last 512k.
        if not output_limit:
            output_limit = 512 * 1024  # 512k
        output = script_error.message_with_output(output_limit=output_limit)
        # We pre-encode the string to a byte array before passing it
        # to status_server, because ClientForm (part of mechanize)
        # wants a file-like object with pre-encoded data.
        return StringIO(output.encode("utf-8"))

    @classmethod
    def _update_status_for_script_error(cls, tool, state, script_error, is_error=False):
        message = str(script_error)
        if is_error:
            message = "Error: %s" % message
        failure_log = cls._log_from_script_error_for_upload(script_error)
        return tool.status_server.update_status(cls.name, message, state["patch"], failure_log)


class FeederQueue(AbstractQueue):
    name = "feeder-queue"

    _sleep_duration = 30  # seconds

    # AbstractPatchQueue methods

    def begin_work_queue(self):
        AbstractQueue.begin_work_queue(self)
        self.feeders = [
            CommitQueueFeeder(self._tool),
            EWSFeeder(self._tool),
        ]

    def next_work_item(self):
        # This really show inherit from some more basic class that doesn't
        # understand work items, but the base class in the heirarchy currently
        # understands work items.
        return "synthetic-work-item"

    def should_proceed_with_work_item(self, work_item):
        return True

    def process_work_item(self, work_item):
        for feeder in self.feeders:
            feeder.feed()
        time.sleep(self._sleep_duration)
        return True

    def work_item_log_path(self, work_item):
        return None

    def handle_unexpected_error(self, work_item, message):
        log(message)


class AbstractPatchQueue(AbstractQueue):
    def _update_status(self, message, patch=None, results_file=None):
        return self._tool.status_server.update_status(self.name, message, patch, results_file)

    def _next_patch(self):
        patch_id = self._tool.status_server.next_work_item(self.name)
        if not patch_id:
            return None
        patch = self._tool.bugs.fetch_attachment(patch_id)
        if not patch:
            # FIXME: Using a fake patch because release_work_item has the wrong API.
            # We also don't really need to release the lock (although that's fine),
            # mostly we just need to remove this bogus patch from our queue.
            # If for some reason bugzilla is just down, then it will be re-fed later.
            patch = Attachment({'id': patch_id}, None)
            self._release_work_item(patch)
            return None
        return patch

    def _release_work_item(self, patch):
        self._tool.status_server.release_work_item(self.name, patch)

    def _did_pass(self, patch):
        self._update_status(self._pass_status, patch)
        self._release_work_item(patch)

    def _did_fail(self, patch):
        self._update_status(self._fail_status, patch)
        self._release_work_item(patch)

    def _did_retry(self, patch):
        self._update_status(self._retry_status, patch)
        self._release_work_item(patch)

    def _did_error(self, patch, reason):
        message = "%s: %s" % (self._error_status, reason)
        self._update_status(message, patch)
        self._release_work_item(patch)

    def work_item_log_path(self, patch):
        return os.path.join(self._log_directory(), "%s.log" % patch.bug_id())


class CommitQueue(AbstractPatchQueue, StepSequenceErrorHandler, CommitQueueTaskDelegate):
    name = "commit-queue"

    # AbstractPatchQueue methods

    def begin_work_queue(self):
        AbstractPatchQueue.begin_work_queue(self)
        self.committer_validator = CommitterValidator(self._tool.bugs)

    def next_work_item(self):
        return self._next_patch()

    def should_proceed_with_work_item(self, patch):
        patch_text = "rollout patch" if patch.is_rollout() else "patch"
        self._update_status("Processing %s" % patch_text, patch)
        return True

    # FIXME: This is not really specific to the commit-queue and could be shared.
    def _upload_results_archive_for_patch(self, patch, results_archive_zip):
        bot_id = self._tool.status_server.bot_id or "bot"
        description = "Archive of layout-test-results from %s" % bot_id
        # results_archive is a ZipFile object, grab the File object (.fp) to pass to Mechanize for uploading.
        results_archive_file = results_archive_zip.fp
        # Rewind the file object to start (since Mechanize won't do that automatically)
        # See https://bugs.webkit.org/show_bug.cgi?id=54593
        results_archive_file.seek(0)
        comment_text = "The attached test failures were seen while running run-webkit-tests on the %s.\n" % (self.name)
        # FIXME: We could easily list the test failures from the archive here.
        comment_text += BotInfo(self._tool).summary_text()
        self._tool.bugs.add_attachment_to_bug(patch.bug_id(), results_archive_file, description, filename="layout-test-results.zip", comment_text=comment_text)

    def process_work_item(self, patch):
        self._cc_watchers(patch.bug_id())
        task = CommitQueueTask(self, patch)
        try:
            if task.run():
                self._did_pass(patch)
                return True
            self._did_retry(patch)
        except ScriptError, e:
            validator = CommitterValidator(self._tool.bugs)
            validator.reject_patch_from_commit_queue(patch.id(), self._error_message_for_bug(task.failure_status_id, e))
            results_archive = task.results_archive_from_patch_test_run(patch)
            if results_archive:
                self._upload_results_archive_for_patch(patch, results_archive)
            self._did_fail(patch)

    def _error_message_for_bug(self, status_id, script_error):
        if not script_error.output:
            return script_error.message_with_output()
        results_link = self._tool.status_server.results_url_for_status(status_id)
        return "%s\nFull output: %s" % (script_error.message_with_output(), results_link)

    def handle_unexpected_error(self, patch, message):
        self.committer_validator.reject_patch_from_commit_queue(patch.id(), message)

    # CommitQueueTaskDelegate methods

    def run_command(self, command):
        self.run_webkit_patch(command)

    def command_passed(self, message, patch):
        self._update_status(message, patch=patch)

    def command_failed(self, message, script_error, patch):
        failure_log = self._log_from_script_error_for_upload(script_error)
        return self._update_status(message, patch=patch, results_file=failure_log)

    # FIXME: This exists for mocking, but should instead be mocked via
    # tool.filesystem.read_text_file.  They have different error handling at the moment.
    def _read_file_contents(self, path):
        try:
            return self._tool.filesystem.read_text_file(path)
        except IOError, e:  # File does not exist or can't be read.
            return None

    # FIXME: This logic should move to the port object.
    def _create_layout_test_results(self):
        results_path = self._tool.port().layout_tests_results_path()
        results_html = self._read_file_contents(results_path)
        if not results_html:
            return None
        return LayoutTestResults.results_from_string(results_html)

    def layout_test_results(self):
        results = self._create_layout_test_results()
        # FIXME: We should not have to set failure_limit_count, but we
        # do until run-webkit-tests can be updated save off the value
        # of --exit-after-N-failures in results.html/results.json.
        # https://bugs.webkit.org/show_bug.cgi?id=58481
        if results:
            results.set_failure_limit_count(RunTests.NON_INTERACTIVE_FAILURE_LIMIT_COUNT)
        return results

    def _results_directory(self):
        results_path = self._tool.port().layout_tests_results_path()
        # FIXME: This is wrong in two ways:
        # 1. It assumes that results.html is at the top level of the results tree.
        # 2. This uses the "old" ports.py infrastructure instead of the new layout_tests/port
        # which will not support Chromium.  However the new arch doesn't work with old-run-webkit-tests
        # so we have to use this for now.
        return os.path.dirname(results_path)

    def archive_last_layout_test_results(self, patch):
        results_directory = self._results_directory()
        results_name, _ = os.path.splitext(os.path.basename(results_directory))
        # Note: We name the zip with the bug_id instead of patch_id to match work_item_log_path().
        zip_path = self._tool.workspace.find_unused_filename(self._log_directory(), "%s-%s" % (patch.bug_id(), results_name), "zip")
        if not zip_path:
            return None
        archive = self._tool.workspace.create_zip(zip_path, results_directory)
        # Remove the results directory to prevent http logs, etc. from getting huge between runs.
        # We could have create_zip remove the original, but this is more explicit.
        self._tool.filesystem.rmtree(results_directory)
        return archive

    def refetch_patch(self, patch):
        return self._tool.bugs.fetch_attachment(patch.id())

    def report_flaky_tests(self, patch, flaky_test_results, results_archive=None):
        reporter = FlakyTestReporter(self._tool, self.name)
        reporter.report_flaky_tests(patch, flaky_test_results, results_archive)

    # StepSequenceErrorHandler methods

    def handle_script_error(cls, tool, state, script_error):
        # Hitting this error handler should be pretty rare.  It does occur,
        # however, when a patch no longer applies to top-of-tree in the final
        # land step.
        log(script_error.message_with_output())

    @classmethod
    def handle_checkout_needs_update(cls, tool, state, options, error):
        message = "Tests passed, but commit failed (checkout out of date).  Updating, then landing without building or re-running tests."
        tool.status_server.update_status(cls.name, message, state["patch"])
        # The only time when we find out that out checkout needs update is
        # when we were ready to actually pull the trigger and land the patch.
        # Rather than spinning in the master process, we retry without
        # building or testing, which is much faster.
        options.build = False
        options.test = False
        options.update = True
        raise TryAgain()


class AbstractReviewQueue(AbstractPatchQueue, StepSequenceErrorHandler):
    """This is the base-class for the EWS queues and the style-queue."""
    def __init__(self, options=None):
        AbstractPatchQueue.__init__(self, options)

    def review_patch(self, patch):
        raise NotImplementedError("subclasses must implement")

    # AbstractPatchQueue methods

    def begin_work_queue(self):
        AbstractPatchQueue.begin_work_queue(self)

    def next_work_item(self):
        return self._next_patch()

    def should_proceed_with_work_item(self, patch):
        raise NotImplementedError("subclasses must implement")

    def process_work_item(self, patch):
        try:
            if not self.review_patch(patch):
                return False
            self._did_pass(patch)
            return True
        except ScriptError, e:
            if e.exit_code != QueueEngine.handled_error_code:
                self._did_fail(patch)
            else:
                # The subprocess handled the error, but won't have released the patch, so we do.
                # FIXME: We need to simplify the rules by which _release_work_item is called.
                self._release_work_item(patch)
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

    def review_patch(self, patch):
        self.run_webkit_patch(["check-style", "--force-clean", "--non-interactive", "--parent-command=style-queue", patch.id()])
        return True

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        is_svn_apply = script_error.command_name() == "svn-apply"
        status_id = cls._update_status_for_script_error(tool, state, script_error, is_error=is_svn_apply)
        if is_svn_apply:
            QueueEngine.exit_after_handled_error(script_error)
        message = "Attachment %s did not pass %s:\n\n%s\n\nIf any of these errors are false positives, please file a bug against check-webkit-style." % (state["patch"].id(), cls.name, script_error.message_with_output(output_limit=3*1024))
        tool.bugs.post_comment_to_bug(state["patch"].bug_id(), message, cc=cls.watchers)
        exit(1)
