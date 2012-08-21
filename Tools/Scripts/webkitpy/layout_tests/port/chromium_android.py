#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import copy
import logging
import os
import re
import threading
import time

from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import server_process


_log = logging.getLogger(__name__)


# The root directory for test resources, which has the same structure as the
# source root directory of Chromium.
# This path is defined in Chromium's base/test/test_support_android.cc.
DEVICE_SOURCE_ROOT_DIR = '/data/local/tmp/'
COMMAND_LINE_FILE = DEVICE_SOURCE_ROOT_DIR + 'chrome-native-tests-command-line'

# The directory to put tools and resources of DumpRenderTree.
# If change this, must also change Tools/DumpRenderTree/chromium/TestShellAndroid.cpp
# and Chromium's webkit/support/platform_support_android.cc.
DEVICE_DRT_DIR = DEVICE_SOURCE_ROOT_DIR + 'drt/'
DEVICE_FORWARDER_PATH = DEVICE_DRT_DIR + 'forwarder'
DEVICE_DRT_STAMP_PATH = DEVICE_DRT_DIR + 'DumpRenderTree.stamp'

DRT_APP_PACKAGE = 'org.chromium.native_test'
DRT_ACTIVITY_FULL_NAME = DRT_APP_PACKAGE + '/.ChromeNativeTestActivity'
DRT_APP_CACHE_DIR = DEVICE_DRT_DIR + 'cache/'

SCALING_GOVERNORS_PATTERN = "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"

# All the test cases are still served to DumpRenderTree through file protocol,
# but we use a file-to-http feature to bridge the file request to host's http
# server to get the real test files and corresponding resources.
TEST_PATH_PREFIX = '/all-tests'

# All ports the Android forwarder to forward.
# 8000, 8080 and 8443 are for http/https tests.
# 8880 and 9323 are for websocket tests
# (see http_server.py, apache_http_server.py and websocket_server.py).
FORWARD_PORTS = '8000 8080 8443 8880 9323'

MS_TRUETYPE_FONTS_DIR = '/usr/share/fonts/truetype/msttcorefonts/'

# Timeout in seconds to wait for start/stop of DumpRenderTree.
DRT_START_STOP_TIMEOUT_SECS = 10

# List of fonts that layout tests expect, copied from DumpRenderTree/gtk/TestShellX11.cpp.
HOST_FONT_FILES = [
    [MS_TRUETYPE_FONTS_DIR, 'Arial.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Arial_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Arial_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Arial_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Comic_Sans_MS.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Comic_Sans_MS_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Courier_New.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Courier_New_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Courier_New_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Courier_New_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Georgia.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Georgia_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Georgia_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Georgia_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Impact.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Trebuchet_MS.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Trebuchet_MS_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Trebuchet_MS_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Trebuchet_MS_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Times_New_Roman.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Times_New_Roman_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Times_New_Roman_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Times_New_Roman_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Verdana.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Verdana_Bold.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Verdana_Bold_Italic.ttf'],
    [MS_TRUETYPE_FONTS_DIR, 'Verdana_Italic.ttf'],
    # The Microsoft font EULA
    ['/usr/share/doc/ttf-mscorefonts-installer/', 'READ_ME!.gz'],
    ['/usr/share/fonts/truetype/ttf-dejavu/', 'DejaVuSans.ttf'],
]
# Should increase this version after changing HOST_FONT_FILES.
FONT_FILES_VERSION = 2

DEVICE_FONTS_DIR = DEVICE_DRT_DIR + 'fonts/'

# The layout tests directory on device, which has two usages:
# 1. as a virtual path in file urls that will be bridged to HTTP.
# 2. pointing to some files that are pushed to the device for tests that
# don't work on file-over-http (e.g. blob protocol tests).
DEVICE_LAYOUT_TESTS_DIR = DEVICE_SOURCE_ROOT_DIR + 'third_party/WebKit/LayoutTests/'

# Test resources that need to be accessed as files directly.
# Each item can be the relative path of a directory or a file.
TEST_RESOURCES_TO_PUSH = [
    # Blob tests need to access files directly.
    'editing/pasteboard/resources',
    'fast/files/resources',
    'http/tests/local/resources',
    'http/tests/local/formdata/resources',
    # User style URLs are accessed as local files in webkit_support.
    'http/tests/security/resources/cssStyle.css',
    # Media tests need to access audio/video as files.
    'media/content',
    'compositing/resources/video.mp4',
]


class ChromiumAndroidPort(chromium.ChromiumPort):
    port_name = 'chromium-android'

    FALLBACK_PATHS = [
        'chromium-android',
        'chromium-linux',
        'chromium-win',
        'chromium',
        'win',
        'mac',
    ]

    def __init__(self, host, port_name, **kwargs):
        super(ChromiumAndroidPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)

        if hasattr(self._options, 'adb_device'):
            self._devices = self._options.adb_device
        else:
            self._devices = []

    def additional_drt_flag(self):
        # The Chromium port for Android always uses the hardware GPU path.
        return ['--encode-binary', '--enable-hardware-gpu']

    def default_timeout_ms(self):
        # Android platform has less computing power than desktop platforms.
        # Using 10 seconds allows us to pass most slow tests which are not
        # marked as slow tests on desktop platforms.
        return 10 * 1000

    def driver_stop_timeout(self):
        # DRT doesn't respond to closing stdin, so we might as well stop the driver immediately.
        return 0.0

    def default_child_processes(self):
        return len(self._get_devices())

    def baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS)

    def check_wdiff(self, logging=True):
        return self._host_port.check_wdiff(logging)

    def check_build(self, needs_http):
        result = super(ChromiumAndroidPort, self).check_build(needs_http)
        result = self.check_wdiff() and result
        if not result:
            _log.error('For complete Android build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/AndroidBuildInstructions')

        return result

    def check_sys_deps(self, needs_http):
        for (font_dir, font_file) in HOST_FONT_FILES:
            font_path = font_dir + font_file
            if not self._check_file_exists(font_path, 'font file'):
                _log.error('You are missing %s. Try installing msttcorefonts. '
                           'See build instructions.' % font_path)
                return False
        return True

    # FIXME: Remove this function when chromium-android is fully upstream.
    def expectations_files(self):
        android_expectations_file = self.path_from_webkit_base('LayoutTests', 'platform', 'chromium', 'test_expectations_android.txt')
        return super(ChromiumAndroidPort, self).expectations_files() + [android_expectations_file]

    def start_http_server(self, additional_dirs=None, number_of_servers=0):
        # The http server runs during the whole testing period, so ignore this call.
        pass

    def stop_http_server(self):
        # Same as start_http_server().
        pass

    def setup_test_run(self):
        # Start the HTTP server so that the device can access the test cases.
        super(ChromiumAndroidPort, self).start_http_server(additional_dirs={TEST_PATH_PREFIX: self.layout_tests_dir()})

    def clean_up_test_run(self):
        super(ChromiumAndroidPort, self).stop_http_server()

    def skipped_layout_tests(self, test_list):
        # This method is more convenient to skip whole directories than SKIP in TestExpectations
        # because its higher priority.
        # Still use TestExpectations to skip individual tests.
        return self._real_tests([
            # Only run these tests as virtual gpu tests.
            'canvas/philip',
            'fast/canvas',

            # Skip tests of other platforms to save time.
            'platform/gtk',
            'platform/mac',
            'platform/mac-wk2',
            'platform/qt',
            'platform/win',

            # Features not supported.
            'compositing/plugins',
            'plugins',
            'http/tests/plugins',
            'platform/chromium/compositing/plugins',
            'platform/chromium/plugins',

            'http/tests/inspector',
            'http/tests/inspector-enabled',
            'inspector',
            'platform/chromium/inspector',

            'accessibility',
            'platform/chromium/accessibility',

            'fast/dom/MediaStream',
            'fast/mediastream',
            'fast/notifications',
            'fast/speech',
            'webaudio',
        ])

    def create_driver(self, worker_number, no_timeout=False):
        # We don't want the default DriverProxy which is not compatible with our driver.
        # See comments in ChromiumAndroidDriver.start().
        return ChromiumAndroidDriver(self, worker_number, pixel_tests=self.get_option('pixel_tests'),
                                     # Force no timeout to avoid DumpRenderTree timeouts before NRWT.
                                     no_timeout=True)

    def driver_cmd_line(self):
        # Override to return the actual DumpRenderTree command line.
        return self.create_driver(0)._drt_cmd_line(self.get_option('pixel_tests'), [])

    # Overridden private functions.

    def _build_path(self, *comps):
        return self._host_port._build_path(*comps)

    def _build_path_with_configuration(self, configuration, *comps):
        return self._host_port._build_path_with_configuration(configuration, *comps)

    def _path_to_apache(self):
        return self._host_port._path_to_apache()

    def _path_to_apache_config_file(self):
        return self._host_port._path_to_apache_config_file()

    def _path_to_driver(self, configuration=None):
        return self._build_path_with_configuration(configuration, 'DumpRenderTree_apk/DumpRenderTree-debug.apk')

    def _path_to_helper(self):
        return None

    def _path_to_forwarder(self):
        return self._build_path('forwarder')

    def _path_to_image_diff(self):
        return self._host_port._path_to_image_diff()

    def _path_to_lighttpd(self):
        return self._host_port._path_to_lighttpd()

    def _path_to_lighttpd_modules(self):
        return self._host_port._path_to_lighttpd_modules()

    def _path_to_lighttpd_php(self):
        return self._host_port._path_to_lighttpd_php()

    def _path_to_wdiff(self):
        return self._host_port._path_to_wdiff()

    def _shut_down_http_server(self, pid):
        return self._host_port._shut_down_http_server(pid)

    def _driver_class(self):
        return ChromiumAndroidDriver

    # Local private functions.

    def _get_devices(self):
        if not self._devices:
            re_device = re.compile('^([a-zA-Z0-9_:.-]+)\tdevice$', re.MULTILINE)
            result = self._executive.run_command(['adb', 'devices'], error_handler=self._executive.ignore_error)
            self._devices = re_device.findall(result)
            if not self._devices:
                raise AssertionError('No devices attached. Result of "adb devices": %s' % result)
        return self._devices

    def _get_device_serial(self, worker_number):
        devices = self._get_devices()
        if worker_number >= len(devices):
            raise AssertionError('Worker number exceeds available number of devices')
        return devices[worker_number]


class ChromiumAndroidDriver(driver.Driver):
    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        super(ChromiumAndroidDriver, self).__init__(port, worker_number, pixel_tests, no_timeout)
        self._cmd_line = None
        self._in_fifo_path = DEVICE_DRT_DIR + 'DumpRenderTree.in'
        self._out_fifo_path = DEVICE_DRT_DIR + 'DumpRenderTree.out'
        self._err_fifo_path = DEVICE_DRT_DIR + 'DumpRenderTree.err'
        self._read_stdout_process = None
        self._read_stderr_process = None
        self._forwarder_process = None
        self._has_setup = False
        self._original_governors = {}
        self._adb_command = ['adb', '-s', port._get_device_serial(worker_number)]

    def __del__(self):
        self._teardown_performance()
        super(ChromiumAndroidDriver, self).__del__()

    def _setup_test(self):
        if self._has_setup:
            return

        self._has_setup = True
        self._run_adb_command(['root'])
        self._setup_performance()
        # Required by webkit_support::GetWebKitRootDirFilePath().
        # Other directories will be created automatically by adb push.
        self._run_adb_command(['shell', 'mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'])

        # Allow the DumpRenderTree app to fully access the directory.
        # The native code needs the permission to write temporary files and create pipes here.
        self._run_adb_command(['shell', 'mkdir', '-p', DEVICE_DRT_DIR])
        self._run_adb_command(['shell', 'chmod', '777', DEVICE_DRT_DIR])

        self._push_executable()
        self._push_fonts()
        self._synchronize_datetime()

        # Delete the disk cache if any to ensure a clean test run.
        # This is like what's done in ChromiumPort.setup_test_run but on the device.
        self._run_adb_command(['shell', 'rm', '-r', DRT_APP_CACHE_DIR])

    def _push_executable(self):
        drt_host_path = self._port._path_to_driver()
        forwarder_host_path = self._port._path_to_forwarder()
        host_stamp = int(float(max(os.stat(drt_host_path).st_mtime,
                                   os.stat(forwarder_host_path).st_mtime)))
        device_stamp = int(float(self._run_adb_command([
            'shell', 'cat %s 2>/dev/null || echo 0' % DEVICE_DRT_STAMP_PATH])))
        if device_stamp != host_stamp:
            _log.debug('Pushing executable')
            self._push_to_device(forwarder_host_path, DEVICE_FORWARDER_PATH)
            self._run_adb_command(['uninstall', DRT_APP_PACKAGE])
            install_result = self._run_adb_command(['install', drt_host_path])
            if install_result.find('Success') == -1:
                raise AssertionError('Failed to install %s onto device: %s' % (drt_host_path, install_result))
            self._push_to_device(self._port._build_path('DumpRenderTree.pak'), DEVICE_DRT_DIR + 'DumpRenderTree.pak')
            self._push_to_device(self._port._build_path('DumpRenderTree_resources'), DEVICE_DRT_DIR + 'DumpRenderTree_resources')
            self._push_to_device(self._port._build_path('android_main_fonts.xml'), DEVICE_DRT_DIR + 'android_main_fonts.xml')
            self._push_to_device(self._port._build_path('android_fallback_fonts.xml'), DEVICE_DRT_DIR + 'android_fallback_fonts.xml')
            # Version control of test resources is dependent on executables,
            # because we will always rebuild executables when resources are
            # updated.
            self._push_test_resources()
            self._run_adb_command(['shell', 'echo %d >%s' % (host_stamp, DEVICE_DRT_STAMP_PATH)])

    def _push_fonts(self):
        if not self._check_version(DEVICE_FONTS_DIR, FONT_FILES_VERSION):
            _log.debug('Pushing fonts')
            path_to_ahem_font = self._port._build_path('AHEM____.TTF')
            self._push_to_device(path_to_ahem_font, DEVICE_FONTS_DIR + 'AHEM____.TTF')
            for (host_dir, font_file) in HOST_FONT_FILES:
                self._push_to_device(host_dir + font_file, DEVICE_FONTS_DIR + font_file)
            self._link_device_file('/system/fonts/DroidSansFallback.ttf', DEVICE_FONTS_DIR + 'DroidSansFallback.ttf')
            self._update_version(DEVICE_FONTS_DIR, FONT_FILES_VERSION)

    def _push_test_resources(self):
        _log.debug('Pushing test resources')
        for resource in TEST_RESOURCES_TO_PUSH:
            self._push_to_device(self._port.layout_tests_dir() + '/' + resource, DEVICE_LAYOUT_TESTS_DIR + resource)

    def _synchronize_datetime(self):
        # The date/time between host and device may not be synchronized.
        # We need to make them synchronized, otherwise tests might fail.
        try:
            # Get seconds since 1970-01-01 00:00:00 UTC.
            host_datetime = self._port._executive.run_command(['date', '-u', '+%s'])
        except:
            # Reset to 1970-01-01 00:00:00 UTC.
            host_datetime = 0
        self._run_adb_command(['shell', 'date -u %s' % (host_datetime)])

    def _check_version(self, dir, version):
        assert(dir.endswith('/'))
        try:
            device_version = int(self._run_adb_command(['shell', 'cat %sVERSION || echo 0' % dir]))
            return device_version == version
        except:
            return False

    def _update_version(self, dir, version):
        self._run_adb_command(['shell', 'echo %d > %sVERSION' % (version, dir)])

    def _run_adb_command(self, cmd, ignore_error=False):
        _log.debug('Run adb command: ' + str(cmd))
        if ignore_error:
            error_handler = self._port._executive.ignore_error
        else:
            error_handler = None
        result = self._port._executive.run_command(self._adb_command + cmd, error_handler=error_handler)
        # Limit the length to avoid too verbose output of commands like 'adb logcat' and 'cat /data/tombstones/tombstone01'
        # whose outputs are normally printed in later logs.
        _log.debug('Run adb result: ' + result[:80])
        return result

    def _link_device_file(self, from_file, to_file, ignore_error=False):
        # rm to_file first to make sure that ln succeeds.
        self._run_adb_command(['shell', 'rm', to_file], ignore_error)
        return self._run_adb_command(['shell', 'ln', '-s', from_file, to_file], ignore_error)

    def _push_to_device(self, host_path, device_path, ignore_error=False):
        return self._run_adb_command(['push', host_path, device_path], ignore_error)

    def _pull_from_device(self, device_path, host_path, ignore_error=False):
        return self._run_adb_command(['pull', device_path, host_path], ignore_error)

    def _get_last_stacktrace(self):
        tombstones = self._run_adb_command(['shell', 'ls', '-n', '/data/tombstones'])
        if not tombstones or tombstones.startswith('/data/tombstones: No such file or directory'):
            _log.error('DRT crashed, but no tombstone found!')
            return ''
        tombstones = tombstones.rstrip().split('\n')
        last_tombstone = tombstones[0].split()
        for tombstone in tombstones[1:]:
            # Format of fields:
            # 0          1      2      3     4          5     6
            # permission uid    gid    size  date       time  filename
            # -rw------- 1000   1000   45859 2011-04-13 06:00 tombstone_00
            fields = tombstone.split()
            if (fields[4] + fields[5] >= last_tombstone[4] + last_tombstone[5]):
                last_tombstone = fields
            else:
                break

        # Use Android tool vendor/google/tools/stack to convert the raw
        # stack trace into a human readable format, if needed.
        # It takes a long time, so don't do it here.
        return '%s\n%s' % (' '.join(last_tombstone),
                           self._run_adb_command(['shell', 'cat', '/data/tombstones/' + last_tombstone[6]]))

    def _get_logcat(self):
        return self._run_adb_command(['logcat', '-d'])

    def _setup_performance(self):
        # Disable CPU scaling and drop ram cache to reduce noise in tests
        if not self._original_governors:
            governor_files = self._run_adb_command(['shell', 'ls', SCALING_GOVERNORS_PATTERN])
            if governor_files.find('No such file or directory') == -1:
                for file in governor_files.split():
                    self._original_governors[file] = self._run_adb_command(['shell', 'cat', file]).strip()
                    self._run_adb_command(['shell', 'echo', 'performance', '>', file])

    def _teardown_performance(self):
        for file, original_content in self._original_governors.items():
            self._run_adb_command(['shell', 'echo', original_content, '>', file])
        self._original_governors = {}

    def _command_wrapper(cls, wrapper_option):
        # Ignore command wrapper which is not applicable on Android.
        return []

    def _get_crash_log(self, stdout, stderr, newer_than):
        if not stdout:
            stdout = ''
        stdout += '********* Logcat:\n' + self._get_logcat()
        if not stderr:
            stderr = ''
        stderr += '********* Tombstone file:\n' + self._get_last_stacktrace()
        return super(ChromiumAndroidDriver, self)._get_crash_log(stdout, stderr, newer_than)

    def cmd_line(self, pixel_tests, per_test_args):
        # The returned command line is used to start _server_process. In our case, it's an interactive 'adb shell'.
        # The command line passed to the DRT process is returned by _drt_cmd_line() instead.
        return self._adb_command + ['shell']

    def _file_exists_on_device(self, full_file_path):
        assert full_file_path.startswith('/')
        return self._run_adb_command(['shell', 'ls', full_file_path]).strip() == full_file_path

    def _drt_cmd_line(self, pixel_tests, per_test_args):
        return driver.Driver.cmd_line(self, pixel_tests, per_test_args) + [
            '--in-fifo=' + self._in_fifo_path,
            '--out-fifo=' + self._out_fifo_path,
            '--err-fifo=' + self._err_fifo_path,
        ]

    @staticmethod
    def _loop_with_timeout(condition, timeout_secs):
        deadline = time.time() + timeout_secs
        while time.time() < deadline:
            if condition():
                return True
        return False

    def _all_pipes_created(self):
        return (self._file_exists_on_device(self._in_fifo_path) and
                self._file_exists_on_device(self._out_fifo_path) and
                self._file_exists_on_device(self._err_fifo_path))

    def _remove_all_pipes(self):
        self._run_adb_command(['shell', 'rm', self._in_fifo_path, self._out_fifo_path, self._err_fifo_path])
        return (not self._file_exists_on_device(self._in_fifo_path) and
                not self._file_exists_on_device(self._out_fifo_path) and
                not self._file_exists_on_device(self._err_fifo_path))

    def run_test(self, driver_input, stop_when_done):
        base = self._port.lookup_virtual_test_base(driver_input.test_name)
        if base:
            driver_input = copy.copy(driver_input)
            driver_input.args = self._port.lookup_virtual_test_args(driver_input.test_name)
            driver_input.test_name = base
        return super(ChromiumAndroidDriver, self).run_test(driver_input, stop_when_done)

    def start(self, pixel_tests, per_test_args):
        # Only one driver instance is allowed because of the nature of Android activity.
        # The single driver needs to restart DumpRenderTree when the command line changes.
        cmd_line = self._drt_cmd_line(pixel_tests, per_test_args)
        if cmd_line != self._cmd_line:
            self.stop()
            self._cmd_line = cmd_line
        super(ChromiumAndroidDriver, self).start(pixel_tests, per_test_args)

    def _start(self, pixel_tests, per_test_args):
        self._setup_test()

        for retries in range(3):
            if self._start_once(pixel_tests, per_test_args):
                return
            _log.error('Failed to start DumpRenderTree application. Retries=%d. Log:%s' % (retries, self._get_logcat()))
            self.stop()
            time.sleep(2)
        raise AssertionError('Failed to start DumpRenderTree application multiple times. Give up.')

    def _start_once(self, pixel_tests, per_test_args):
        super(ChromiumAndroidDriver, self)._start(pixel_tests, per_test_args)

        _log.debug('Starting forwarder')
        self._forwarder_process = server_process.ServerProcess(
            self._port, 'Forwarder', self._adb_command + ['shell', '%s -D %s' % (DEVICE_FORWARDER_PATH, FORWARD_PORTS)])
        self._forwarder_process.start()

        self._run_adb_command(['logcat', '-c'])
        self._run_adb_command(['shell', 'echo'] + self._cmd_line + ['>', COMMAND_LINE_FILE])
        start_result = self._run_adb_command(['shell', 'am', 'start', '-e', 'RunInSubThread', '-n', DRT_ACTIVITY_FULL_NAME])
        if start_result.find('Exception') != -1:
            _log.error('Failed to start DumpRenderTree application. Exception:\n' + start_result)
            return False

        if not ChromiumAndroidDriver._loop_with_timeout(self._all_pipes_created, DRT_START_STOP_TIMEOUT_SECS):
            return False

        # Read back the shell prompt to ensure adb shell ready.
        deadline = time.time() + DRT_START_STOP_TIMEOUT_SECS
        self._server_process.start()
        self._read_prompt(deadline)
        _log.debug('Interactive shell started')

        # Start a process to read from the stdout fifo of the DumpRenderTree app and print to stdout.
        _log.debug('Redirecting stdout to ' + self._out_fifo_path)
        self._read_stdout_process = server_process.ServerProcess(
            self._port, 'ReadStdout', self._adb_command + ['shell', 'cat', self._out_fifo_path], universal_newlines=True)
        self._read_stdout_process.start()

        # Start a process to read from the stderr fifo of the DumpRenderTree app and print to stdout.
        _log.debug('Redirecting stderr to ' + self._err_fifo_path)
        self._read_stderr_process = server_process.ServerProcess(
            self._port, 'ReadStderr', self._adb_command + ['shell', 'cat', self._err_fifo_path], universal_newlines=True)
        self._read_stderr_process.start()

        _log.debug('Redirecting stdin to ' + self._in_fifo_path)
        self._server_process.write('cat >%s\n' % self._in_fifo_path)

        # Combine the stdout and stderr pipes into self._server_process.
        self._server_process.replace_outputs(self._read_stdout_process._proc.stdout, self._read_stderr_process._proc.stdout)

        def deadlock_detector(processes, normal_startup_event):
            if not ChromiumAndroidDriver._loop_with_timeout(lambda: normal_startup_event.is_set(), DRT_START_STOP_TIMEOUT_SECS):
                # If normal_startup_event is not set in time, the main thread must be blocked at
                # reading/writing the fifo. Kill the fifo reading/writing processes to let the
                # main thread escape from the deadlocked state. After that, the main thread will
                # treat this as a crash.
                _log.warn('Deadlock detected. Processes killed.')
                for i in processes:
                    i.kill()

        # Start a thread to kill the pipe reading/writing processes on deadlock of the fifos during startup.
        normal_startup_event = threading.Event()
        threading.Thread(name='DeadlockDetector', target=deadlock_detector,
                         args=([self._server_process, self._read_stdout_process, self._read_stderr_process], normal_startup_event)).start()

        output = ''
        line = self._server_process.read_stdout_line(deadline)
        while not self._server_process.timed_out and not self.has_crashed() and line.rstrip() != '#READY':
            output += line
            line = self._server_process.read_stdout_line(deadline)

        if self._server_process.timed_out and not self.has_crashed():
            # DumpRenderTree crashes during startup, or when the deadlock detector detected
            # deadlock and killed the fifo reading/writing processes.
            _log.error('Failed to start DumpRenderTree: \n%s' % output)
            return False
        else:
            # Inform the deadlock detector that the startup is successful without deadlock.
            normal_startup_event.set()
            return True

    def stop(self):
        self._run_adb_command(['shell', 'am', 'force-stop', DRT_APP_PACKAGE])

        if self._read_stdout_process:
            self._read_stdout_process.kill()
            self._read_stdout_process = None

        if self._read_stderr_process:
            self._read_stderr_process.kill()
            self._read_stderr_process = None

        super(ChromiumAndroidDriver, self).stop()

        if self._forwarder_process:
            self._forwarder_process.kill()
            self._forwarder_process = None

        if not ChromiumAndroidDriver._loop_with_timeout(self._remove_all_pipes, DRT_START_STOP_TIMEOUT_SECS):
            raise AssertionError('Failed to remove fifo files. May be locked.')

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumAndroidDriver, self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            # Convert the host file path to a device file path. See comment of
            # DEVICE_LAYOUT_TESTS_DIR for details.
            command = DEVICE_LAYOUT_TESTS_DIR + self._port.relative_test_filename(command)
        return command

    def _read_prompt(self, deadline):
        last_char = ''
        while True:
            current_char = self._server_process.read_stdout(deadline, 1)
            if current_char == ' ':
                if last_char == '#':
                    return
                if last_char == '$':
                    raise AssertionError('Adbd is not running as root')
            last_char = current_char
