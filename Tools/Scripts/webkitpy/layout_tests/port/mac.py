# Copyright (C) 2011 Google Inc. All rights reserved.
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
import platform
import re

from webkitpy.layout_tests.port.apple import ApplePort
from webkitpy.layout_tests.port.leakdetector import LeakDetector


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
        7: 'lion',
    }
    assert release_version >= min(version_strings.keys())
    version_string = version_strings.get(release_version, 'future')
    if supported_versions:
        assert version_string in supported_versions
    return version_string


class MacPort(ApplePort):
    port_name = "mac"

    # This is a list of all supported OS-VERSION pairs for the AppleMac port
    # and the order of fallback between them.  Matches ORWT.
    VERSION_FALLBACK_ORDER = ["mac-leopard", "mac-snowleopard", "mac-lion", "mac"]

    def _detect_version(self, os_version_string):
        # FIXME: MacPort and WinPort implement _detect_version differently.
        # WinPort uses os_version_string as a replacement for self.version.
        # Thus just returns os_version_string from this function if not None.
        # Mac (incorrectly) uses os_version_string as a way to unit-test
        # the os_version parsing logic.  We should split the os_version parsing tests
        # into separate unittests so that they do not need to construct
        # MacPort objects just to test our version parsing.
        return os_version(os_version_string)

    def __init__(self, **kwargs):
        ApplePort.__init__(self, **kwargs)
        self._operating_system = 'mac'
        self._leak_detector = LeakDetector(self)
        if self.get_option("leaks"):
            # DumpRenderTree slows down noticably if we run more than about 1000 tests in a batch
            # with MallocStackLogging enabled.
            self.set_option_default("batch_size", 1000)

    def baseline_search_path(self):
        try:
            fallback_index = self.VERSION_FALLBACK_ORDER.index(self._port_name_with_version())
            fallback_names = list(self.VERSION_FALLBACK_ORDER[fallback_index:])
        except ValueError:
            # Unknown versions just fall back to the base port results.
            fallback_names = [self.port_name]
        if self.get_option('webkit_test_runner'):
            fallback_names.insert(0, self._wk2_port_name())
            # Note we do not add 'wk2' here, even though it's included in _skipped_search_paths().
        return map(self._webkit_baseline_path, fallback_names)

    def setup_environ_for_server(self, server_name=None):
        env = super(MacPort, self).setup_environ_for_server(server_name)
        if server_name == self.driver_name():
            if self.get_option('leaks'):
                env['MallocStackLogging'] = '1'
            if self.get_option('guard_malloc'):
                env['DYLD_INSERT_LIBRARIES'] = '/usr/lib/libgmalloc.dylib'
        return env

    # Belongs on a Platform object.
    def is_leopard(self):
        return self._version == "leopard"

    # Belongs on a Platform object.
    def is_snowleopard(self):
        return self._version == "snowleopard"

    # Belongs on a Platform object.
    def is_crash_reporter(self, process_name):
        return re.search(r'ReportCrash', process_name)

    def _build_java_test_support(self):
        java_tests_path = self._filesystem.join(self.layout_tests_dir(), "java")
        build_java = ["/usr/bin/make", "-C", java_tests_path]
        if self._executive.run_command(build_java, return_exit_code=True):  # Paths are absolute, so we don't need to set a cwd.
            _log.error("Failed to build Java support files: %s" % build_java)
            return False
        return True

    def check_for_leaks(self, process_name, process_pid):
        if not self.get_option('leaks'):
            return
        # We could use http://code.google.com/p/psutil/ to get the process_name from the pid.
        self._leak_detector.check_for_leaks(process_name, process_pid)

    def print_leaks_summary(self):
        if not self.get_option('leaks'):
            return
        # We're in the manager process, so the leak detector will not have a valid list of leak files.
        # FIXME: This is a hack, but we don't have a better way to get this information from the workers yet.
        # FIXME: This will include too many leaks in subsequent runs until the results directory is cleared!
        leaks_files = self._leak_detector.leaks_files_in_directory(self.results_directory())
        if not leaks_files:
            return
        total_bytes_string, unique_leaks = self._leak_detector.parse_leak_files(leaks_files)
        # old-run-webkit-tests used to print the "total leaks" count, but that would
        # require re-parsing each of the leaks files (which we could do at some later point if that would be useful.)
        # Tools/BuildSlaveSupport/build.webkit.org-config/master.cfg greps for "leaks found".
        # master.cfg will need an update if these strings change.
        _log.info("leaks found for a total of %s!" % total_bytes_string)
        _log.info("%s unique leaks found!" % unique_leaks)

    def _check_port_build(self):
        return self._build_java_test_support()

    def _path_to_webcore_library(self):
        return self._build_path('WebCore.framework/Versions/A/WebCore')

    def show_results_html_file(self, results_filename):
        self._run_script('run-safari', ['-NSOpen', results_filename])
