# Copyright (C) 2014-2016 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import os

from webkitpy.common.memoized import memoized
from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.system.executive import ScriptError
from webkitpy.port.apple import ApplePort
from webkitpy.port.leakdetector import LeakDetector


_log = logging.getLogger(__name__)


class DarwinPort(ApplePort):

    SDK = None

    def __init__(self, host, port_name, **kwargs):
        ApplePort.__init__(self, host, port_name, **kwargs)

        self._leak_detector = LeakDetector(self)
        if self.get_option("leaks"):
            # DumpRenderTree slows down noticably if we run more than about 1000 tests in a batch
            # with MallocStackLogging enabled.
            self.set_option_default("batch_size", 1000)

    def default_timeout_ms(self):
        if self.get_option('guard_malloc'):
            return 350 * 1000
        return super(DarwinPort, self).default_timeout_ms()

    def _port_specific_expectations_files(self):
        return list(reversed([self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in self.baseline_search_path()]))

    def check_for_leaks(self, process_name, process_pid):
        if not self.get_option('leaks'):
            return
        # We could use http://code.google.com/p/psutil/ to get the process_name from the pid.
        self._leak_detector.check_for_leaks(process_name, process_pid)

    def print_leaks_summary(self):
        if not self.get_option('leaks'):
            return
        # We're in the manager process, so the leak detector will not have a valid list of leak files.
        # FIXME: This is a hack, but we don't have a better way to get this information from the workers yet.
        # FIXME: This will include too many leaks in subsequent runs until the results directory is cleared!
        leaks_files = self._leak_detector.leaks_files_in_directory(self.results_directory())
        if not leaks_files:
            return
        total_bytes_string, unique_leaks = self._leak_detector.count_total_bytes_and_unique_leaks(leaks_files)
        total_leaks = self._leak_detector.count_total_leaks(leaks_files)
        _log.info("%s total leaks found for a total of %s." % (total_leaks, total_bytes_string))
        _log.info("%s unique leaks found." % unique_leaks)

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')

    def show_results_html_file(self, results_filename):
        # We don't use self._run_script() because we don't want to wait for the script
        # to exit and we want the output to show up on stdout in case there are errors
        # launching the browser.
        self._executive.popen([self.path_to_script('run-safari')] + self._arguments_for_configuration() + ['--no-saved-state', '-NSOpen', results_filename],
            cwd=self.webkit_base(), stdout=file(os.devnull), stderr=file(os.devnull))

    def _merge_crash_logs(self, logs, new_logs, crashed_processes):
        for test, crash_log in new_logs.iteritems():
            try:
                process_name = test.split("-")[0]
                pid = int(test.split("-")[1])
            except IndexError:
                continue
            if not any(entry[1] == process_name and entry[2] == pid for entry in crashed_processes):
                # if this is a new crash, then append the logs
                logs[test] = crash_log
        return logs

    def _look_for_all_crash_logs_in_log_dir(self, newer_than):
        crash_log = CrashLogs(self.host)
        return crash_log.find_all_logs(include_errors=True, newer_than=newer_than)

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, time_fn=None, sleep_fn=None, wait_for_log=True):
        return None

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        """Since crash logs can take a long time to be written out if the system is
           under stress do a second pass at the end of the test run.

           crashes: test_name -> pid, process_name tuple of crashed process
           start_time: time the tests started at.  We're looking for crash
               logs after that time.
        """
        crash_logs = {}
        for (test_name, process_name, pid) in crashed_processes:
            # Passing None for output.  This is a second pass after the test finished so
            # if the output had any logging we would have already collected it.
            crash_log = self._get_crash_log(process_name, pid, None, None, start_time, wait_for_log=False)[1]
            if not crash_log:
                continue
            crash_logs[test_name] = crash_log
        all_crash_log = self._look_for_all_crash_logs_in_log_dir(start_time)
        return self._merge_crash_logs(crash_logs, all_crash_log, crashed_processes)

    def sample_process(self, name, pid):
        exit_status = self._executive.run_command([
            "/usr/bin/sudo",
            "-n",
            "/usr/sbin/spindump",
            pid,
            10,
            10,
            "-file",
            self.spindump_file_path(name, pid),
        ], return_exit_code=True)
        if exit_status:
            try:
                self._executive.run_command([
                    "/usr/bin/sample",
                    pid,
                    10,
                    10,
                    "-file",
                    self.sample_file_path(name, pid),
                ])
            except ScriptError as e:
                _log.warning('Unable to sample process:' + str(e))

    def sample_file_path(self, name, pid):
        return self._filesystem.join(self.results_directory(), "{0}-{1}-sample.txt".format(name, pid))

    def spindump_file_path(self, name, pid):
        return self._filesystem.join(self.results_directory(), "{0}-{1}-spindump.txt".format(name, pid))

    def look_for_new_samples(self, unresponsive_processes, start_time):
        sample_files = {}
        for (test_name, process_name, pid) in unresponsive_processes:
            sample_file = self.sample_file_path(process_name, pid)
            if not self._filesystem.isfile(sample_file):
                continue
            sample_files[test_name] = sample_file
        return sample_files

    def make_command(self):
        return self.xcrun_find('make', '/usr/bin/make')

    def nm_command(self):
        return self.xcrun_find('nm', 'nm')

    def xcrun_find(self, command, fallback=None):
        fallback = fallback or command
        try:
            return self._executive.run_command(['xcrun', '--sdk', self.SDK, '-find', command]).rstrip()
        except ScriptError:
            _log.warn("xcrun failed; falling back to '%s'." % fallback)
            return fallback

    @memoized
    def app_identifier_from_bundle(self, app_bundle):
        plist_path = self._filesystem.join(app_bundle, 'Info.plist')
        if not self._filesystem.exists(plist_path):
            plist_path = self._filesystem.join(app_bundle, 'Contents', 'Info.plist')
        if not self._filesystem.exists(plist_path):
            return None
        return self._executive.run_command(['/usr/libexec/PlistBuddy', '-c', 'Print CFBundleIdentifier', plist_path]).rstrip()
