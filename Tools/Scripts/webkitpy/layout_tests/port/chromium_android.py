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
    # The Microsoft font EULA
    ['/usr/share/doc/ttf-mscorefonts-installer/', 'READ_ME!.gz'],
    ['/usr/share/fonts/truetype/ttf-dejavu/', 'DejaVuSans.ttf'],
]
# Should increase this version after changing HOST_FONT_FILES.
FONT_FILES_VERSION = 1

DEVICE_FONTS_DIR = DEVICE_DRT_DIR + 'fonts/'
DEVICE_FIRST_FALLBACK_FONT = '/system/fonts/DroidNaskh-Regular.ttf'

# The layout tests directory on device, which has two usages:
# 1. as a virtual path in file urls that will be bridged to HTTP.
# 2. pointing to some files that are pushed to the device for tests that
# don't work on file-over-http (e.g. blob protocol tests).
DEVICE_LAYOUT_TESTS_DIR = (DEVICE_SOURCE_ROOT_DIR + 'third_party/WebKit/LayoutTests/')

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

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'
        # TODO: we may support other architectures in the future.
        self._architecture = 'arm'
        self._original_governor = None
        self._android_base_dir = None

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)

        self._adb_command = ['adb']
        adb_args = self.get_option('adb_args')
        if adb_args:
            self._adb_command += shlex.split(adb_args)

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

    def default_worker_model(self):
        return 'inline'

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
        self._setup_system_font_for_test()

    def stop_helper(self):
        self._restore_system_font()
        # Leave the forwarder and tests httpd server there because they are
        # useful for debugging and do no harm to subsequent tests.
        self._teardown_performance()

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

    def _setup_system_font_for_test(self):
        # The DejaVu font implicitly used by some CSS 2.1 tests should be added
        # into the font fallback list of the system. DroidNaskh-Regular.ttf is
        # the first font in Android Skia's font fallback list. Fortunately the
        # DejaVu font also contains Naskh glyphs.
        # First remount /system in read/write mode.
        self._run_adb_command(['remount'])
        self._copy_device_file(DEVICE_FONTS_DIR + 'DejaVuSans.ttf', DEVICE_FIRST_FALLBACK_FONT)

    def _restore_system_font(self):
        # First remount /system in read/write mode.
        self._run_adb_command(['remount'])
        self._push_to_device(os.environ['OUT'] + DEVICE_FIRST_FALLBACK_FONT, DEVICE_FIRST_FALLBACK_FONT)

    def _push_test_resources(self):
        _log.debug('Pushing test resources')
        for resource in TEST_RESOURCES_TO_PUSH:
            self._push_to_device(self.layout_tests_dir() + '/' + resource, DEVICE_LAYOUT_TESTS_DIR + resource)

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

    def _setup_performance(self):
        # Disable CPU scaling and drop ram cache to reduce noise in tests
        if not self._original_governor:
            self._original_governor = self._run_adb_command(['shell', 'cat', SCALING_GOVERNOR])
            self._run_adb_command(['shell', 'echo', 'performance', '>', SCALING_GOVERNOR])

    def _teardown_performance(self):
        if self._original_governor:
            self._run_adb_command(['shell', 'echo', self._original_governor, SCALING_GOVERNOR])
        self._original_governor = None
