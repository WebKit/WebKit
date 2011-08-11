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

import logging
import re
import sys

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.port.webkit import WebKitPort


_log = logging.getLogger(__name__)


class WinPort(WebKitPort):
    port_name = "win"

    # This is a list of all supported OS-VERSION pairs for the AppleWin port
    # and the order of fallback between them.  Matches ORWT.
    VERSION_FALLBACK_ORDER = ["win-xp", "win-vista", "win-7sp0", "win"]

    def _version_string_from_windows_version_tuple(self, windows_version_tuple):
        if windows_version_tuple[:3] == (6, 1, 7600):
            return '7sp0'
        if windows_version_tuple[:2] == (6, 0):
            return 'vista'
        if windows_version_tuple[:2] == (5, 1):
            return 'xp'
        return None

    def _detect_version(self):
        # Note, this intentionally returns None to mean that it can't detect what the current version is.
        # Callers can then decide what version they want to pretend to be.
        try:
            ver_output = self._executive.run_command(['cmd', '/c', 'ver'])
        except (ScriptError, OSError), e:
            ver_output = ""
            _log.error("Failed to detect Windows version, assuming latest.\n%s" % e)
        match_object = re.search(r'(?P<major>\d)\.(?P<minor>\d)\.(?P<build>\d+)', ver_output)
        if match_object:
            version_tuple = tuple(map(int, match_object.groups()))
            return self._version_string_from_windows_version_tuple(version_tuple)

    def __init__(self, os_version_string=None, **kwargs):
        # FIXME: This will not create a properly versioned WinPort object when instantiated from a buildbot-name, like win-xp.
        # We'll need to add port_name parsing of some kind (either here, or in factory.py).
        WebKitPort.__init__(self, **kwargs)
        self._version = os_version_string or self._detect_version() or 'future'  # FIXME: This is a hack, as TestConfiguration assumes that this value is never None even though the base "win" port has no "version".
        self._operating_system = 'win'

    # FIXME: A more sophisitcated version of this function should move to WebKitPort and replace all calls to name().
    def _port_name_with_version(self):
        components = [self.port_name]
        if self._version != 'future':  # FIXME: This is a hack, but TestConfiguration doesn't like self._version ever being None.
            components.append(self._version)
        return '-'.join(components)

    def baseline_search_path(self):
        try:
            fallback_index = self.VERSION_FALLBACK_ORDER.index(self._port_name_with_version())
            fallback_names = list(self.VERSION_FALLBACK_ORDER[fallback_index:])
        except ValueError:
            # Unknown versions just fall back to the base port results.
            fallback_names = [self.port_name]
        # FIXME: The AppleWin port falls back to AppleMac for some results.  Eventually we'll have a shared 'apple' port.
        if self.get_option('webkit_test_runner'):
            fallback_names.insert(0, 'win-wk2')
            fallback_names.append('mac-wk2')
            # Note we do not add 'wk2' here, even though it's included in _skipped_search_paths().
        # FIXME: Perhaps we should get this list from MacPort?
        fallback_names.extend(['mac-snowleopard', 'mac'])
        return map(self._webkit_baseline_path, fallback_names)

    def _generate_all_test_configurations(self):
        configurations = []
        for version in self.VERSION_FALLBACK_ORDER:
            version = version.replace('win-', '')
            if version == 'win':  # It's unclear what the "version" for 'win' is?
                continue
            for build_type in self.ALL_BUILD_TYPES:
                configurations.append(TestConfiguration(version=self._version, architecture='x86', build_type=build_type, graphics_type='cpu'))
        return configurations

    # FIXME: webkitperl/httpd.pm installs /usr/lib/apache/libphp4.dll on cycwin automatically
    # as part of running old-run-webkit-tests.  That's bad design, but we may need some similar hack.
    # We might use setup_environ_for_server for such a hack (or modify apache_http_server.py).
