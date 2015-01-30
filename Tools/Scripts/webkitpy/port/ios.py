# Copyright (C) 2014 Apple Inc. All rights reserved.
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
import shutil
import time
import re
import itertools
import subprocess

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.system.executive import ScriptError
from webkitpy.port import driver, image_diff
from webkitpy.port.base import Port
from webkitpy.port.leakdetector import LeakDetector
from webkitpy.port import config as port_config
from webkitpy.xcode import simulator


_log = logging.getLogger(__name__)


class IOSSimulatorPort(Port):
    port_name = "ios-simulator"

    FUTURE_VERSION = 'future'

    ARCHITECTURES = ['x86_64', 'x86']

    relay_name = 'LayoutTestRelay'

    def __init__(self, *args, **kwargs):
        super(IOSSimulatorPort, self).__init__(*args, **kwargs)

        self._architecture = self.get_option('architecture')

        if not self._architecture:
            self._architecture = 'x86_64'

        self._leak_detector = LeakDetector(self)
        if self.get_option("leaks"):
            # DumpRenderTree slows down noticably if we run more than about 1000 tests in a batch
            # with MallocStackLogging enabled.
            self.set_option_default("batch_size", 1000)
        mac_config = port_config.Config(self._executive, self._filesystem, 'mac')
        self._mac_build_directory = mac_config.build_directory(self.get_option('configuration'))

        self._testing_device = None

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        if self.get_option('webkit_test_runner'):
            return 'WebKitTestRunnerApp.app'
        return 'DumpRenderTree.app'

    @property
    def relay_path(self):
        return self._filesystem.join(self._mac_build_directory, self.relay_name)

    def default_timeout_ms(self):
        if self.get_option('guard_malloc'):
            return 350 * 1000
        return super(IOSSimulatorPort, self).default_timeout_ms()

    def supports_per_test_timeout(self):
        return True

    def _check_relay(self):
        if not self._filesystem.exists(self.relay_path):
            _log.error("%s was not found at %s" % (self.relay_name, self.relay_path))
            return False
        return True

    def _check_build_relay(self):
        if self.get_option('build') and not self._build_relay():
            return False
        if not self._check_relay():
            return False
        return True

    def check_build(self, needs_http):
        needs_driver = super(IOSSimulatorPort, self).check_build(needs_http)
        return needs_driver and self._check_build_relay()

    def _build_relay(self):
        environment = self.host.copy_current_environment()
        environment.disable_gcc_smartquotes()
        env = environment.to_dictionary()

        try:
            self._run_script("build-layouttestrelay", env=env)
        except ScriptError, e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _build_driver(self):
        built_tool = super(IOSSimulatorPort, self)._build_driver()
        built_relay = self._build_relay()
        return built_tool and built_relay

    def _build_driver_flags(self):
        archs = ['ARCHS=i386'] if self.architecture() == 'x86' else []
        sdk = ['--sdk', 'iphonesimulator']
        return archs + sdk

    def should_retry_crashes(self):
        return True

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            for architecture in self.ARCHITECTURES:
                configurations.append(TestConfiguration(version=self._version, architecture=architecture, build_type=build_type))
        return configurations

    def _driver_class(self):
        return driver.IOSSimulatorDriver

    def default_baseline_search_path(self):
        if self.get_option('webkit_test_runner'):
            fallback_names = [self.port_name + '-wk2'] + [self.port_name]
        else:
            fallback_names = [self.port_name + '-wk1'] + [self.port_name]

        return map(self._webkit_baseline_path, fallback_names)

    def _port_specific_expectations_files(self):
        return list(reversed([self._filesystem.join(self._webkit_baseline_path(p), 'TestExpectations') for p in self.baseline_search_path()]))

    def setup_test_run(self):
        self._executive.run_command(['osascript', '-e', 'tell application "iOS Simulator" to quit'])
        time.sleep(2)
        self._executive.run_command([
            'open', '-a', os.path.join(self.developer_dir, 'Applications', 'iOS Simulator.app'),
            '--args', '-CurrentDeviceUDID', self.testing_device.udid])

    def clean_up_test_run(self):
        super(IOSSimulatorPort, self).clean_up_test_run()
        fifos = [path for path in os.listdir('/tmp') if re.search('org.webkit.(DumpRenderTree|WebKitTestRunner).*_(IN|OUT|ERROR)', path)]
        for fifo in fifos:
            try:
                os.remove(os.path.join('/tmp', fifo))
            except OSError:
                _log.warning('Unable to remove ' + fifo)
                pass

    def setup_environ_for_server(self, server_name=None):
        env = super(IOSSimulatorPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
            if self.get_option('guard_malloc'):
                env['DYLD_INSERT_LIBRARIES'] = '/usr/lib/libgmalloc.dylib:' + self._build_path("libWebCoreTestShim.dylib")
            else:
                env['DYLD_INSERT_LIBRARIES'] = self._build_path("libWebCoreTestShim.dylib")
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    def operating_system(self):
        return 'ios-simulator'

    def check_for_leaks(self, process_name, process_pid):
        if not self.get_option('leaks'):
            return
        # We could use http://code.google.com/p/psutil/ to get the process_name from the pid.
        self._leak_detector.check_for_leaks(process_name, process_pid)

    def print_leaks_summary(self):
        if not self.get_option('leaks'):
            return
        # We're in the manager process, so the leak detector will not have a valid list of leak files.
        leaks_files = self._leak_detector.leaks_files_in_directory(self.results_directory())
        if not leaks_files:
            return
        total_bytes_string, unique_leaks = self._leak_detector.count_total_bytes_and_unique_leaks(leaks_files)
        total_leaks = self._leak_detector.count_total_leaks(leaks_files)
        _log.info("%s total leaks found for a total of %s!" % (total_leaks, total_bytes_string))
        _log.info("%s unique leaks found!" % unique_leaks)

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')

    def show_results_html_file(self, results_filename):
        # We don't use self._run_script() because we don't want to wait for the script
        # to exit and we want the output to show up on stdout in case there are errors
        # launching the browser.
        self._executive.popen([self.path_to_script('run-safari')] + self._arguments_for_configuration() + ['--no-saved-state', '-NSOpen', results_filename],
            cwd=self.webkit_base(), stdout=file(os.devnull), stderr=file(os.devnull))

    def acquire_http_lock(self):
        pass

    def release_http_lock(self):
        pass

    def sample_file_path(self, name, pid):
        return self._filesystem.join(self.results_directory(), "{0}-{1}-sample.txt".format(name, pid))

    SUBPROCESS_CRASH_REGEX = re.compile('#CRASHED - (?P<subprocess_name>\S+) \(pid (?P<subprocess_pid>\d+)\)')

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, time_fn=time.time, sleep_fn=time.sleep, wait_for_log=True):
        time_fn = time_fn or time.time
        sleep_fn = sleep_fn or time.sleep

        # FIXME: We should collect the actual crash log for DumpRenderTree.app because it includes more
        # information (e.g. exception codes) than is available in the stack trace written to standard error.
        stderr_lines = []
        crashed_subprocess_name_and_pid = None  # e.g. ('DumpRenderTree.app', 1234)
        for line in (stderr or '').splitlines():
            if not crashed_subprocess_name_and_pid:
                match = self.SUBPROCESS_CRASH_REGEX.match(line)
                if match:
                    crashed_subprocess_name_and_pid = (match.group('subprocess_name'), int(match.group('subprocess_pid')))
                    continue
            stderr_lines.append(line)

        if crashed_subprocess_name_and_pid:
            return self._get_crash_log(crashed_subprocess_name_and_pid[0], crashed_subprocess_name_and_pid[1], stdout,
                '\n'.join(stderr_lines), newer_than, time_fn, sleep_fn, wait_for_log)

        # LayoutTestRelay crashed
        _log.debug('looking for crash log for %s:%s' % (name, str(pid)))
        crash_log = ''
        crash_logs = CrashLogs(self.host)
        now = time_fn()
        deadline = now + 5 * int(self.get_option('child_processes', 1))
        while not crash_log and now <= deadline:
            crash_log = crash_logs.find_newest_log(name, pid, include_errors=True, newer_than=newer_than)
            if not wait_for_log:
                break
            if not crash_log or not [line for line in crash_log.splitlines() if not line.startswith('ERROR')]:
                sleep_fn(0.1)
                now = time_fn()

        if not crash_log:
            return stderr, None
        return stderr, crash_log

    @property
    def testing_device(self):
        if self._testing_device is not None:
            return self._testing_device

        device_type = self.get_option('device_type')
        runtime = self.get_option('runtime')
        self._testing_device = simulator.Simulator().lookup_or_create_device(device_type.name + ' WebKit Tester', device_type, runtime)
        return self.testing_device

    def simulator_path(self, udid):
        if udid:
            return os.path.realpath(os.path.expanduser(os.path.join('~/Library/Developer/CoreSimulator/Devices', udid)))

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        crash_logs = {}
        for (test_name, process_name, pid) in crashed_processes:
            # Passing None for output.  This is a second pass after the test finished so
            # if the output had any logging we would have already collected it.
            crash_log = self._get_crash_log(process_name, pid, None, None, start_time, wait_for_log=False)[1]
            if not crash_log:
                continue
            crash_logs[test_name] = crash_log
        return crash_logs

    def look_for_new_samples(self, unresponsive_processes, start_time):
        sample_files = {}
        for (test_name, process_name, pid) in unresponsive_processes:
            sample_file = self.sample_file_path(process_name, pid)
            if not self._filesystem.isfile(sample_file):
                continue
            sample_files[test_name] = sample_file
        return sample_files

    def sample_process(self, name, pid):
        try:
            hang_report = self.sample_file_path(name, pid)
            self._executive.run_command([
                "/usr/bin/sample",
                pid,
                10,
                10,
                "-file",
                hang_report,
            ])
        except ScriptError as e:
            _log.warning('Unable to sample process:' + str(e))

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper'
        return self._build_path(binary_name)

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        if not actual_contents and not expected_contents:
            return (None, 0, None)
        if not actual_contents or not expected_contents:
            return (True, 0, None)
        if not self._image_differ:
            self._image_differ = image_diff.IOSSimulatorImageDiffer(self)
        self.set_option_default('tolerance', 0.1)
        if tolerance is None:
            tolerance = self.get_option('tolerance')
        return self._image_differ.diff_image(expected_contents, actual_contents, tolerance)

    def reset_preferences(self):
        simulator_path = self.testing_device.path
        data_path = os.path.join(simulator_path, 'data')
        if os.path.isdir(data_path):
            shutil.rmtree(data_path)

    def make_command(self):
        return self.xcrun_find('make', '/usr/bin/make')

    def nm_command(self):
        return self.xcrun_find('nm')

    def xcrun_find(self, command, fallback=None):
        fallback = fallback or command
        try:
            return self._executive.run_command(['xcrun', '--sdk', 'iphonesimulator', '-find', command]).rstrip()
        except ScriptError:
            _log.warn("xcrun failed; falling back to '%s'." % fallback)
            return fallback

    @property
    def developer_dir(self):
        return self._executive.run_command(['xcode-select', '--print-path']).rstrip()

    def logging_patterns_to_strip(self):
        return []
