# Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
import re
import shutil
import subprocess
import time

from webkitpy.common.memoized import memoized
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port import image_diff
from webkitpy.port.ios import IOSPort
from webkitpy.xcode.simulator import Simulator, Runtime, DeviceType
from webkitpy.common.system.crashlogs import CrashLogs


_log = logging.getLogger(__name__)


class IOSSimulatorPort(IOSPort):
    port_name = "ios-simulator"

    FUTURE_VERSION = 'future'
    ARCHITECTURES = ['x86_64', 'x86']
    DEFAULT_ARCHITECTURE = 'x86_64'

    DEFAULT_DEVICE_CLASS = 'iphone'
    CUSTOM_DEVICE_CLASSES = ['ipad', 'iphone7']
    SDK = 'iphonesimulator'

    SIMULATOR_BUNDLE_ID = 'com.apple.iphonesimulator'
    SIMULATOR_DIRECTORY = "/tmp/WebKitTestingSimulators/"
    LSREGISTER_PATH = "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Versions/Current/Support/lsregister"
    PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE = 100

    DEVICE_CLASS_MAP = {
        'x86_64': {
            'iphone': 'iPhone 5s',
            'iphone7': 'iPhone 7',
            'ipad': 'iPad Air',
        },
        'x86': {
            'iphone': 'iPhone 5',
            'ipad': 'iPad Retina',
        },
    }

    def __init__(self, host, port_name, **kwargs):
        super(IOSSimulatorPort, self).__init__(host, port_name, **kwargs)

        optional_device_class = self.get_option('device_class')
        self._device_class = optional_device_class if optional_device_class else self.DEFAULT_DEVICE_CLASS
        _log.debug('IOSSimulatorPort _device_class is %s', self._device_class)

        self._current_device = Simulator(host).current_device()
        if not self._current_device:
            self.set_option('dedicated_simulators', True)
        if not self.get_option('dedicated_simulators'):
            if self.get_option('child_processes') > 1:
                _log.warn('Cannot have more than one child process when using a running simulator.  Setting child_processes to 1.')
            self.set_option('child_processes', 1)

    def _device_for_worker_number_map(self):
        return Simulator.managed_devices

    @property
    @memoized
    def simulator_runtime(self):
        runtime_identifier = self.get_option('runtime')
        if runtime_identifier:
            runtime = Runtime.from_identifier(runtime_identifier)
        else:
            runtime = Runtime.from_version_string(self.host.platform.xcode_sdk_version('iphonesimulator'))
        return runtime

    def simulator_device_type(self):
        device_type_identifier = self.get_option('device_type')
        if device_type_identifier:
            _log.debug('simulator_device_type for device identifier %s', device_type_identifier)
            device_type = DeviceType.from_identifier(device_type_identifier)
        else:
            _log.debug('simulator_device_type for device %s', self._device_class)
            device_name = self.DEVICE_CLASS_MAP[self.architecture()][self._device_class]
            if not device_name:
                raise Exception('Failed to find device for architecture {} and device class {}'.format(self.architecture()), self._device_class)
            device_type = DeviceType.from_name(device_name)
        return device_type

    @memoized
    def default_child_processes(self):
        """Return the number of Simulators instances to use for this port."""
        best_child_process_count_for_cpu = self._executive.cpu_count() / 2
        system_process_count_limit = int(subprocess.check_output(["ulimit", "-u"]).strip())
        current_process_count = len(subprocess.check_output(["ps", "aux"]).strip().split('\n'))
        _log.debug('Process limit: %d, current #processes: %d' % (system_process_count_limit, current_process_count))
        maximum_simulator_count_on_this_system = (system_process_count_limit - current_process_count) // self.PROCESS_COUNT_ESTIMATE_PER_SIMULATOR_INSTANCE
        # FIXME: We should also take into account the available RAM.

        if (maximum_simulator_count_on_this_system < best_child_process_count_for_cpu):
            _log.warn("This machine could support %s simulators, but is only configured for %s."
                % (best_child_process_count_for_cpu, maximum_simulator_count_on_this_system))
            _log.warn('Please see <https://trac.webkit.org/wiki/IncreasingKernelLimits>.')

        if maximum_simulator_count_on_this_system == 0:
            maximum_simulator_count_on_this_system = 1

        return min(maximum_simulator_count_on_this_system, best_child_process_count_for_cpu)

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

        # App crashed
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

    def _build_driver_flags(self):
        archs = ['ARCHS=i386'] if self.architecture() == 'x86' else []
        sdk = ['--sdk', 'iphonesimulator']
        return archs + sdk

    def _generate_all_test_configurations(self):
        configurations = []
        for build_type in self.ALL_BUILD_TYPES:
            for architecture in self.ARCHITECTURES:
                configurations.append(TestConfiguration(version=self._version, architecture=architecture, build_type=build_type))
        return configurations

    def default_baseline_search_path(self):
        if self.get_option('webkit_test_runner'):
            fallback_names = [self._wk2_port_name()] + [self.port_name] + ['wk2']
        else:
            fallback_names = [self.port_name + '-wk1'] + [self.port_name]

        return map(self._webkit_baseline_path, fallback_names)

    def _set_device_class(self, device_class):
        self._device_class = device_class if device_class else self.DEFAULT_DEVICE_CLASS

    def _create_simulators(self):
        if (self.default_child_processes() < self.child_processes()):
            _log.warn('You have specified very high value({0}) for --child-processes'.format(self.child_processes()))
            _log.warn('maximum child-processes which can be supported on this system are: {0}'.format(self.default_child_processes()))
            _log.warn('This is very likely to fail.')

        if self._using_dedicated_simulators():
            self._createSimulatorApps()

            for i in xrange(self.child_processes()):
                self._create_device(i)

            for i in xrange(self.child_processes()):
                device_udid = self._testing_device(i).udid
                Simulator.wait_until_device_is_in_state(device_udid, Simulator.DeviceState.SHUTDOWN)
                Simulator.reset_device(device_udid)
        else:
            assert(self._current_device)
            if self._current_device.name != self.simulator_device_type().name:
                _log.warn("Expected simulator of type '" + self.simulator_device_type().name + "' but found simulator of type '" + self._current_device.name + "'")
                _log.warn('The next block of tests may fail due to device mis-match')

    def setup_test_run(self, device_class=None):
        mac_os_version = self.host.platform.os_version

        self._set_device_class(device_class)

        _log.debug('')
        _log.debug('setup_test_run for %s', self._device_class)

        self._create_simulators()

        if not self._using_dedicated_simulators():
            return

        for i in xrange(self.child_processes()):
            device_udid = self._testing_device(i).udid
            _log.debug('testing device %s has udid %s', i, device_udid)

            # FIXME: <rdar://problem/20916140> Switch to using CoreSimulator.framework for launching and quitting iOS Simulator
            self._executive.run_command([
                'open', '-g', '-b', self.SIMULATOR_BUNDLE_ID + str(i),
                '--args', '-CurrentDeviceUDID', device_udid])

            if mac_os_version in ['elcapitan', 'yosemite', 'mavericks']:
                time.sleep(2.5)

        _log.info('Waiting for all iOS Simulators to finish booting.')
        for i in xrange(self.child_processes()):
            Simulator.wait_until_device_is_booted(self._testing_device(i).udid)

    def _quit_ios_simulator(self):
        if not self._using_dedicated_simulators():
            return
        _log.debug("_quit_ios_simulator killing all Simulator processes")
        # FIXME: We should kill only the Simulators we started.
        subprocess.call(["killall", "-9", "-m", "Simulator"])

    def clean_up_test_run(self):
        super(IOSSimulatorPort, self).clean_up_test_run()
        _log.debug("clean_up_test_run")
        self._quit_ios_simulator()
        fifos = [path for path in os.listdir('/tmp') if re.search('org.webkit.(DumpRenderTree|WebKitTestRunner).*_(IN|OUT|ERROR)', path)]
        for fifo in fifos:
            try:
                os.remove(os.path.join('/tmp', fifo))
            except OSError:
                _log.warning('Unable to remove ' + fifo)
                pass

        if not self._using_dedicated_simulators():
            return

        for i in xrange(self.child_processes()):
            simulator_path = self.get_simulator_path(i)
            device_udid = self._testing_device(i).udid
            self._remove_device(i)

            if not os.path.exists(simulator_path):
                continue
            try:
                self._executive.run_command([self.LSREGISTER_PATH, "-u", simulator_path])

                _log.debug('rmtree %s', simulator_path)
                self._filesystem.rmtree(simulator_path)

                logs_path = self._filesystem.join(self._filesystem.expanduser("~"), "Library/Logs/CoreSimulator/", device_udid)
                _log.debug('rmtree %s', logs_path)
                self._filesystem.rmtree(logs_path)

                saved_state_path = self._filesystem.join(self._filesystem.expanduser("~"), "Library/Saved Application State/", self.SIMULATOR_BUNDLE_ID + str(i) + ".savedState")
                _log.debug('rmtree %s', saved_state_path)
                self._filesystem.rmtree(saved_state_path)

            except:
                _log.warning('Unable to remove Simulator' + str(i))

    def setup_environ_for_server(self, server_name=None):
        _log.debug("setup_environ_for_server")
        env = super(IOSSimulatorPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
                env['__XPC_MallocStackLogging'] = '1'
                env['MallocScribble'] = '1'
                env['__XPC_MallocScribble'] = '1'
            if self.get_option('guard_malloc'):
                self._append_value_colon_separated(env, 'DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
                self._append_value_colon_separated(env, '__XPC_DYLD_INSERT_LIBRARIES', '/usr/lib/libgmalloc.dylib')
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    def operating_system(self):
        return 'ios-simulator'

    def check_sys_deps(self, needs_http):
        if not self.simulator_runtime.available:
            _log.error('The iOS Simulator runtime with identifier "{0}" cannot be used because it is unavailable.'.format(self.simulator_runtime.identifier))
            return False
        return super(IOSSimulatorPort, self).check_sys_deps(needs_http)

    SUBPROCESS_CRASH_REGEX = re.compile('#CRASHED - (?P<subprocess_name>\S+) \(pid (?P<subprocess_pid>\d+)\)')

    def _using_dedicated_simulators(self):
        return self.get_option('dedicated_simulators')

    def using_multiple_devices(self):
        return self._using_dedicated_simulators()

    def _create_device(self, number):
        return Simulator.create_device(number, self.simulator_device_type(), self.simulator_runtime)

    def _remove_device(self, number):
        Simulator.remove_device(number)

    def get_simulator_path(self, suffix=""):
        return os.path.join(self.SIMULATOR_DIRECTORY, "Simulator" + str(suffix) + ".app")

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
        _log.debug("reset_preferences")
        self._quit_ios_simulator()
        # Maybe this should delete all devices that we've created?

    def nm_command(self):
        return self.xcrun_find('nm')

    @property
    @memoized
    def developer_dir(self):
        return self._executive.run_command(['xcode-select', '--print-path']).rstrip()

    def logging_patterns_to_strip(self):
        return []

    def stderr_patterns_to_strip(self):
        return []

    def _createSimulatorApps(self):
        for i in xrange(self.child_processes()):
            self._createSimulatorApp(i)

    def _createSimulatorApp(self, suffix):
        destination = self.get_simulator_path(suffix)
        _log.info("Creating app:" + destination)
        if os.path.exists(destination):
            shutil.rmtree(destination, ignore_errors=True)
        simulator_app_path = self.developer_dir + "/Applications/Simulator.app"
        shutil.copytree(simulator_app_path, destination)

        # Update app's package-name inside plist and re-code-sign it
        plist_path = destination + "/Contents/Info.plist"
        command = "Set CFBundleIdentifier com.apple.iphonesimulator" + str(suffix)
        subprocess.check_output(["/usr/libexec/PlistBuddy", "-c", command, plist_path])
        subprocess.check_output(["install_name_tool", "-add_rpath", self.developer_dir + "/Library/PrivateFrameworks/", destination + "/Contents/MacOS/Simulator"])
        subprocess.check_output(["install_name_tool", "-add_rpath", self.developer_dir + "/../Frameworks/", destination + "/Contents/MacOS/Simulator"])
        subprocess.check_output(["codesign", "-fs", "-", destination])
        subprocess.check_output([self.LSREGISTER_PATH, "-f", destination])
