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

from webkitpy.common.system.executive import ScriptError
from webkitpy.layout_tests.port.apple import ApplePort


_log = logging.getLogger(__name__)


class WinPort(ApplePort):
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

    def _detect_version(self, os_version_string=None, run_on_non_windows_platforms=None):
        # FIXME: os_version_string is for unit testing, but may eventually be provided by factory.py instead.
        if os_version_string is not None:
            return os_version_string

        # No sense in trying to detect our windows version on non-windows platforms, unless we're unittesting.
        if sys.platform != 'cygwin' and not run_on_non_windows_platforms:
            return None

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

    def __init__(self, host, **kwargs):
        self._operating_system = 'win'
        ApplePort.__init__(self, host, **kwargs)

    def compare_text(self, expected_text, actual_text):
        # Sanity was restored in WK2, so we don't need this hack there.
        if self.get_option('webkit_test_runner'):
            return ApplePort.compare_text(self, expected_text, actual_text)

        # This is a hack (which dates back to ORWT).
        # Windows does not have an EDITING DELEGATE, so we strip any EDITING DELEGATE
        # messages to make more of the tests pass.
        # It's possible more of the ports might want this and this could move down into WebKitPort.
        delegate_regexp = re.compile("^EDITING DELEGATE: .*?\n", re.MULTILINE)
        expected_text = delegate_regexp.sub("", expected_text)
        actual_text = delegate_regexp.sub("", actual_text)
        return expected_text != actual_text

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
        fallback_names.extend(['mac-lion', 'mac'])
        return map(self._webkit_baseline_path, fallback_names)

    # This port may need to override setup_environ_for_server
    # to match behavior of setPathForRunningWebKitApp from ORWT.
    # $env->{PATH} = join(':', productDir(), dirname(installedSafariPath()), appleApplicationSupportPath(), $env->{PATH} || "");

    # FIXME: webkitperl/httpd.pm installs /usr/lib/apache/libphp4.dll on cycwin automatically
    # as part of running old-run-webkit-tests.  That's bad design, but we may need some similar hack.
    # We might use setup_environ_for_server for such a hack (or modify apache_http_server.py).
