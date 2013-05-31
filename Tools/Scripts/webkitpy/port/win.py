# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013 Apple Inc. All rights reserved.
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

import atexit
import os
import logging
import re
import sys

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.system.executive import ScriptError, Executive
from webkitpy.common.system.path import abspath_to_uri, cygpath
from webkitpy.port.apple import ApplePort


_log = logging.getLogger(__name__)


class WinPort(ApplePort):
    port_name = "win"

    VERSION_FALLBACK_ORDER = ["win-xp", "win-vista", "win-7sp0", "win"]

    ARCHITECTURES = ['x86']

    CRASH_LOG_PREFIX = "CrashLog"

    POST_MORTEM_DEBUGGER_KEY = "/HKLM/SOFTWARE/Microsoft/Windows NT/CurrentVersion/AeDebug/%s"

    previous_debugger_values = {}

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

    # Note: These are based on the stock Cygwin locations for these files.
    def _uses_apache(self):
        return False

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    # Remove this implementation when we are confident that DumpRenderTree on Windows works properly in parallel.
    def default_child_processes(self):
        return 1

    def _driver_tempdir_for_environment(self):
        return cygpath(self._driver_tempdir())

    def test_search_path(self):
        test_fallback_names = [path for path in self.baseline_search_path() if not path.startswith(self._webkit_baseline_path('mac'))]
        return map(self._webkit_baseline_path, test_fallback_names)

    def _ntsd_location(self):
        possible_paths = [self._filesystem.join(os.environ['PROGRAMFILES'], "Windows Kits", "8.0", "Debuggers", "x86", "ntsd.exe"),
            self._filesystem.join(os.environ['PROGRAMFILES'], "Windows Kits", "8.0", "Debuggers", "x64", "ntsd.exe"),
            self._filesystem.join(os.environ['PROGRAMFILES'], "Debugging Tools for Windows (x86)", "ntsd.exe"),
            self._filesystem.join(os.environ['ProgramW6432'], "Debugging Tools for Windows (x64)", "ntsd.exe"),
            self._filesystem.join(os.environ['SYSTEMROOT'], "system32", "ntsd.exe")]
        for path in possible_paths:
            expanded_path = self._filesystem.expanduser(path)
            if self._filesystem.exists(expanded_path):
                _log.debug("Using ntsd located in '%s'" % path)
                return expanded_path
        return None

    def create_debugger_command_file(self):
        debugger_temp_directory = str(self._filesystem.mkdtemp())
        command_file = self._filesystem.join(debugger_temp_directory, "debugger-commands.txt")
        self._filesystem.write_text_file(command_file, '.logopen /t "%s\\%s.txt"\n' % (cygpath(self.results_directory()), self.CRASH_LOG_PREFIX))
        self._filesystem.write_text_file(command_file, '.srcpath "%s"\n' % cygpath(self._webkit_finder.webkit_base()))
        self._filesystem.write_text_file(command_file, '!analyze -vv\n')
        self._filesystem.write_text_file(command_file, '~*kpn\n')
        self._filesystem.write_text_file(command_file, 'q\n')
        return command_file

    def read_registry_string(self, key):
        registry_key = self.POST_MORTEM_DEBUGGER_KEY % key
        read_registry_command = ["regtool", "--wow32", "get", registry_key]
        value = self._executive.run_command(read_registry_command, error_handler=Executive.ignore_error)
        return value.rstrip()

    def write_registry_string(self, key, value):
        registry_key = self.POST_MORTEM_DEBUGGER_KEY % key
        set_reg_value_command = ["regtool", "--wow32", "set", "-s", str(registry_key), str(value)]
        rc = self._executive.run_command(set_reg_value_command, return_exit_code=True)
        if rc == 2:
            add_reg_value_command = ["regtool", "--wow32", "add", "-s", str(registry_key)]
            rc = self._executive.run_command(add_reg_value_command, return_exit_code=True)
            if rc == 0:
                rc = self._executive.run_command(set_reg_value_command, return_exit_code=True)
        if rc:
            _log.warn("Error setting key: %s to value %s.  Error=%ld." % (key, value, rc))
            return False

        # On Windows Vista/7 with UAC enabled, regtool will fail to modify the registry, but will still
        # return a successful exit code. So we double-check here that the value we tried to write to the
        # registry was really written.
        if self.read_registry_string(key) != value:
            _log.warn("Regtool reported success, but value of key %s did not change." % key)
            return False

        return True

    def setup_crash_log_saving(self):
        if '_NT_SYMBOL_PATH' not in os.environ:
            _log.warning("The _NT_SYMBOL_PATH environment variable is not set. Crash logs will not be saved.")
            return None
        ntsd_path = self._ntsd_location()
        if not ntsd_path:
            _log.warning("Can't find ntsd.exe. Crash logs will not be saved.")
            return None
        # If we used -c (instead of -cf) we could pass the commands directly on the command line. But
        # when the commands include multiple quoted paths (e.g., for .logopen and .srcpath), Windows
        # fails to invoke the post-mortem debugger at all (perhaps due to a bug in Windows's command
        # line parsing). So we save the commands to a file instead and tell the debugger to execute them
        # using -cf.
        command_file = self.create_debugger_command_file()
        if not command_file:
            return None
        debugger_options = '"{0}" -p %ld -e %ld -g -lines -cf "{1}"'.format(cygpath(ntsd_path), cygpath(command_file))
        registry_settings = {'Debugger': debugger_options, 'Auto': "1"}
        for key in registry_settings:
            self.previous_debugger_values[key] = self.read_registry_string(key)
            self.write_registry_string(key, registry_settings[key])

    def restore_crash_log_saving(self):
        for key in self.previous_debugger_values:
            self.write_registry_string(key, self.previous_debugger_values[key])

    def setup_test_run(self):
        atexit.register(self.restore_crash_log_saving)
        self.setup_crash_log_saving()
        super(WinPort, self).setup_test_run()

    def clean_up_test_run(self):
        self.restore_crash_log_saving()
        super(WinPort, self).clean_up_test_run()
