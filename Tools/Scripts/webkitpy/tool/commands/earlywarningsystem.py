# Copyright (c) 2009 Google Inc. All rights reserved.
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

from webkitpy.common.config.committers import CommitterList
from webkitpy.common.config.ports import WebKitPort
from webkitpy.common.system.deprecated_logging import error, log
from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.bot.expectedfailures import ExpectedFailures
from webkitpy.tool.bot.layouttestresultsreader import LayoutTestResultsReader
from webkitpy.tool.bot.queueengine import QueueEngine
from webkitpy.tool.bot.earlywarningsystemtask import EarlyWarningSystemTask, EarlyWarningSystemTaskDelegate, UnableToApplyPatch
from webkitpy.tool.commands.queues import AbstractReviewQueue


class AbstractEarlyWarningSystem(AbstractReviewQueue):
    _build_style = "release"

    def __init__(self):
        AbstractReviewQueue.__init__(self)
        self.port = WebKitPort.port(self.port_name)

    def should_proceed_with_work_item(self, patch):
        return True

    def _can_build(self):
        try:
            self.run_webkit_patch([
                "build",
                self.port.flag(),
                "--build-style=%s" % self._build_style,
                "--force-clean",
                "--no-update"])
            return True
        except ScriptError, e:
            failure_log = self._log_from_script_error_for_upload(e)
            self._update_status("Unable to perform a build", results_file=failure_log)
            return False

    def _build(self, patch, first_run=False):
        try:
            args = [
                "build-attachment",
                self.port.flag(),
                "--build",
                "--build-style=%s" % self._build_style,
                "--force-clean",
                "--quiet",
                "--non-interactive",
                patch.id()]
            if not first_run:
                # See commit-queue for an explanation of what we're doing here.
                args.append("--no-update")
                args.append("--parent-command=%s" % self.name)
            self.run_webkit_patch(args)
            return True
        except ScriptError, e:
            if first_run:
                return False
            raise

    def review_patch(self, patch):
        if patch.is_obsolete():
            self._did_error(patch, "%s does not process obsolete patches." % self.name)
            return False

        if patch.bug().is_closed():
            self._did_error(patch, "%s does not process patches on closed bugs." % self.name)
            return False

        if not self._build(patch, first_run=True):
            if not self._can_build():
                return False
            self._build(patch)
        return True

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        is_svn_apply = script_error.command_name() == "svn-apply"
        status_id = cls._update_status_for_script_error(tool, state, script_error, is_error=is_svn_apply)
        if is_svn_apply:
            QueueEngine.exit_after_handled_error(script_error)
        results_link = tool.status_server.results_url_for_status(status_id)
        message = "Attachment %s did not build on %s:\nBuild output: %s" % (state["patch"].id(), cls.port_name, results_link)
        tool.bugs.post_comment_to_bug(state["patch"].bug_id(), message, cc=cls.watchers)
        exit(1)

# FIXME: This should merge with AbstractEarlyWarningSystem once all the EWS
# bots run tests.
class AbstractTestingEWS(AbstractEarlyWarningSystem, EarlyWarningSystemTaskDelegate):
    def begin_work_queue(self):
        # FIXME: This violates abstraction
        self._tool._port = self.port
        AbstractEarlyWarningSystem.begin_work_queue(self)
        self._expected_failures = ExpectedFailures()
        self._layout_test_results_reader = LayoutTestResultsReader(self._tool, self._log_directory())

    def review_patch(self, patch):
        task = EarlyWarningSystemTask(self, patch)
        if not task.validate():
            self._did_error(patch, "%s did not process patch." % self.name)
            return False
        try:
            return task.run()
        except UnableToApplyPatch, e:
            self._did_error(patch, "%s unable to apply patch." % self.name)
            return False
        except ScriptError, e:
            # FIXME: We should upload the results archive on failure.
            results_link = self._tool.status_server.results_url_for_status(task.failure_status_id)
            message = "Attachment %s did not pass %s:\nOutput: %s" % (patch.id(), self.name, results_link)
            results = task.results_from_patch_test_run(patch)
            unexpected_failures = self._expected_failures.unexpected_failures(results)
            if unexpected_failures:
                message += "\nNew failing tests:\n%s" % "\n".join(unexpected_failures)
            self._tool.bugs.post_comment_to_bug(patch.bug_id(), message, cc=self.watchers)
            raise

    # EarlyWarningSystemDelegate methods

    def parent_command(self):
        return self.name

    def run_command(self, command):
        self.run_webkit_patch(command + [self.port.flag()])

    def command_passed(self, message, patch):
        pass

    def command_failed(self, message, script_error, patch):
        failure_log = self._log_from_script_error_for_upload(script_error)
        return self._update_status(message, patch=patch, results_file=failure_log)

    def expected_failures(self):
        return self._expected_failures

    def layout_test_results(self):
        return self._layout_test_results_reader.results()

    def archive_last_layout_test_results(self, patch):
        return self._layout_test_results_reader.archive(patch)

    def refetch_patch(self, patch):
        return self._tool.bugs.fetch_attachment(patch.id())

    def report_flaky_tests(self, patch, flaky_test_results, results_archive):
        pass

    # StepSequenceErrorHandler methods

    @classmethod
    def handle_script_error(cls, tool, state, script_error):
        log(script_error.message_with_output())


class GtkEWS(AbstractEarlyWarningSystem):
    name = "gtk-ews"
    port_name = "gtk"
    watchers = AbstractEarlyWarningSystem.watchers + [
        "gns@gnome.org",
        "xan.lopez@gmail.com",
    ]


class EflEWS(AbstractEarlyWarningSystem):
    name = "efl-ews"
    port_name = "efl"
    watchers = AbstractEarlyWarningSystem.watchers + [
        "leandro@profusion.mobi",
        "antognolli@profusion.mobi",
        "lucas.demarchi@profusion.mobi",
        "gyuyoung.kim@samsung.com",
    ]


class QtEWS(AbstractEarlyWarningSystem):
    name = "qt-ews"
    port_name = "qt"


class WinEWS(AbstractEarlyWarningSystem):
    name = "win-ews"
    port_name = "win"
    # Use debug, the Apple Win port fails to link Release on 32-bit Windows.
    # https://bugs.webkit.org/show_bug.cgi?id=39197
    _build_style = "debug"


class AbstractChromiumEWS(AbstractEarlyWarningSystem):
    port_name = "chromium"
    watchers = AbstractEarlyWarningSystem.watchers + [
        "dglazkov@chromium.org",
    ]


class ChromiumLinuxEWS(AbstractTestingEWS):
    # FIXME: We should rename this command to cr-linux-ews, but that requires
    #        a database migration. :(
    name = "chromium-ews"
    port_name = "chromium-xvfb"

    # FIXME: ChromiumLinuxEWS should inherit from AbstractChromiumEWS once
    # all the Chromium EWS bots run tests
    watchers = AbstractChromiumEWS.watchers


class ChromiumWindowsEWS(AbstractChromiumEWS):
    name = "cr-win-ews"


# For platforms that we can't run inside a VM (like Mac OS X), we require
# patches to be uploaded by committers, who are generally trustworthy folk. :)
class AbstractCommitterOnlyEWS(AbstractEarlyWarningSystem):
    def process_work_item(self, patch):
        if not patch.attacher() or not patch.attacher().can_commit:
            self._did_error(patch, "%s cannot process patches from non-committers :(" % self.name)
            return False
        return AbstractEarlyWarningSystem.process_work_item(self, patch)


# FIXME: Inheriting from AbstractCommitterOnlyEWS is kinda a hack, but it
# happens to work because AbstractChromiumEWS and AbstractCommitterOnlyEWS
# provide disjoint sets of functionality, and Python is otherwise smart
# enough to handle the diamond inheritance.
class ChromiumMacEWS(AbstractChromiumEWS, AbstractCommitterOnlyEWS):
    name = "cr-mac-ews"


class MacEWS(AbstractCommitterOnlyEWS):
    name = "mac-ews"
    port_name = "mac"
