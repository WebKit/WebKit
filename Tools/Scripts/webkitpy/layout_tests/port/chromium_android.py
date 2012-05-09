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
import re
import signal
import time

from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import factory


_log = logging.getLogger(__name__)


# The root directory for test resources, which has the same structure as the
# source root directory of Chromium.
# This path is defined in base/base_paths_android.cc and
# webkit/support/platform_support_android.cc.
DEVICE_SOURCE_ROOT_DIR = '/data/local/tmp/'

DEVICE_DRT_DIR = '/data/drt/'
DEVICE_DRT_PATH = DEVICE_DRT_DIR + 'DumpRenderTree'
DEVICE_DRT_STDERR = DEVICE_DRT_DIR + 'DumpRenderTree.stderr'
DEVICE_FORWARDER_PATH = DEVICE_DRT_DIR + 'forwarder'
DEVICE_DRT_STAMP_PATH = DEVICE_DRT_DIR + 'DumpRenderTree.stamp'

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

# List of fonts that layout tests expect, copied from DumpRenderTree/gtk/TestShellGtk.cpp.
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
]
# Should increase this version after changing HOST_FONT_FILES.
FONT_FILES_VERSION = 1

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

        # The Chromium port for Android always uses the hardware GPU path.
        self._options.enable_hardware_gpu = True

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'
        self._original_governor = None
        self._android_base_dir = None

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)

        self._adb_command = ['adb']
        adb_args = self.get_option('adb_args')
        if adb_args:
            self._adb_command += shlex.split(adb_args)
        self._drt_retry_after_killed = 0

    def default_test_timeout_ms(self):
        # Android platform has less computing power than desktop platforms.
        # Using 10 seconds allows us to pass most slow tests which are not
        # marked as slow tests on desktop platforms.
        return 10 * 1000

    def default_child_processes(self):
        # Currently we only use one process, but it might be helpful to use
        # more that one process in the future to improve performance.
        return 1

    def baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS)

    def check_build(self, needs_http):
        return self._host_port.check_build(needs_http)

    def check_sys_deps(self, needs_http):
        for (font_dir, font_file) in HOST_FONT_FILES:
            font_path = font_dir + font_file
            if not self._check_file_exists(font_path, 'font file'):
                _log.error('You are missing %s. Try installing msttcorefonts. '
                           'See build instructions.' % font_path)
                return False
        return True

    def test_expectations(self):
        # Automatically apply all expectation rules of chromium-linux to
        # chromium-android.
        # FIXME: This is a temporary measure to reduce the manual work when
        # updating WebKit. This method should be removed when we merge
        # test_expectations_android.txt into test_expectations.txt.
        expectations = chromium.ChromiumPort.test_expectations(self)
        return expectations.replace('LINUX ', 'LINUX ANDROID ')

    def start_http_server(self, additional_dirs=None):
        # The http server runs during the whole testing period, so ignore this call.
        pass

    def stop_http_server(self):
        # Same as start_http_server().
        pass

    def start_helper(self):
        self._setup_performance()
        # Required by webkit_support::GetWebKitRootDirFilePath().
        # Other directories will be created automatically by adb push.
        self._run_adb_command(['shell', 'mkdir', '-p',
                               DEVICE_SOURCE_ROOT_DIR + 'chrome'])

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

    def skipped_tests(self, test_list):
        return base.Port._real_tests(self, [
            # Canvas tests are run as virtual gpu tests.
            'fast/canvas',
            'canvas/philip',
        ])

    def _build_path(self, *comps):
        return self._host_port._build_path(*comps)

    def _path_to_apache(self):
        return self._host_port._path_to_apache()

    def _path_to_apache_config_file(self):
        return self._host_port._path_to_apache_config_file()

    def _path_to_driver(self, configuration=None):
        # Returns the host path to driver which will be pushed to the device.
        if not configuration:
            configuration = self.get_option('configuration')
        return self._build_path(configuration, 'DumpRenderTree')

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

    def _push_executable(self):
        drt_host_path = self._path_to_driver()
        forwarder_host_path = self._path_to_helper()
        drt_jar_host_path = drt_host_path + '.jar'
        host_stamp = int(float(max(os.stat(drt_host_path).st_mtime,
                                   os.stat(forwarder_host_path).st_mtime,
                                   os.stat(drt_jar_host_path).st_mtime)))
        device_stamp = int(float(self._run_adb_command([
            'shell', 'cat %s 2>/dev/null || echo 0' % DEVICE_DRT_STAMP_PATH])))
        if device_stamp < host_stamp:
            _log.debug('Pushing executable')
            self._kill_device_process(DEVICE_FORWARDER_PATH)
            self._push_to_device(forwarder_host_path, DEVICE_FORWARDER_PATH)
            self._push_to_device(drt_host_path, DEVICE_DRT_PATH)
            self._push_to_device(drt_host_path + '.pak', DEVICE_DRT_PATH + '.pak')
            self._push_to_device(drt_host_path + '_resources', DEVICE_DRT_PATH + '_resources')
            self._push_to_device(drt_jar_host_path, DEVICE_DRT_PATH + '.jar')
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
        if ignore_error:
            error_handler = self._executive.ignore_error
        else:
            error_handler = None
        return self._executive.run_command(self._adb_command + cmd, error_handler=error_handler)

    def _copy_device_file(self, from_file, to_file, ignore_error=False):
        # 'cp' is unavailable on Android, so use 'dd' instead.
        return self._run_adb_command(['shell', 'dd', 'if=' + from_file, 'of=' + to_file], ignore_error)

    def _push_to_device(self, host_path, device_path, ignore_error=False):
        return self._run_adb_command(['push', host_path, device_path], ignore_error)

    def _pull_from_device(self, device_path, host_path, ignore_error=False):
        return self._run_adb_command(['pull', device_path, host_path], ignore_error)

    def _kill_device_process(self, name):
        ps_result = self._run_adb_command(['shell', 'ps']).split('\n')
        for line in ps_result:
            if line.find(name) > 0:
                pid = line.split()[1]
                self._run_adb_command(['shell', 'kill', pid])

    def get_stderr(self):
        return self._run_adb_command(['shell', 'cat', DEVICE_DRT_STDERR], ignore_error=True)

    def get_last_stacktrace(self):
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
        return self._run_adb_command(['shell', 'cat', '/data/tombstones/' + last_tombstone[6]])

    def _setup_performance(self):
        # Disable CPU scaling and drop ram cache to reduce noise in tests
        if not self._original_governor:
            self._original_governor = self._run_adb_command(['shell', 'cat', SCALING_GOVERNOR])
            self._run_adb_command(['shell', 'echo', 'performance', '>', SCALING_GOVERNOR])

    def _teardown_performance(self):
        if self._original_governor:
            self._run_adb_command(['shell', 'echo', self._original_governor, SCALING_GOVERNOR])
        self._original_governor = None


class ChromiumAndroidDriver(chromium.ChromiumDriver):
    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        chromium.ChromiumDriver.__init__(self, port, worker_number, pixel_tests, no_timeout)
        self._device_image_path = None
        self._drt_return_parser = re.compile('#DRT_RETURN (\d+)')

    def _start(self, pixel_tests, per_test_args):
        # Convert the original command line into to two parts:
        # - the 'adb shell' command line to start an interactive adb shell;
        # - the DumpRenderTree command line to send to the adb shell.
        original_cmd = self.cmd_line(pixel_tests, per_test_args)
        shell_cmd = []
        drt_args = []
        path_to_driver = self._port._path_to_driver()
        reading_args_before_driver = True
        for param in original_cmd:
            if reading_args_before_driver:
                if param == path_to_driver:
                    reading_args_before_driver = False
                else:
                    shell_cmd.append(param)
            else:
                if param.startswith('--pixel-tests='):
                    if not self._device_image_path:
                        self._device_image_path = DEVICE_DRT_DIR + self._port.host.filesystem.basename(self._image_path)
                    param = '--pixel-tests=' + self._device_image_path
                drt_args.append(param)

        shell_cmd += self._port._adb_command
        shell_cmd.append('shell')
        retries = 0
        while True:
            _log.debug('Starting adb shell for DumpRenderTree: ' + ' '.join(shell_cmd))
            executive = self._port.host.executive
            self._proc = executive.popen(shell_cmd, stdin=executive.PIPE, stdout=executive.PIPE, stderr=executive.STDOUT,
                                         close_fds=True, universal_newlines=True)
            # Read back the shell prompt to ensure adb shell ready.
            self._read_prompt()
            # Some tests rely on this to produce proper number format etc.,
            # e.g. fast/speech/input-appearance-numberandspeech.html.
            self._write_command_and_read_line("export LC_CTYPE='en_US'\n")
            self._write_command_and_read_line("export CLASSPATH='/data/drt/DumpRenderTree.jar'\n")

            # When DumpRenderTree crashes, the Android debuggerd will stop the
            # process before dumping stack to log/tombstone file and terminating
            # the process. Sleep 1 second (long enough for debuggerd to dump
            # stack) before exiting the shell to ensure the process has quit,
            # otherwise the exit will fail because "You have stopped jobs".
            drt_cmd = '%s %s 2>%s;echo "#DRT_RETURN $?";sleep 1;exit\n' % (DEVICE_DRT_PATH, ' '.join(drt_args), DEVICE_DRT_STDERR)
            _log.debug('Starting DumpRenderTree: ' + drt_cmd)

            # Wait until DRT echos '#READY'.
            output = ''
            (line, crash) = self._write_command_and_read_line(drt_cmd)
            while not crash and line.rstrip() != '#READY':
                if line == '':  # EOF or crashed
                    crash = True
                else:
                    output += line
                    (line, crash) = self._write_command_and_read_line()

            if crash:
                # Sometimes the device is in unstable state (may be out of
                # memory?) and kills DumpRenderTree just after it is started.
                # Try to stop and start it again.
                _log.error('Failed to start DumpRenderTree: \n%s\n%s\n' % (output, self._port.get_stderr()))
                self.stop()
                retries += 1
                if retries > 2:
                    raise AssertionError('Failed multiple times to start DumpRenderTree')
            else:
                return

    def run_test(self, driver_input):
        driver_output = chromium.ChromiumDriver.run_test(self, driver_input)

        drt_return = self._get_drt_return_value(driver_output.error)
        if drt_return is not None:
            _log.debug('DumpRenderTree return value: %d' % drt_return)
        # FIXME: Retrieve stderr from the target.
        if driver_output.crash:
            # When Android is OOM, it sends a SIGKILL signal to DRT. DRT
            # is stopped silently and regarded as crashed. Re-run the test for
            # such crash.
            if drt_return == 128 + signal.SIGKILL:
                self._port._drt_retry_after_killed += 1
                if self._port._drt_retry_after_killed > 10:
                    raise AssertionError('DumpRenderTree is killed by Android for too many times!')
                _log.error('DumpRenderTree is killed by SIGKILL. Retry the test (%d).' % self._port._drt_retry_after_killed)
                self.stop()
                # Sleep 10 seconds to let system recover.
                time.sleep(10)
                return self.run_test(driver_input)
            # Fetch the stack trace from the tombstone file.
            # FIXME: sometimes the crash doesn't really happen so that no
            # tombstone is generated. In that case we fetch the wrong stack
            # trace.
            driver_output.error += self._port.get_last_stacktrace().encode('ascii', 'ignore')
            driver_output.error += self._port._run_adb_command(['logcat', '-d']).encode('ascii', 'ignore')
        return driver_output

    def stop(self):
        _log.debug('Stopping DumpRenderTree')
        if self._proc:
            # Send an explicit QUIT command because closing the pipe can't let
            # DumpRenderTree on Android quit immediately.
            try:
                self._proc.stdin.write('QUIT\n')
            except IOError:
                # The pipe has already been closed, indicating abnormal
                # situation occurred. Wait a while to allow the device to
                # recover. *fingers crossed*
                time.sleep(1)
        chromium.ChromiumDriver.stop(self)

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
        if not crash and line.startswith(url_marker) and line.find(FILE_TEST_URI_PREFIX) == len(url_marker):
            # Convert the device test uri back to host uri otherwise
            # chromium.ChromiumDriver.run_test() will complain.
            line = '#URL:file://%s/%s' % (self._port.layout_tests_dir(), line[len(url_marker) + len(FILE_TEST_URI_PREFIX):])
        if not crash and self._has_crash_hint(line):
            crash = True
        return (line, crash)

    def _output_image(self):
        if self._image_path:
            _log.debug('pulling from device: %s to %s' % (self._device_image_path, self._image_path))
            self._port._pull_from_device(self._device_image_path, self._image_path, ignore_error=True)
        return chromium.ChromiumDriver._output_image(self)

    def _has_crash_hint(self, line):
        # When DRT crashes, it sends a signal to Android Debuggerd, like
        # SIGSEGV, SIGFPE, etc. When Debuggerd receives the signal, it stops DRT
        # (which causes Shell to output a message), and dumps the stack strace.
        # We use the Shell output as a crash hint.
        return line is not None and line.find('[1] + Stopped (signal)') >= 0

    def _get_drt_return_value(self, error):
        return_match = self._drt_return_parser.search(error)
        return None if (return_match is None) else int(return_match.group(1))

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
