# Copyright (c) 2009 Google Inc. All rights reserved.
# Copyright (c) 2009, 2017 Apple Inc. All rights reserved.
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
import sys
import traceback

from optparse import make_option

from webkitpy.common.config.ports import DeprecatedPort
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.bot.botinfo import BotInfo
from webkitpy.tool.bot.queueengine import QueueEngine, QueueEngineDelegate
from webkitpy.tool.commands.stepsequence import StepSequenceErrorHandler
from webkitpy.tool.multicommandtool import Command

_log = logging.getLogger(__name__)


class AbstractQueue(Command, QueueEngineDelegate):
    watchers = [
    ]

    def __init__(self, options=None):  # Default values should never be collections (like []) as default values are shared between invocations
        options_list = (options or []) + [
            make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Do not ask the user for confirmation before running the queue.  Dangerous!"),
            make_option("--exit-after-iteration", action="store", type="int", dest="iterations", default=None, help="Stop running the queue after iterating this number of times."),
        ]
        self.help_text = "Run the %s" % self.name
        Command.__init__(self, options=options_list)
        self._iteration_count = 0
        if not hasattr(self, 'architecture'):
            self.architecture = None

    def _cc_watchers(self, bug_id):
        try:
            self._tool.bugs.add_cc_to_bug(bug_id, self.watchers)
        except Exception as e:
            traceback.print_exc()
            _log.error("Failed to CC watchers.")

    def run_webkit_patch(self, args):
        webkit_patch_args = [self._tool.path()]
        # FIXME: This is a hack, we should have a more general way to pass global options.
        # FIXME: We must always pass global options and their value in one argument
        # because our global option code looks for the first argument which does
        # not begin with "-" and assumes that is the command name.
        if self._options.port:
            webkit_patch_args += ["--port=%s" % self._options.port]
        webkit_patch_args.extend(args)

        try:
            args_for_printing = list(webkit_patch_args)
            args_for_printing[0] = 'webkit-patch'  # Printing our path for each log is redundant.
            _log.info("Running: %s" % self._tool.executive.command_for_printing(args_for_printing))
            command_output = self._tool.executive.run_command(webkit_patch_args, cwd=self._tool.scm().checkout_root)
        except ScriptError as e:
            # Make sure the whole output gets printed if the command failed.
            _log.error(e.message_with_output(output_limit=None))
            raise
        return command_output

    def _log_directory(self):
        return os.path.join("..", "%s-logs" % self.name)

    # QueueEngineDelegate methods

    def queue_log_path(self):
        return os.path.join(self._log_directory(), "%s.log" % self.name)

    def work_item_log_path(self, work_item):
        raise NotImplementedError('subclasses must implement')

    def begin_work_queue(self):
        _log.info("CAUTION: %s will discard all local changes in \"%s\"" % (self.name, self._tool.scm().checkout_root))
        if self._options.confirm:
            response = self._tool.user.prompt("Are you sure?  Type \"yes\" to continue: ")
            if (response != "yes"):
                _log.error("User declined.")
                sys.exit(1)
        _log.info("Running WebKit %s." % self.name)

    def stop_work_queue(self, reason):
        pass

    def should_continue_work_queue(self):
        self._iteration_count += 1
        if not isinstance(self._options.iterations, int):
            return True
        return not self._options.iterations or self._iteration_count <= self._options.iterations

    def next_work_item(self):
        raise NotImplementedError('subclasses must implement')

    def process_work_item(self, work_item):
        raise NotImplementedError('subclasses must implement')

    def handle_unexpected_error(self, work_item, message):
        raise NotImplementedError('subclasses must implement')

    # Command methods

    def execute(self, options, args, tool, engine=QueueEngine):
        self._options = options  # FIXME: This code is wrong.  Command.options is a list, this assumes an Options element!
        self._tool = tool  # FIXME: This code is wrong too!  Command.bind_to_tool handles this!
        return engine(self.name, self, self._tool.wakeup_event, self._options.seconds_to_sleep).run()


class AbstractPatchQueue(AbstractQueue):
    def work_item_log_path(self, patch):
        return os.path.join(self._log_directory(), "%s.log" % patch.bug_id())

    def build_style(self):
        return "release"


# Used to share code between the EWS and commit-queue.
class PatchProcessingQueue(AbstractPatchQueue):
    # Subclasses must override.
    port_name = None

    def __init__(self, options=None):
        self._port = None  # We can't instantiate port here because tool isn't avaialble.
        AbstractPatchQueue.__init__(self, options)

    # FIXME: This is a hack to map between the old port names and the new port names.
    def _new_port_name_from_old(self, port_name, platform):
        # ApplePort.determine_full_port_name asserts if the name doesn't include version.
        if port_name == 'mac':
            return 'mac-' + platform.os_version_name().lower().replace(' ', '')
        if port_name == 'win':
            return 'win-future'
        return port_name

    def _create_port(self):
        self._port = self._tool.port_factory.get(self._new_port_name_from_old(self.port_name, self._tool.platform))
        if self.architecture:
            self._port.set_architecture(self.architecture)
        if self.build_style() == "debug":
            self._port.set_option("configuration", "Debug")
        else:
            self._port.set_option("configuration", "Release")

    def begin_work_queue(self):
        AbstractPatchQueue.begin_work_queue(self)
        if not self.port_name:
            return
        # FIXME: This is only used for self._deprecated_port.flag()
        self._deprecated_port = DeprecatedPort.port(self.port_name)
        # FIXME: This violates abstraction
        self._tool._deprecated_port = self._deprecated_port

        self._create_port()

    # FIXME: Bugzilla member functions should perform this check as they can do it as part of the same
    # network request. This would also avoid bugs where the access of the Bugzilla bug changes between
    # the time-of-check (calling this function) and time-of-use (when we ask Bugzilla to perform the
    # actual operation).
    def _can_access_bug(self, bug_id):
        return bool(self._tool.bugs.fetch_bug(bug_id))

    def _upload_results_archive_for_patch(self, patch, results_archive_zip):
        if not self._port:
            self._create_port()

        description = "Archive of layout-test-results for %s" % (self._port.name())
        # results_archive is a ZipFile object, grab the File object (.fp) to pass to Mechanize for uploading.
        results_archive_file = results_archive_zip.fp
        # Rewind the file object to start (since Mechanize won't do that automatically)
        # See https://bugs.webkit.org/show_bug.cgi?id=54593
        results_archive_file.seek(0)
        # FIXME: This is a small lie to always say run-webkit-tests since Chromium uses new-run-webkit-tests.
        # We could make this code look up the test script name off the port.
        comment_text = "The attached test failures were seen while running run-webkit-tests on the %s.\n" % (self.name)
        # FIXME: We could easily list the test failures from the archive here,
        # currently callers do that separately.
        comment_text += BotInfo(self._tool, self._port.name()).summary_text()
        if self._can_access_bug(patch.bug_id()):
            self._tool.bugs.add_attachment_to_bug(patch.bug_id(), results_archive_file, description, filename="layout-test-results.zip", comment_text=comment_text)


class AbstractReviewQueue(PatchProcessingQueue, StepSequenceErrorHandler):
    """This is the base-class for the EWS queues and the style-queue."""
    def __init__(self, options=None):
        PatchProcessingQueue.__init__(self, options)

    def review_patch(self, patch):
        raise NotImplementedError("subclasses must implement")

    # AbstractPatchQueue methods

    def begin_work_queue(self):
        PatchProcessingQueue.begin_work_queue(self)

    def next_work_item(self):
        return None

    def process_work_item(self, patch):
        passed = self.review_patch(patch)
        return passed

    def handle_unexpected_error(self, patch, message):
        _log.error(message)

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        _log.error(script_error.output)
