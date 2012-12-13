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

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.system.executive import ScriptError, Executive
from webkitpy.common.system.path import abspath_to_uri
from webkitpy.layout_tests.port.apple import ApplePort


_log = logging.getLogger(__name__)


class WinPort(ApplePort):
    port_name = "win"

    VERSION_FALLBACK_ORDER = ["win-xp", "win-vista", "win-7sp0", "win-win7"]

    ARCHITECTURES = ['x86']

    def do_text_results_differ(self, expected_text, actual_text):
        # Sanity was restored in WK2, so we don't need this hack there.
        if self.get_option('webkit_test_runner'):
            return ApplePort.do_text_results_differ(self, expected_text, actual_text)

        # This is a hack (which dates back to ORWT).
        # Windows does not have an EDITING DELEGATE, so we strip any EDITING DELEGATE
        # messages to make more of the tests pass.
        # It's possible more of the ports might want this and this could move down into WebKitPort.
        delegate_regexp = re.compile("^EDITING DELEGATE: .*?\n", re.MULTILINE)
        expected_text = delegate_regexp.sub("", expected_text)
        actual_text = delegate_regexp.sub("", actual_text)
        return expected_text != actual_text

    def default_baseline_search_path(self):
        name = self._name.replace('-wk2', '')
        if name.endswith(self.FUTURE_VERSION):
            fallback_names = [self.port_name]
        else:
            fallback_names = self.VERSION_FALLBACK_ORDER[self.VERSION_FALLBACK_ORDER.index(name):-1] + [self.port_name]
        # FIXME: The AppleWin port falls back to AppleMac for some results.  Eventually we'll have a shared 'apple' port.
        if self.get_option('webkit_test_runner'):
            fallback_names.insert(0, 'win-wk2')
            fallback_names.append('mac-wk2')
            # Note we do not add 'wk2' here, even though it's included in _skipped_search_paths().
        # FIXME: Perhaps we should get this list from MacPort?
        fallback_names.extend(['mac-lion', 'mac'])
        return map(self._webkit_baseline_path, fallback_names)

    def operating_system(self):
        return 'win'

    def show_results_html_file(self, results_filename):
        self._run_script('run-safari', [abspath_to_uri(SystemHost().platform, results_filename)])

    # FIXME: webkitperl/httpd.pm installs /usr/lib/apache/libphp4.dll on cycwin automatically
    # as part of running old-run-webkit-tests.  That's bad design, but we may need some similar hack.
    # We might use setup_environ_for_server for such a hack (or modify apache_http_server.py).

    def _runtime_feature_list(self):
        supported_features_command = [self._path_to_driver(), '--print-supported-features']
        try:
            output = self._executive.run_command(supported_features_command, error_handler=Executive.ignore_error)
        except OSError, e:
            _log.warn("Exception running driver: %s, %s.  Driver must be built before calling WebKitPort.test_expectations()." % (supported_features_command, e))
            return None

        # Note: win/DumpRenderTree.cpp does not print a leading space before the features_string.
        match_object = re.match("SupportedFeatures:\s*(?P<features_string>.*)\s*", output)
        if not match_object:
            return None
        return match_object.group('features_string').split(' ')
