# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Chromium Win implementation of the Port interface."""

import os
import logging

import chromium


_log = logging.getLogger(__name__)


class ChromiumWinPort(chromium.ChromiumPort):
    port_name = 'chromium-win'

    # FIXME: Figure out how to unify this with base.TestConfiguration.all_systems()?
    SUPPORTED_VERSIONS = ('xp', 'win7')

    FALLBACK_PATHS = {
        'xp': [
            'chromium-win-xp',
            'chromium-win',
            'chromium',
        ],
        'win7': [
            'chromium-win',
            'chromium',
        ],
    }

    DEFAULT_BUILD_DIRECTORIES = ('build', 'out')

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('-win'):
            assert host.platform.is_win()
            # We don't maintain separate baselines for vista, so we pretend it is win7.
            if host.platform.os_version in ('vista', '7sp0', '7sp1', 'future'):
                version = 'win7'
            else:
                version = host.platform.os_version
            port_name = port_name + '-' + version
        return port_name

    def __init__(self, host, port_name, **kwargs):
        chromium.ChromiumPort.__init__(self, host, port_name, **kwargs)
        self._version = port_name[port_name.index('chromium-win-') + len('chromium-win-'):]
        assert self._version in self.SUPPORTED_VERSIONS, "%s is not in %s" % (self._version, self.SUPPORTED_VERSIONS)

    def setup_environ_for_server(self, server_name=None):
        env = chromium.ChromiumPort.setup_environ_for_server(self, server_name)

        # FIXME: lighttpd depends on some environment variable we're not whitelisting.
        # We should add the variable to an explicit whitelist in base.Port.
        # FIXME: This is a temporary hack to get the cr-win bot online until
        # someone from the cr-win port can take a look.
        for key, value in os.environ.items():
            if key not in env:
                env[key] = value

        # Put the cygwin directory first in the path to find cygwin1.dll.
        env["PATH"] = "%s;%s" % (self.path_from_chromium_base("third_party", "cygwin", "bin"), env["PATH"])
        # Configure the cygwin directory so that pywebsocket finds proper
        # python executable to run cgi program.
        env["CYGWIN_PATH"] = self.path_from_chromium_base("third_party", "cygwin", "bin")
        if self.get_option('register_cygwin'):
            setup_mount = self.path_from_chromium_base("third_party", "cygwin", "setup_mount.bat")
            self._executive.run_command([setup_mount])  # Paths are all absolute, so this does not require a cwd.
        return env

    def _modules_to_search_for_symbols(self):
        # FIXME: we should return the path to the ffmpeg equivalents to detect if we have the mp3 and aac codecs installed.
        # See https://bugs.webkit.org/show_bug.cgi?id=89706.
        return []

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
        if not result:
            _log.error('For complete Windows build requirements, please see:')
            _log.error('')
            _log.error('    http://dev.chromium.org/developers/how-tos/build-instructions-windows')
        return result

    def operating_system(self):
        return 'win'

    def relative_test_filename(self, filename):
        path = filename[len(self.layout_tests_dir()) + 1:]
        return path.replace('\\', '/')

    #
    # PROTECTED ROUTINES
    #

    def _uses_apache(self):
        return False

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd', 'win', *comps)

    def _path_to_apache(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'usr', 'sbin', 'httpd')

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', 'cygwin-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttpd_path('LightTPD.exe')

    def _path_to_lighttpd_modules(self):
        return self._lighttpd_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('php5', 'php-cgi.exe')

    def _path_to_driver(self, configuration=None):
        binary_name = '%s.exe' % self.driver_name()
        return self._build_path_with_configuration(configuration, binary_name)

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper.exe'
        return self._build_path(binary_name)

    def _path_to_image_diff(self):
        binary_name = 'ImageDiff.exe'
        return self._build_path(binary_name)

    def _path_to_wdiff(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'bin', 'wdiff.exe')
