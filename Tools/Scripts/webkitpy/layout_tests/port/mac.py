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
#     * Neither the Google name nor the names of its
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

"""WebKit Mac implementation of the Port interface."""

import logging
import platform
import re

from webkitpy.layout_tests.port.webkit import WebKitPort


_log = logging.getLogger(__name__)


def os_version(os_version_string=None, supported_versions=None):
    if not os_version_string:
        if hasattr(platform, 'mac_ver') and platform.mac_ver()[0]:
            os_version_string = platform.mac_ver()[0]
        else:
            # Make up something for testing.
            os_version_string = "10.5.6"
    release_version = int(os_version_string.split('.')[1])
    version_strings = {
        5: 'leopard',
        6: 'snowleopard',
        # Add 7: 'lion' here?
    }
    assert release_version >= min(version_strings.keys())
    version_string = version_strings.get(release_version, 'future')
    if supported_versions:
        assert version_string in supported_versions
    return version_string


class MacPort(WebKitPort):
    port_name = "mac"

    # FIXME: 'wk2' probably shouldn't be a version, it should probably be
    # a modifier, like 'chromium-gpu' is to 'chromium'.
    SUPPORTED_VERSIONS = ('leopard', 'snowleopard', 'future', 'wk2')

    FALLBACK_PATHS = {
        'leopard': [
            'mac-leopard',
            'mac-snowleopard',
            'mac',
        ],
        'snowleopard': [
            'mac-snowleopard',
            'mac',
        ],
        'future': [
            'mac',
        ],
        'wk2': [],  # wk2 does not make sense as a version, this is only here to make the rebaseline unit tests not crash.
    }

    def __init__(self, port_name=None, os_version_string=None, **kwargs):
        port_name = port_name or 'mac'
        WebKitPort.__init__(self, port_name=port_name, **kwargs)
        if port_name == 'mac':
            self._version = os_version(os_version_string)
            self._name = port_name + '-' + self._version
        else:
            assert port_name.startswith('mac')
            self._version = port_name[len('mac-'):]
            assert self._version in self.SUPPORTED_VERSIONS, "%s is not in %s" % (self._version, self.SUPPORTED_VERSIONS)
        self._operating_system = 'mac'

    def baseline_search_path(self):
        search_paths = self.FALLBACK_PATHS[self._version]
        if self.get_option('webkit_test_runner'):
            search_paths.insert(0, self._wk2_port_name())
        return map(self._webkit_baseline_path, search_paths)

    def is_crash_reporter(self, process_name):
        return re.search(r'ReportCrash', process_name)

    def _build_java_test_support(self):
        java_tests_path = self._filesystem.join(self.layout_tests_dir(), "java")
        build_java = ["/usr/bin/make", "-C", java_tests_path]
        if self._executive.run_command(build_java, return_exit_code=True):
            _log.error("Failed to build Java support files: %s" % build_java)
            return False
        return True

    def _check_port_build(self):
        return self._build_java_test_support()

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')
