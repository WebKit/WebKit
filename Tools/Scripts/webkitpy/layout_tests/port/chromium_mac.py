#!/usr/bin/env python
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

"""Chromium Mac implementation of the Port interface."""

import logging
import os
import signal

from webkitpy.layout_tests.port import mac
from webkitpy.layout_tests.port import chromium

from webkitpy.common.system.executive import Executive


_log = logging.getLogger(__name__)


class ChromiumMacPort(chromium.ChromiumPort):
    SUPPORTED_OS_VERSIONS = ('leopard', 'snowleopard', 'lion', 'future')

    FALLBACK_PATHS = {
        'leopard': [
            'chromium-mac-leopard',
            'chromium-mac-snowleopard',
            'chromium-mac',
            'chromium',
            'mac-leopard',
            'mac-snowleopard',
            'mac-lion',
            'mac',
        ],
        'snowleopard': [
            'chromium-mac-snowleopard',
            'chromium-mac',
            'chromium',
            'mac-snowleopard',
            'mac-lion',
            'mac',
        ],
        'lion': [
            'chromium-mac',
            'chromium',
            'mac-lion',
            'mac',
        ],
        'future': [
            'chromium-mac',
            'chromium',
            'mac',
        ],
    }

    def __init__(self, host, port_name=None, os_version_string=None, **kwargs):
        # We're a little generic here because this code is reused by the
        # 'google-chrome' port as well as the 'mock-' and 'dryrun-' ports.
        port_name = port_name or 'chromium-mac'
        chromium.ChromiumPort.__init__(self, host, port_name=port_name, **kwargs)
        if port_name.endswith('-mac'):
            self._version = mac.os_version(os_version_string, self.SUPPORTED_OS_VERSIONS)
            self._name = port_name + '-' + self._version
        else:
            self._version = port_name[port_name.index('-mac-') + len('-mac-'):]
            assert self._version in self.SUPPORTED_OS_VERSIONS

    def baseline_search_path(self):
        fallback_paths = self.FALLBACK_PATHS
        return map(self._webkit_baseline_path, fallback_paths[self._version])

    def check_build(self, needs_http):
        result = chromium.ChromiumPort.check_build(self, needs_http)
        result = self.check_wdiff() and result
        if not result:
            _log.error('For complete Mac build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/MacBuildInstructions')

        return result

    def default_child_processes(self):
        if not self._multiprocessing_is_available:
            # Running multiple threads in Mac Python is unstable (See
            # https://bugs.webkit.org/show_bug.cgi?id=38553 for more info).
            return 1
        return chromium.ChromiumPort.default_child_processes(self)

    def operating_system(self):
        return 'mac'

    #
    # PROTECTED METHODS
    #

    def _build_path(self, *comps):
        if self.get_option('build_directory'):
            return self._filesystem.join(self.get_option('build_directory'),
                                         *comps)
        base = self.path_from_chromium_base()
        path = self._filesystem.join(base, 'out', *comps)
        if self._filesystem.exists(path):
            return path

        path = self._filesystem.join(base, 'xcodebuild', *comps)
        if self._filesystem.exists(path):
            return path

        base = self.path_from_webkit_base()
        path = self._filesystem.join(base, 'out', *comps)
        if self._filesystem.exists(path):
            return path

        return self._filesystem.join(base, 'xcodebuild', *comps)

    def check_wdiff(self, logging=True):
        try:
            # We're ignoring the return and always returning True
            self._executive.run_command([self._path_to_wdiff()], error_handler=Executive.ignore_error)
        except OSError:
            if logging:
                _log.warning('wdiff not found. Install using MacPorts or some other means')
        return True

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd', 'mac', *comps)

    def _path_to_apache(self):
        return '/usr/sbin/httpd'

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', 'apache2-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttpd_path('bin', 'lighttpd')

    def _path_to_lighttpd_modules(self):
        return self._lighttpd_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('bin', 'php-cgi')

    def _path_to_driver(self, configuration=None):
        # FIXME: make |configuration| happy with case-sensitive file systems.
        if not configuration:
            configuration = self.get_option('configuration')
        return self._build_path(configuration, self.driver_name() + '.app',
            'Contents', 'MacOS', self.driver_name())

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper'
        return self._build_path(self.get_option('configuration'), binary_name)

    def _path_to_wdiff(self):
        return 'wdiff'
