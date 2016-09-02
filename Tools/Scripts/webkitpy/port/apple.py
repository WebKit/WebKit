# Copyright (C) 2011 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.system.executive import ScriptError
from webkitpy.port.base import Port
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port.leakdetector import LeakDetector


_log = logging.getLogger(__name__)


class ApplePort(Port):
    """Shared logic between all of Apple's ports."""

    # This is used to represent the version of an operating system
    # corresponding to the "mac" or "win" base LayoutTests/platform
    # directory.  I'm not sure this concept is very useful,
    # but it gives us a way to refer to fallback paths *only* including
    # the base directory.
    # This is mostly done because TestConfiguration assumes that self.version()
    # will never return None. (None would be another way to represent this concept.)
    # Apple supposedly has explicit "future" results which are kept in an internal repository.
    # It's possible that Apple would want to fix this code to work better with those results.
    FUTURE_VERSION = 'future'  # FIXME: This whole 'future' thing feels like a hack.

    # overridden in subclasses
    VERSION_FALLBACK_ORDER = []
    ARCHITECTURES = []

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        options = options or {}
        if port_name in (cls.port_name, cls.port_name + '-wk2'):
            # If the port_name matches the (badly named) cls.port_name, that
            # means that they passed 'mac' or 'win' and didn't specify a version.
            # That convention means that we're supposed to use the version currently
            # being run, so this won't work if you're not on mac or win (respectively).
            # If you're not on the o/s in question, you must specify a full version or -future (cf. above).
            if port_name == cls.port_name and not getattr(options, 'webkit_test_runner', False):
                port_name = cls.port_name + '-' + host.platform.os_version
            else:
                port_name = cls.port_name + '-' + host.platform.os_version + '-wk2'
        elif getattr(options, 'webkit_test_runner', False) and  '-wk2' not in port_name:
            port_name += '-wk2'

        return port_name

    def _strip_port_name_prefix(self, port_name):
        # Callers treat this return value as the "version", which only works
        # because Apple ports use a simple name-version port_name scheme.
        # FIXME: This parsing wouldn't be needed if port_name handling was moved to factory.py
        # instead of the individual port constructors.
        return port_name[len(self.port_name + '-'):]

    def __init__(self, host, port_name, **kwargs):
        super(ApplePort, self).__init__(host, port_name, **kwargs)

        allowed_port_names = self.VERSION_FALLBACK_ORDER + [self.operating_system() + "-future"]
        port_name = port_name.replace('-wk2', '')
        self._version = self._strip_port_name_prefix(port_name)

        self._leak_detector = self._make_leak_detector()
        if self.get_option("leaks"):
            # DumpRenderTree slows down noticably if we run more than about 1000 tests in a batch
            # with MallocStackLogging enabled.
            self.set_option_default("batch_size", 1000)

    def _make_leak_detector(self):
        return LeakDetector(self)

    def default_timeout_ms(self):
        if self.get_option('guard_malloc'):
            return 350 * 1000
        return super(ApplePort, self).default_timeout_ms()

    def supports_per_test_timeout(self):
        return True

    def should_retry_crashes(self):
        return True

    def _skipped_file_search_paths(self):
        # We don't have a dedicated Skipped file for the most recent version of the port;
        # we just use the one in platform/{mac,win}
        most_recent_name = self.VERSION_FALLBACK_ORDER[-1]
        return set(filter(lambda name: name != most_recent_name, super(ApplePort, self)._skipped_file_search_paths()))

    # FIXME: A more sophisticated version of this function should move to WebKitPort and replace all calls to name().
    # This is also a misleading name, since 'mac-future' gets remapped to 'mac'.
    def _port_name_with_version(self):
        return self.name().replace('-future', '').replace('-wk2', '')

    def _generate_all_test_configurations(self):
        configurations = []
        allowed_port_names = self.VERSION_FALLBACK_ORDER + [self.operating_system() + "-future"]
        for port_name in allowed_port_names:
            for build_type in self.ALL_BUILD_TYPES:
                for architecture in self.ARCHITECTURES:
                    configurations.append(TestConfiguration(version=self._strip_port_name_prefix(port_name), architecture=architecture, build_type=build_type))
        return configurations

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

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, time_fn, sleep_fn, wait_for_log):
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

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper'
        return self._build_path(binary_name)
