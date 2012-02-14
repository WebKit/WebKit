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

from webkitpy.layout_tests.port import chromium

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

        self._host_port = host.port_factory.get('chromium', **kwargs)

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
        # FIXME: Not implemented (yet!)
        pass

    def stop_helper(self):
        # FIXME: Not implemented (yet!)
        pass

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
