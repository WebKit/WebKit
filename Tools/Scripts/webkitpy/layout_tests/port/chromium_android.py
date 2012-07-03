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

import logging
import os
import re
import signal
import shlex
import shutil
import threading
import time

from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import factory


_log = logging.getLogger(__name__)


# The root directory for test resources, which has the same structure as the
# source root directory of Chromium.
# This path is defined in base/base_paths_android.cc and
# webkit/support/platform_support_android.cc.
DEVICE_SOURCE_ROOT_DIR = '/data/local/tmp/'
COMMAND_LINE_FILE = DEVICE_SOURCE_ROOT_DIR + 'chrome-native-tests-command-line'

# The directory to put tools and resources of DumpRenderTree.
DEVICE_DRT_DIR = '/data/drt/'
DEVICE_FORWARDER_PATH = DEVICE_DRT_DIR + 'forwarder'
DEVICE_DRT_STAMP_PATH = DEVICE_DRT_DIR + 'DumpRenderTree.stamp'

DRT_APP_PACKAGE = 'org.chromium.native_test'
DRT_ACTIVITY_FULL_NAME = DRT_APP_PACKAGE + '/.ChromeNativeTestActivity'
DRT_APP_FILE_DIR = '/data/user/0/' + DRT_APP_PACKAGE + '/files/'

# This only works for single core devices so far.
# FIXME: Find a solution for multi-core devices.
SCALING_GOVERNOR = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"

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
DEVICE_LAYOUT_TESTS_DIR = (DEVICE_SOURCE_ROOT_DIR + 'third_party/WebKit/LayoutTests/')
FILE_TEST_URI_PREFIX = 'file://' + DEVICE_LAYOUT_TESTS_DIR

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
        chromium.ChromiumPort.__init__(self, host, port_name, **kwargs)

        # FIXME: Stop using test_shell mode: https://bugs.webkit.org/show_bug.cgi?id=88542
        if not hasattr(self._options, 'additional_drt_flag'):
            self._options.additional_drt_flag = []
        if not '--test-shell' in self._options.additional_drt_flag:
            self._options.additional_drt_flag.append('--test-shell')

        # The Chromium port for Android always uses the hardware GPU path.
        self._options.enable_hardware_gpu = True

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'
        self._original_governor = None
        self._android_base_dir = None
        self._read_fifo_proc = None

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)

        self._adb_command = ['adb']
        adb_args = self.get_option('adb_args')
        if adb_args:
            self._adb_command += shlex.split(adb_args)
        self._drt_retry_after_killed = 0

    def default_timeout_ms(self):
        # Android platform has less computing power than desktop platforms.
        # Using 10 seconds allows us to pass most slow tests which are not
        # marked as slow tests on desktop platforms.
        return 10 * 1000

    def default_child_processes(self):
        # Because of the nature of apk, we don't support more than one process.
        return 1

    def baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS)

    def check_wdiff(self, logging=True):
        return self._host_port.check_wdiff(logging)

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
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

    def test_expectations(self):
        # Automatically apply all expectation rules of chromium-linux to
        # chromium-android.
        # FIXME: This is a temporary measure to reduce the manual work when
        # updating WebKit. This method should be removed when we merge
        # test_expectations_android.txt into TestExpectations.
        expectations = chromium.ChromiumPort.test_expectations(self)
        return expectations.replace('LINUX ', 'LINUX ANDROID ')

    def start_http_server(self, additional_dirs=None, number_of_servers=0):
        # The http server runs during the whole testing period, so ignore this call.
        pass

    def stop_http_server(self):
        # Same as start_http_server().
        pass

    def start_helper(self):
        self._run_adb_command(['root'])
        self._setup_performance()
        # Required by webkit_support::GetWebKitRootDirFilePath().
        # Other directories will be created automatically by adb push.
        self._run_adb_command(['shell', 'mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'])
        # Allow the DumpRenderTree app to fully access the directory.
        # The native code needs the permission to write temporary files here.
        self._run_adb_command(['shell', 'chmod', '777', DEVICE_SOURCE_ROOT_DIR])

        self._push_executable()
        self._push_fonts()
        self._synchronize_datetime()

        # Start the HTTP server so that the device can access the test cases.
        chromium.ChromiumPort.start_http_server(self, additional_dirs={TEST_PATH_PREFIX: self.layout_tests_dir()})

        _log.debug('Starting forwarder')
        cmd = self._run_adb_command(['shell', '%s %s' % (DEVICE_FORWARDER_PATH, FORWARD_PORTS)])

    def stop_helper(self):
        # Leave the forwarder and tests httpd server there because they are
        # useful for debugging and do no harm to subsequent tests.
        self._teardown_performance()

    def skipped_layout_tests(self, test_list):
        return self._real_tests([
            # Canvas tests are run as virtual gpu tests.
            'fast/canvas',
            'canvas/philip',
        ])

    # Overridden private functions.

    def _build_path(self, *comps):
        return self._host_port._build_path(*comps)

    def _path_to_apache(self):
        return self._host_port._path_to_apache()

    def _path_to_apache_config_file(self):
        return self._host_port._path_to_apache_config_file()

    def _path_to_driver(self, configuration=None):
        if not configuration:
            configuration = self.get_option('configuration')
        return self._build_path(configuration, 'DumpRenderTree_apk/DumpRenderTree-debug.apk')

    def _path_to_helper(self):
        return self._build_path(self.get_option('configuration'), 'forwarder')

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

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than):
        if not stdout:
            stdout = ''
        stdout += '********* Logcat:\n' + self._get_logcat()
        if not stderr:
            stderr = ''
        stderr += '********* Tombstone file:\n' + self._get_last_stacktrace()
        return chromium.ChromiumPort._get_crash_log(self, name, pid, stdout, stderr, newer_than)

    # Local private functions.

    def _push_executable(self):
        drt_host_path = self._path_to_driver()
        forwarder_host_path = self._path_to_helper()
        host_stamp = int(float(max(os.stat(drt_host_path).st_mtime,
                                   os.stat(forwarder_host_path).st_mtime)))
        device_stamp = int(float(self._run_adb_command([
            'shell', 'cat %s 2>/dev/null || echo 0' % DEVICE_DRT_STAMP_PATH])))
        if device_stamp < host_stamp:
            _log.debug('Pushing executable')
            self._push_to_device(forwarder_host_path, DEVICE_FORWARDER_PATH)
            self._run_adb_command(['uninstall', DRT_APP_PACKAGE])
            install_result = self._run_adb_command(['install', drt_host_path])
            if install_result.find('Success') == -1:
                raise AssertionError('Failed to install %s onto device: %s' % (drt_host_path, install_result))
            self._push_to_device(self._build_path(self.get_option('configuration'), 'DumpRenderTree.pak'),
                                 DEVICE_DRT_DIR + 'DumpRenderTree.pak')
            self._push_to_device(self._build_path(self.get_option('configuration'), 'DumpRenderTree_resources'),
                                 DEVICE_DRT_DIR + 'DumpRenderTree_resources')
            self._push_to_device(self._build_path(self.get_option('configuration'), 'android_main_fonts.xml'),
                                 DEVICE_DRT_DIR + 'android_main_fonts.xml')
            self._push_to_device(self._build_path(self.get_option('configuration'), 'android_fallback_fonts.xml'),
                                 DEVICE_DRT_DIR + 'android_fallback_fonts.xml')
            # Version control of test resources is dependent on executables,
            # because we will always rebuild executables when resources are
            # updated.
            self._push_test_resources()
            self._run_adb_command(['shell', 'echo %d >%s' % (host_stamp, DEVICE_DRT_STAMP_PATH)])

    def _push_fonts(self):
        if not self._check_version(DEVICE_FONTS_DIR, FONT_FILES_VERSION):
            _log.debug('Pushing fonts')
            path_to_ahem_font = self._build_path(self.get_option('configuration'), 'AHEM____.TTF')
            self._push_to_device(path_to_ahem_font, DEVICE_FONTS_DIR + 'AHEM____.TTF')
            for (host_dir, font_file) in HOST_FONT_FILES:
                self._push_to_device(host_dir + font_file, DEVICE_FONTS_DIR + font_file)
            self._link_device_file('/system/fonts/DroidSansFallback.ttf', DEVICE_FONTS_DIR + 'DroidSansFallback.ttf')
            self._update_version(DEVICE_FONTS_DIR, FONT_FILES_VERSION)

    def _push_test_resources(self):
        _log.debug('Pushing test resources')
        for resource in TEST_RESOURCES_TO_PUSH:
            self._push_to_device(self.layout_tests_dir() + '/' + resource, DEVICE_LAYOUT_TESTS_DIR + resource)

    def _synchronize_datetime(self):
        # The date/time between host and device may not be synchronized.
        # We need to make them synchronized, otherwise tests might fail.
        try:
            # Get seconds since 1970-01-01 00:00:00 UTC.
            host_datetime = self._executive.run_command(['date', '-u', '+%s'])
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
            error_handler = self._executive.ignore_error
        else:
            error_handler = None
        result = self._executive.run_command(self._adb_command + cmd, error_handler=error_handler)
        _log.debug('Run adb result:\n' + result)
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
        if not self._original_governor:
            self._original_governor = self._run_adb_command(['shell', 'cat', SCALING_GOVERNOR], ignore_error=True)
            if self._original_governor:
                self._run_adb_command(['shell', 'echo', 'performance', '>', SCALING_GOVERNOR])

    def _teardown_performance(self):
        if self._original_governor:
            self._run_adb_command(['shell', 'echo', self._original_governor, SCALING_GOVERNOR])
        self._original_governor = None


class ChromiumAndroidDriver(chromium.ChromiumDriver):
    # The controller may start multiple drivers during test, but for now we
    # don't support multiple Android activities, so only one driver can be
    # started at a time.
    _started_driver = None

    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        chromium.ChromiumDriver.__init__(self, port, worker_number, pixel_tests, no_timeout)
        self._in_fifo_path = DRT_APP_FILE_DIR + 'DumpRenderTree.in'
        self._out_fifo_path = DRT_APP_FILE_DIR + 'DumpRenderTree.out'
        self._err_file_path = DRT_APP_FILE_DIR + 'DumpRenderTree.err'
        self._restart_after_killed = False
        self._read_fifo_proc = None

    def _command_wrapper(cls, wrapper_option):
        # Ignore command wrapper which is not applicable on Android.
        return []

    def cmd_line(self, pixel_tests, per_test_args):
        original_cmd = chromium.ChromiumDriver.cmd_line(self, pixel_tests, per_test_args)
        cmd = []
        for param in original_cmd:
            if param.startswith('--pixel-tests='):
                self._device_image_path = DRT_APP_FILE_DIR + self._port.host.filesystem.basename(self._image_path)
                param = '--pixel-tests=' + self._device_image_path
            cmd.append(param)

        cmd.append('--in-fifo=' + self._in_fifo_path)
        cmd.append('--out-fifo=' + self._out_fifo_path)
        cmd.append('--err-file=' + self._err_file_path)
        return cmd

    def _file_exists_on_device(self, full_file_path):
        assert full_file_path.startswith('/')
        return self._port._run_adb_command(['shell', 'ls', full_file_path]).strip() == full_file_path

    def _deadlock_detector(self, pids, normal_startup_event):
        time.sleep(DRT_START_STOP_TIMEOUT_SECS)
        if not normal_startup_event.is_set():
            # If normal_startup_event is not set in time, the main thread must be blocked at
            # reading/writing the fifo. Kill the fifo reading/writing processes to let the
            # main thread escape from the deadlocked state. After that, the main thread will
            # treat this as a crash.
            for i in pids:
                self._port._executive.kill_process(i)
        # Otherwise the main thread has been proceeded normally. This thread just exits silently.

    def _start(self, pixel_tests, per_test_args):
        if ChromiumAndroidDriver._started_driver:
            ChromiumAndroidDriver._started_driver.stop()

        ChromiumAndroidDriver._started_driver = self

        retries = 0
        while not self._start_once(pixel_tests, per_test_args):
            _log.error('Failed to start DumpRenderTree application. Log:\n' + self._port._get_logcat())
            retries += 1
            if retries >= 3:
                raise AssertionError('Failed to start DumpRenderTree application multiple times. Give up.')
            self.stop()
            time.sleep(2)

    def _start_once(self, pixel_tests, per_test_args):
        self._port._run_adb_command(['logcat', '-c'])
        self._port._run_adb_command(['shell', 'echo'] + self.cmd_line(pixel_tests, per_test_args) + ['>', COMMAND_LINE_FILE])
        start_result = self._port._run_adb_command(['shell', 'am', 'start', '-n', DRT_ACTIVITY_FULL_NAME])
        if start_result.find('Exception') != -1:
            _log.error('Failed to start DumpRenderTree application. Exception:\n' + start_result)
            return False

        seconds = 0
        while (not self._file_exists_on_device(self._in_fifo_path) or
               not self._file_exists_on_device(self._out_fifo_path) or
               not self._file_exists_on_device(self._err_file_path)):
            time.sleep(1)
            seconds += 1
            if seconds >= DRT_START_STOP_TIMEOUT_SECS:
                return False

        shell_cmd = self._port._adb_command + ['shell']
        executive = self._port._executive
        # Start a process to send command through the input fifo of the DumpRenderTree app.
        # This process must be run as an interactive adb shell because the normal adb shell doesn't support stdin.
        self._proc = executive.popen(shell_cmd, stdin=executive.PIPE, stdout=executive.PIPE, universal_newlines=True)
        # Read back the shell prompt to ensure adb shell ready.
        self._read_prompt()
        _log.debug('Interactive shell started')

        # Start a process to read from the output fifo of the DumpRenderTree app and print to stdout.
        _log.debug('Redirecting stdout to ' + self._out_fifo_path)
        self._read_fifo_proc = executive.popen(shell_cmd + ['cat', self._out_fifo_path],
                                               stdout=executive.PIPE, universal_newlines=True)

        _log.debug('Redirecting stdin to ' + self._in_fifo_path)
        (line, crash) = self._write_command_and_read_line('cat >%s\n' % self._in_fifo_path)

        # Combine the two unidirectional pipes into one bidirectional pipe to make _write_command_and_read_line() etc
        # work with self._proc.
        self._proc.stdout.close()
        self._proc.stdout = self._read_fifo_proc.stdout

        # Start a thread to kill the pipe reading/writing processes on deadlock of the fifos during startup.
        normal_startup_event = threading.Event()
        threading.Thread(target=self._deadlock_detector,
                         args=([self._proc.pid, self._read_fifo_proc.pid], normal_startup_event)).start()

        output = ''
        while not crash and line.rstrip() != '#READY':
            output += line
            (line, crash) = self._write_command_and_read_line()

        if crash:
            # DumpRenderTree crashes during startup, or when the deadlock detector detected
            # deadlock and killed the fifo reading/writing processes.
            _log.error('Failed to start DumpRenderTree: \n%s\nLog:\n%s' % (output, self._port._get_logcat()))
            self.stop()
            raise AssertionError('Failed to start DumpRenderTree application')
        else:
            # Inform the deadlock detector that the startup is successful without deadlock.
            normal_startup_event.set()
            return True

    def run_test(self, driver_input):
        driver_output = chromium.ChromiumDriver.run_test(self, driver_input)
        if driver_output.crash:
            # When Android is OOM, DRT process may be killed by ActivityManager or system OOM.
            # It looks like a crash but there is no fatal signal logged. Re-run the test for
            # such crash.
            # To test: adb shell am force-stop org.chromium.native_test,
            # or kill -11 pid twice or three times to simulate a fatal crash.
            if self._port._get_logcat().find('Fatal signal') == -1:
                self._restart_after_killed = True
                self._port._drt_retry_after_killed += 1
                if self._port._drt_retry_after_killed > 10:
                    raise AssertionError('DumpRenderTree is killed by Android for too many times!')
                _log.error('DumpRenderTree is killed by system (%d).' % self._port._drt_retry_after_killed)
                self.stop()
                # Sleep 10 seconds to let system recover.
                time.sleep(10)
                return self.run_test(driver_input)

        self._restart_after_killed = False
        driver_output.error += self._get_stderr()
        return driver_output

    def stop(self):
        if ChromiumAndroidDriver._started_driver != self:
            return
        ChromiumAndroidDriver._started_driver = None

        self._port._run_adb_command(['shell', 'am', 'force-stop', DRT_APP_PACKAGE])

        if self._read_fifo_proc:
            self._port._executive.kill_process(self._read_fifo_proc.pid)
            self._read_fifo_proc = None

        # Here duplicate some logic in ChromiumDriver.stop() instead of directly calling it,
        # because our pipe reading/writing processes won't quit by itself on close of the pipes.
        if self._proc:
            self._proc.stdin.close()
            self._proc.stdout.close()
            if self._proc.stderr:
                self._proc.stderr.close()
            self._port._executive.kill_process(self._proc.pid)
            if self._proc.poll() is not None:
                self._proc.wait()
            self._proc = None

        seconds = 0
        while (self._file_exists_on_device(self._in_fifo_path) or
               self._file_exists_on_device(self._out_fifo_path) or
               self._file_exists_on_device(self._err_file_path)):
            time.sleep(1)
            self._port._run_adb_command(['shell', 'rm', self._in_fifo_path, self._out_fifo_path, self._err_file_path])
            seconds += 1
            if seconds >= DRT_START_STOP_TIMEOUT_SECS:
                raise AssertionError('Failed to remove fifo files. May be locked.')

    def _test_shell_command(self, uri, timeout_ms, checksum):
        if uri.startswith('file:///'):
            # Convert the host uri to a device uri. See comment of
            # DEVICE_LAYOUT_TESTS_DIR for details.
            # Not overriding Port.filename_to_uri() because we don't want the
            # links in the html report point to device paths.
            uri = FILE_TEST_URI_PREFIX + self.uri_to_test(uri)
        return chromium.ChromiumDriver._test_shell_command(self, uri, timeout_ms, checksum)

    def _write_command_and_read_line(self, input=None):
        (line, crash) = chromium.ChromiumDriver._write_command_and_read_line(self, input)
        url_marker = '#URL:'
        if not crash:
            if line.startswith(url_marker) and line.find(FILE_TEST_URI_PREFIX) == len(url_marker):
                # Convert the device test uri back to host uri otherwise
                # chromium.ChromiumDriver.run_test() will complain.
                line = '#URL:file://%s/%s' % (self._port.layout_tests_dir(), line[len(url_marker) + len(FILE_TEST_URI_PREFIX):])
            # chromium.py uses "line == '' and self._proc.poll() is not None" to detect crash,
            # but on Android "not line" is enough because self._proc.poll() seems not reliable.
            if not line:
                crash = True
        return (line, crash)

    def _output_image(self):
        if self._image_path:
            _log.debug('Pulling from device: %s to %s' % (self._device_image_path, self._image_path))
            self._port._pull_from_device(self._device_image_path, self._image_path, ignore_error=True)
        return chromium.ChromiumDriver._output_image(self)

    def _get_stderr(self):
        return self._port._run_adb_command(['shell', 'cat', self._err_file_path], ignore_error=True)

    def _read_prompt(self):
        last_char = ''
        while True:
            current_char = self._proc.stdout.read(1)
            if current_char == ' ':
                if last_char == '#':
                    return
                if last_char == '$':
                    raise AssertionError('Adbd is not running as root')
            last_char = current_char
