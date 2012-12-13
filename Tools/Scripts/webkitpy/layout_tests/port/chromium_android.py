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
import sys
import subprocess
import threading
import time

from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import server_process
from webkitpy.common.system.profiler import SingleFileOutputProfiler

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

# Path on the device where the test framework will create the fifo pipes.
DEVICE_FIFO_PATH = '/data/data/org.chromium.native_test/files/'

DRT_APP_PACKAGE = 'org.chromium.native_test'
DRT_ACTIVITY_FULL_NAME = DRT_APP_PACKAGE + '/.ChromeNativeTestActivity'
DRT_APP_CACHE_DIR = DEVICE_DRT_DIR + 'cache/'
DRT_LIBRARY_NAME = 'libDumpRenderTree.so'

SCALING_GOVERNORS_PATTERN = "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"

# All the test cases are still served to DumpRenderTree through file protocol,
# but we use a file-to-http feature to bridge the file request to host's http
# server to get the real test files and corresponding resources.
# See webkit/support/platform_support_android.cc for the other side of this bridge.
PERF_TEST_PATH_PREFIX = '/all-perf-tests'
LAYOUT_TEST_PATH_PREFIX = '/all-tests'

# All ports the Android forwarder to forward.
# 8000, 8080 and 8443 are for http/https tests.
# 8880 and 9323 are for websocket tests
# (see http_server.py, apache_http_server.py and websocket_server.py).
FORWARD_PORTS = '8000 8080 8443 8880 9323'

MS_TRUETYPE_FONTS_DIR = '/usr/share/fonts/truetype/msttcorefonts/'
MS_TRUETYPE_FONTS_PACKAGE = 'ttf-mscorefonts-installer'

# Timeout in seconds to wait for start/stop of DumpRenderTree.
DRT_START_STOP_TIMEOUT_SECS = 10

# List of fonts that layout tests expect, copied from DumpRenderTree/chromium/TestShellX11.cpp.
HOST_FONT_FILES = [
    [[MS_TRUETYPE_FONTS_DIR], 'Arial.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Comic_Sans_MS.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Comic_Sans_MS_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Impact.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    # The Microsoft font EULA
    [['/usr/share/doc/ttf-mscorefonts-installer/'], 'READ_ME!.gz', MS_TRUETYPE_FONTS_PACKAGE],
    # Other fonts: Arabic, CJK, Indic, Thai, etc.
    [['/usr/share/fonts/truetype/ttf-dejavu/'], 'DejaVuSans.ttf', 'ttf-dejavu'],
    [['/usr/share/fonts/truetype/kochi/'], 'kochi-mincho.ttf', 'ttf-kochi-mincho'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'lohit_hi.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'lohit_ta.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'MuktiNarrow.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/thai/', '/usr/share/fonts/truetype/tlwg/'], 'Garuda.ttf', 'fonts-tlwg-garuda'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/', '/usr/share/fonts/truetype/ttf-punjabi-fonts/'], 'lohit_pa.ttf', 'ttf-indic-fonts-core'],
]

DEVICE_FONTS_DIR = DEVICE_DRT_DIR + 'fonts/'

# The layout tests directory on device, which has two usages:
# 1. as a virtual path in file urls that will be bridged to HTTP.
# 2. pointing to some files that are pushed to the device for tests that
# don't work on file-over-http (e.g. blob protocol tests).
DEVICE_WEBKIT_BASE_DIR = DEVICE_SOURCE_ROOT_DIR + 'third_party/WebKit/'
DEVICE_LAYOUT_TESTS_DIR = DEVICE_WEBKIT_BASE_DIR + 'LayoutTests/'

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

MD5SUM_DEVICE_FILE_NAME = 'md5sum_bin'
MD5SUM_DEVICE_PATH = '/data/local/tmp/' + MD5SUM_DEVICE_FILE_NAME

class ChromiumAndroidPort(chromium.ChromiumPort):
    port_name = 'chromium-android'

    # Avoid initializing the adb path [worker count]+1 times by storing it as a static member.
    _adb_path = None

    FALLBACK_PATHS = [
        'chromium-android',
        'chromium-linux',
        'chromium-win',
        'chromium',
    ]

    def __init__(self, host, port_name, **kwargs):
        super(ChromiumAndroidPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)
        self._server_process_constructor = self._android_server_process_constructor

        if hasattr(self._options, 'adb_device'):
            self._devices = self._options.adb_device
        else:
            self._devices = []

    @staticmethod
    def _android_server_process_constructor(port, server_name, cmd_line, env=None):
        return server_process.ServerProcess(port, server_name, cmd_line, env,
                                            universal_newlines=True, treat_no_data_as_crash=True)

    def additional_drt_flag(self):
        # The Chromium port for Android always uses the hardware GPU path.
        return ['--encode-binary', '--enable-hardware-gpu',
                '--force-compositing-mode',
                '--enable-accelerated-fixed-position']

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

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS)

    def check_wdiff(self, logging=True):
        return self._host_port.check_wdiff(logging)

    def check_build(self, needs_http):
        result = super(ChromiumAndroidPort, self).check_build(needs_http)
        result = self._check_file_exists(self.path_to_md5sum(), 'md5sum utility') and result
        result = self._check_file_exists(self.path_to_forwarder(), 'forwarder utility') and result
        if not result:
            _log.error('For complete Android build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/AndroidBuildInstructions')

        return result

    def check_sys_deps(self, needs_http):
        for (font_dirs, font_file, package) in HOST_FONT_FILES:
            exists = False
            for font_dir in font_dirs:
                font_path = font_dir + font_file
                if self._check_file_exists(font_path, '', logging=False):
                    exists = True
                    break
            if not exists:
                _log.error('You are missing %s under %s. Try installing %s. See build instructions.' % (font_file, font_dirs, package))
                return False
        return True

    def expectations_files(self):
        # LayoutTests/platform/chromium-android/TestExpectations should contain only the rules to
        # skip tests for the features not supported or not testable on Android.
        # Other rules should be in LayoutTests/platform/chromium/TestExpectations.
        android_expectations_file = self.path_from_webkit_base('LayoutTests', 'platform', 'chromium-android', 'TestExpectations')
        return super(ChromiumAndroidPort, self).expectations_files() + [android_expectations_file]

    def requires_http_server(self):
        """Chromium Android runs tests on devices, and uses the HTTP server to
        serve the actual layout tests to DumpRenderTree."""
        return True

    def start_http_server(self, additional_dirs=None, number_of_servers=0):
        if not additional_dirs:
            additional_dirs = {}
        additional_dirs[PERF_TEST_PATH_PREFIX] = self.perf_tests_dir()
        additional_dirs[LAYOUT_TEST_PATH_PREFIX] = self.layout_tests_dir()
        super(ChromiumAndroidPort, self).start_http_server(additional_dirs, number_of_servers)

    def create_driver(self, worker_number, no_timeout=False):
        # We don't want the default DriverProxy which is not compatible with our driver.
        # See comments in ChromiumAndroidDriver.start().
        return ChromiumAndroidDriver(self, worker_number, pixel_tests=self.get_option('pixel_tests'),
                                     # Force no timeout to avoid DumpRenderTree timeouts before NRWT.
                                     no_timeout=True)

    def driver_cmd_line(self):
        # Override to return the actual DumpRenderTree command line.
        return self.create_driver(0)._drt_cmd_line(self.get_option('pixel_tests'), [])

    def path_to_adb(self):
        if ChromiumAndroidPort._adb_path:
            return ChromiumAndroidPort._adb_path

        provided_adb_path = self.path_from_chromium_base('third_party', 'android_tools', 'sdk', 'platform-tools', 'adb')

        path_version = self._determine_adb_version('adb')
        provided_version = self._determine_adb_version(provided_adb_path)
        assert provided_version, 'The checked in Android SDK is missing. Are you sure you ran update-webkit --chromium-android?'

        if not path_version:
            ChromiumAndroidPort._adb_path = provided_adb_path
        elif provided_version > path_version:
            _log.warning('The "adb" version in your path is older than the one checked in, consider updating your local Android SDK. Using the checked in one.')
            ChromiumAndroidPort._adb_path = provided_adb_path
        else:
            ChromiumAndroidPort._adb_path = 'adb'

        return ChromiumAndroidPort._adb_path

    def path_to_forwarder(self):
        return self._build_path('forwarder')

    def path_to_md5sum(self):
        return self._build_path(MD5SUM_DEVICE_FILE_NAME)

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

    def _determine_adb_version(self, adb_path):
        re_version = re.compile('^.*version ([\d\.]+)$')
        try:
            output = self._executive.run_command([adb_path, 'version'], error_handler=self._executive.ignore_error)
        except OSError:
            return None
        result = re_version.match(output)
        if not output or not result:
            return None
        return [int(n) for n in result.group(1).split('.')]

    def _get_devices(self):
        if not self._devices:
            re_device = re.compile('^([a-zA-Z0-9_:.-]+)\tdevice$', re.MULTILINE)
            result = self._executive.run_command([self.path_to_adb(), 'devices'], error_handler=self._executive.ignore_error)
            self._devices = re_device.findall(result)
            if not self._devices:
                raise AssertionError('No devices attached. Result of "adb devices": %s' % result)
        return self._devices

    def _get_device_serial(self, worker_number):
        devices = self._get_devices()
        if worker_number >= len(devices):
            raise AssertionError('Worker number exceeds available number of devices')
        return devices[worker_number]


class AndroidPerf(SingleFileOutputProfiler):
    _cached_perf_host_path = None
    _have_searched_for_perf_host = False

    def __init__(self, host, executable_path, output_dir, adb_path, device_serial, symfs_path, identifier=None):
        super(AndroidPerf, self).__init__(host, executable_path, output_dir, "data", identifier)
        self._device_serial = device_serial
        self._adb_command = [adb_path, '-s', self._device_serial]
        self._perf_process = None
        self._symfs_path = symfs_path

    def check_configuration(self):
        # Check that perf is installed
        if not self._file_exists_on_device('/system/bin/perf'):
            print "Cannot find /system/bin/perf on device %s" % self._device_serial
            return False
        # Check that the device is a userdebug build (or at least has the necessary libraries).
        if self._run_adb_command(['shell', 'getprop', 'ro.build.type']).strip() != 'userdebug':
            print "Device %s is not flashed with a userdebug build of Android" % self._device_serial
            return False
        # FIXME: Check that the binary actually is perf-able (has stackframe pointers)?
        # objdump -s a function and make sure it modifies the fp?
        # Instruct users to rebuild after export GYP_DEFINES="profiling=1 $GYP_DEFINES"
        return True

    def print_setup_instructions(self):
        print """
perf on android requires a 'userdebug' build of Android, see:
http://source.android.com/source/building-devices.html"

The perf command can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
and requires libefl, libebl, libdw, and libdwfl available in:
https://android.googlesource.com/platform/external/elfutils/

DumpRenderTree must be built with profiling=1, make sure you've done:
export GYP_DEFINES="profiling=1 $GYP_DEFINES"
update-webkit --chromium-android
build-webkit --chromium-android

Googlers should read:
http://goto.google.com/cr-android-perf-howto
"""

    def _file_exists_on_device(self, full_file_path):
        assert full_file_path.startswith('/')
        return self._run_adb_command(['shell', 'ls', full_file_path]).strip() == full_file_path

    def _run_adb_command(self, cmd):
        return self._host.executive.run_command(self._adb_command + cmd)

    def attach_to_pid(self, pid):
        assert(pid)
        assert(self._perf_process == None)
        # FIXME: This can't be a fixed timeout!
        cmd = self._adb_command + ['shell', 'perf', 'record', '-g', '-p', pid, 'sleep', 30]
        cmd = map(unicode, cmd)
        self._perf_process = self._host.executive.popen(cmd)

    def _perf_version_string(self, perf_path):
        try:
            return self._host.executive.run_command([perf_path, '--version'])
        except:
            return None

    def _find_perfhost_binary(self):
        perfhost_version = self._perf_version_string('perfhost_linux')
        if perfhost_version:
            return 'perfhost_linux'
        perf_version = self._perf_version_string('perf')
        if perf_version:
            return 'perf'
        return None

    def _perfhost_path(self):
        if self._have_searched_for_perf_host:
            return self._cached_perf_host_path
        self._have_searched_for_perf_host = True
        self._cached_perf_host_path = self._find_perfhost_binary()
        return self._cached_perf_host_path

    def _first_ten_lines_of_profile(self, perf_output):
        match = re.search("^#[^\n]*\n((?: [^\n]*\n){1,10})", perf_output, re.MULTILINE)
        return match.group(1) if match else None

    def profile_after_exit(self):
        perf_exitcode = self._perf_process.wait()
        if perf_exitcode != 0:
            print "Perf failed (exit code: %i), can't process results." % perf_exitcode
            return
        self._run_adb_command(['pull', '/data/perf.data', self._output_path])

        perfhost_path = self._perfhost_path()
        if perfhost_path:
            perfhost_args = [perfhost_path, 'report', '-g', 'none', '-i', self._output_path, '--symfs', self._symfs_path]
            perf_output = self._host.executive.run_command(perfhost_args)
            # We could save off the full -g report to a file if users found that useful.
            print self._first_ten_lines_of_profile(perf_output)
        else:
            print """
Failed to find perfhost_linux binary, can't process samples from the device.

perfhost_linux can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
also, modern versions of perf (available from apt-get install goobuntu-kernel-tools-common)
may also be able to process the perf.data files from the device.

Googlers should read:
http://goto.google.com/cr-android-perf-howto
for instructions on installing pre-built copies of perfhost_linux
http://crbug.com/165250 discusses making these pre-built binaries externally available.
"""

        perfhost_display_patch = perfhost_path if perfhost_path else 'perfhost_linux'
        print "To view the full profile, run:"
        print ' '.join([perfhost_display_patch, 'report', '-i', self._output_path, '--symfs', self._symfs_path])


class ChromiumAndroidDriver(driver.Driver):
    def __init__(self, port, worker_number, pixel_tests, no_timeout=False):
        super(ChromiumAndroidDriver, self).__init__(port, worker_number, pixel_tests, no_timeout)
        self._cmd_line = None
        self._in_fifo_path = DEVICE_FIFO_PATH + 'stdin.fifo'
        self._out_fifo_path = DEVICE_FIFO_PATH + 'test.fifo'
        self._err_fifo_path = DEVICE_FIFO_PATH + 'stderr.fifo'
        self._read_stdout_process = None
        self._read_stderr_process = None
        self._forwarder_process = None
        self._has_setup = False
        self._original_governors = {}
        self._device_serial = port._get_device_serial(worker_number)
        self._adb_command_base = None

        # FIXME: If we taught ProfileFactory about "target" devices we could
        # just use the logic in Driver instead of duplicating it here.
        if self._port.get_option("profile"):
            symfs_path = self._find_or_create_symfs()
            # FIXME: We should pass this some sort of "Bridge" object abstraction around ADB instead of a path/device pair.
            self._profiler = AndroidPerf(self._port.host, self._port._path_to_driver(), self._port.results_directory(),
                self._port.path_to_adb(), self._device_serial, symfs_path)\
            # FIXME: This is a layering violation and should be moved to Port.check_sys_deps
            # once we have an abstraction around an adb_path/device_serial pair to make it
            # easy to make these class methods on AndroidPerf.
            if not self._profiler.check_configuration():
                self._profiler.print_setup_instructions()
                sys.exit(1)
        else:
            self._profiler = None

    def __del__(self):
        self._teardown_performance()
        super(ChromiumAndroidDriver, self).__del__()

    def _find_or_create_symfs(self):
        environment = self._port.host.copy_current_environment()
        env = environment.to_dictionary()
        fs = self._port.host.filesystem

        if 'ANDROID_SYMFS' in env:
            symfs_path = env['ANDROID_SYMFS']
        else:
            symfs_path = fs.join(self._port.results_directory(), 'symfs')
            print "ANDROID_SYMFS not set, using %s" % symfs_path

        # find the installed path, and the path of the symboled built library
        # FIXME: We should get the install path from the device!
        symfs_library_path = fs.join(symfs_path, "data/app-lib/%s-1/%s" % (DRT_APP_PACKAGE, DRT_LIBRARY_NAME))
        built_library_path = self._port._build_path('lib', DRT_LIBRARY_NAME)
        assert(fs.exists(built_library_path))

        # FIXME: Ideally we'd check the sha1's first and make a soft-link instead of copying (since we probably never care about windows).
        print "Updating symfs libary (%s) from built copy (%s)" % (symfs_library_path, built_library_path)
        fs.maybe_make_directory(fs.dirname(symfs_library_path))
        fs.copyfile(built_library_path, symfs_library_path)

        return symfs_path

    def _setup_md5sum_and_push_data_if_needed(self):
        self._md5sum_path = self._port.path_to_md5sum()
        if not self._file_exists_on_device(MD5SUM_DEVICE_PATH):
            if not self._push_to_device(self._md5sum_path, MD5SUM_DEVICE_PATH):
                raise AssertionError('Could not push md5sum to device')

        self._push_executable()
        self._push_fonts()
        self._push_test_resources()

    def _setup_test(self):
        if self._has_setup:
            return

        self._restart_adb_as_root()
        self._setup_md5sum_and_push_data_if_needed()
        self._has_setup = True
        self._setup_performance()

        # Required by webkit_support::GetWebKitRootDirFilePath().
        # Other directories will be created automatically by adb push.
        self._run_adb_command(['shell', 'mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'])

        # Allow the DumpRenderTree app to fully access the directory.
        # The native code needs the permission to write temporary files and create pipes here.
        self._run_adb_command(['shell', 'mkdir', '-p', DEVICE_DRT_DIR])
        self._run_adb_command(['shell', 'chmod', '777', DEVICE_DRT_DIR])

        # Delete the disk cache if any to ensure a clean test run.
        # This is like what's done in ChromiumPort.setup_test_run but on the device.
        self._run_adb_command(['shell', 'rm', '-r', DRT_APP_CACHE_DIR])

    def _log_error(self, message):
        _log.error('[%s] %s' % (self._device_serial, message))

    def _log_debug(self, message):
        _log.debug('[%s] %s' % (self._device_serial, message))

    def _abort(self, message):
        raise AssertionError('[%s] %s' % (self._device_serial, message))

    @staticmethod
    def _extract_hashes_from_md5sum_output(md5sum_output):
        assert md5sum_output
        return [line.split('  ')[0] for line in md5sum_output]

    def _push_file_if_needed(self, host_file, device_file):
        assert os.path.exists(host_file)
        device_hashes = self._extract_hashes_from_md5sum_output(
                self._port.host.executive.popen(self._adb_command() + ['shell', MD5SUM_DEVICE_PATH, device_file],
                                                stdout=subprocess.PIPE).stdout)
        host_hashes = self._extract_hashes_from_md5sum_output(
                self._port.host.executive.popen(args=['%s_host' % self._md5sum_path, host_file],
                                                stdout=subprocess.PIPE).stdout)
        if host_hashes and device_hashes == host_hashes:
            return
        self._push_to_device(host_file, device_file)

    def _push_executable(self):
        self._push_file_if_needed(self._port.path_to_forwarder(), DEVICE_FORWARDER_PATH)
        self._push_file_if_needed(self._port._build_path('DumpRenderTree.pak'), DEVICE_DRT_DIR + 'DumpRenderTree.pak')
        self._push_file_if_needed(self._port._build_path('DumpRenderTree_resources'), DEVICE_DRT_DIR + 'DumpRenderTree_resources')
        self._push_file_if_needed(self._port._build_path('android_main_fonts.xml'), DEVICE_DRT_DIR + 'android_main_fonts.xml')
        self._push_file_if_needed(self._port._build_path('android_fallback_fonts.xml'), DEVICE_DRT_DIR + 'android_fallback_fonts.xml')
        self._run_adb_command(['uninstall', DRT_APP_PACKAGE])
        drt_host_path = self._port._path_to_driver()
        install_result = self._run_adb_command(['install', drt_host_path])
        if install_result.find('Success') == -1:
            self._abort('Failed to install %s onto device: %s' % (drt_host_path, install_result))

    def _push_fonts(self):
        self._log_debug('Pushing fonts')
        path_to_ahem_font = self._port._build_path('AHEM____.TTF')
        self._push_file_if_needed(path_to_ahem_font, DEVICE_FONTS_DIR + 'AHEM____.TTF')
        for (host_dirs, font_file, package) in HOST_FONT_FILES:
            for host_dir in host_dirs:
                host_font_path = host_dir + font_file
                if self._port._check_file_exists(host_font_path, '', logging=False):
                    self._push_file_if_needed(host_font_path, DEVICE_FONTS_DIR + font_file)

    def _push_test_resources(self):
        self._log_debug('Pushing test resources')
        for resource in TEST_RESOURCES_TO_PUSH:
            self._push_file_if_needed(self._port.layout_tests_dir() + '/' + resource, DEVICE_LAYOUT_TESTS_DIR + resource)

    def _restart_adb_as_root(self):
        output = self._run_adb_command(['root'])
        if 'adbd is already running as root' in output:
            return
        elif not 'restarting adbd as root' in output:
            self._log_error('Unrecognized output from adb root: %s' % output)

        # Regardless the output, give the device a moment to come back online.
        self._run_adb_command(['wait-for-device'])

    def _run_adb_command(self, cmd, ignore_error=False):
        self._log_debug('Run adb command: ' + str(cmd))
        if ignore_error:
            error_handler = self._port._executive.ignore_error
        else:
            error_handler = None
        result = self._port._executive.run_command(self._adb_command() + cmd, error_handler=error_handler)
        # Limit the length to avoid too verbose output of commands like 'adb logcat' and 'cat /data/tombstones/tombstone01'
        # whose outputs are normally printed in later logs.
        self._log_debug('Run adb result: ' + result[:80])
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
            self._log_error('DRT crashed, but no tombstone found!')
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
        return self._run_adb_command(['logcat', '-d', '-v', 'threadtime'])

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

    def _get_crash_log(self, stdout, stderr, newer_than):
        if not stdout:
            stdout = ''
        stdout += '********* [%s] Logcat:\n%s' % (self._device_serial, self._get_logcat())
        if not stderr:
            stderr = ''
        stderr += '********* [%s] Tombstone file:\n%s' % (self._device_serial, self._get_last_stacktrace())
        return super(ChromiumAndroidDriver, self)._get_crash_log(stdout, stderr, newer_than)

    def cmd_line(self, pixel_tests, per_test_args):
        # The returned command line is used to start _server_process. In our case, it's an interactive 'adb shell'.
        # The command line passed to the DRT process is returned by _drt_cmd_line() instead.
        return self._adb_command() + ['shell']

    def _file_exists_on_device(self, full_file_path):
        assert full_file_path.startswith('/')
        return self._run_adb_command(['shell', 'ls', full_file_path]).strip() == full_file_path

    def _drt_cmd_line(self, pixel_tests, per_test_args):
        return driver.Driver.cmd_line(self, pixel_tests, per_test_args) + ['--create-stdin-fifo', '--separate-stderr-fifo']

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
        for file in [self._in_fifo_path, self._out_fifo_path, self._err_fifo_path]:
            self._run_adb_command(['shell', 'rm', file])

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
            self._log_error('Failed to start DumpRenderTree application. Retries=%d. Log:%s' % (retries, self._get_logcat()))
            self.stop()
            time.sleep(2)
        self._abort('Failed to start DumpRenderTree application multiple times. Give up.')

    def _start_once(self, pixel_tests, per_test_args):
        super(ChromiumAndroidDriver, self)._start(pixel_tests, per_test_args)

        self._log_debug('Starting forwarder')
        self._forwarder_process = self._port._server_process_constructor(
            self._port, 'Forwarder', self._adb_command() + ['shell', '%s -D %s' % (DEVICE_FORWARDER_PATH, FORWARD_PORTS)])
        self._forwarder_process.start()

        self._run_adb_command(['logcat', '-c'])
        self._run_adb_command(['shell', 'echo'] + self._cmd_line + ['>', COMMAND_LINE_FILE])
        start_result = self._run_adb_command(['shell', 'am', 'start', '-e', 'RunInSubThread', '-n', DRT_ACTIVITY_FULL_NAME])
        if start_result.find('Exception') != -1:
            self._log_error('Failed to start DumpRenderTree application. Exception:\n' + start_result)
            return False

        if not ChromiumAndroidDriver._loop_with_timeout(self._all_pipes_created, DRT_START_STOP_TIMEOUT_SECS):
            return False

        # Read back the shell prompt to ensure adb shell ready.
        deadline = time.time() + DRT_START_STOP_TIMEOUT_SECS
        self._server_process.start()
        self._read_prompt(deadline)
        self._log_debug('Interactive shell started')

        # Start a process to read from the stdout fifo of the DumpRenderTree app and print to stdout.
        self._log_debug('Redirecting stdout to ' + self._out_fifo_path)
        self._read_stdout_process = self._port._server_process_constructor(
            self._port, 'ReadStdout', self._adb_command() + ['shell', 'cat', self._out_fifo_path])
        self._read_stdout_process.start()

        # Start a process to read from the stderr fifo of the DumpRenderTree app and print to stdout.
        self._log_debug('Redirecting stderr to ' + self._err_fifo_path)
        self._read_stderr_process = self._port._server_process_constructor(
            self._port, 'ReadStderr', self._adb_command() + ['shell', 'cat', self._err_fifo_path])
        self._read_stderr_process.start()

        self._log_debug('Redirecting stdin to ' + self._in_fifo_path)
        self._server_process.write('cat >%s\n' % self._in_fifo_path)

        # Combine the stdout and stderr pipes into self._server_process.
        self._server_process.replace_outputs(self._read_stdout_process._proc.stdout, self._read_stderr_process._proc.stdout)

        def deadlock_detector(processes, normal_startup_event):
            if not ChromiumAndroidDriver._loop_with_timeout(lambda: normal_startup_event.is_set(), DRT_START_STOP_TIMEOUT_SECS):
                # If normal_startup_event is not set in time, the main thread must be blocked at
                # reading/writing the fifo. Kill the fifo reading/writing processes to let the
                # main thread escape from the deadlocked state. After that, the main thread will
                # treat this as a crash.
                self._log_error('Deadlock detected. Processes killed.')
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

        # Inform the deadlock detector that the startup is successful without deadlock.
        normal_startup_event.set()
        return True

    def _pid_from_android_ps_output(self, ps_output, package_name):
        # ps output seems to be fixed width, we only care about the name and the pid
        # u0_a72    21630 125   947920 59364 ffffffff 400beee4 S org.chromium.native_test
        for line in ps_output.split('\n'):
            if line.find(DRT_APP_PACKAGE) != -1:
                match = re.match(r'\S+\s+(\d+)', line)
                return int(match.group(1))

    def _pid_on_target(self):
        # FIXME: There must be a better way to do this than grepping ps output!
        ps_output = self._run_adb_command(['shell', 'ps'])
        return self._pid_from_android_ps_output(ps_output, DRT_APP_PACKAGE)

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

        if self._has_setup:
            if not ChromiumAndroidDriver._loop_with_timeout(self._remove_all_pipes, DRT_START_STOP_TIMEOUT_SECS):
                raise AssertionError('Failed to remove fifo files. May be locked.')

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumAndroidDriver, self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            fs = self._port._filesystem
            # FIXME: what happens if command lies outside of the layout_tests_dir on the host?
            relative_test_filename = fs.relpath(command, fs.dirname(self._port.layout_tests_dir()))
            command = DEVICE_WEBKIT_BASE_DIR + relative_test_filename
        return command

    def _read_prompt(self, deadline):
        last_char = ''
        while True:
            current_char = self._server_process.read_stdout(deadline, 1)
            if current_char == ' ':
                if last_char in ('#', '$'):
                    return
            last_char = current_char

    def _adb_command(self):
        if not self._adb_command_base:
            self._adb_command_base = [self._port.path_to_adb(), '-s', self._device_serial]
        return self._adb_command_base
