# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
import glob
import logging
import os
import re
import sys
import time

from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.common.system.executive import Executive
from webkitpy.common.system.path import abspath_to_uri, cygpath
from webkitpy.common.version import Version
from webkitpy.common.version_name_map import VersionNameMap
from webkitpy.port.apple import ApplePort
from webkitpy.port.config import apple_additions

_log = logging.getLogger(__name__)


try:
    import _winreg
    import win32com.client
except ImportError:
    _log.debug("Not running on native Windows.")


class WinPort(ApplePort):
    port_name = "win"

    VERSION_MIN = Version(5, 1)
    VERSION_MAX = Version(10)

    ARCHITECTURES = ['x86', 'x86_64']

    CRASH_LOG_PREFIX = "CrashLog"

    if sys.platform.startswith('win'):
        POST_MORTEM_DEBUGGER_KEY = r'SOFTWARE\Microsoft\Windows NT\CurrentVersion\AeDebug'
        WOW64_POST_MORTEM_DEBUGGER_KEY = r'SOFTWARE\Wow6432Node\Microsoft\Windows NT\CurrentVersion\AeDebug'
        WINDOWS_ERROR_REPORTING_KEY = r'SOFTWARE\Microsoft\Windows\Windows Error Reporting'
        WOW64_WINDOWS_ERROR_REPORTING_KEY = r'SOFTWARE\Wow6432Node\Microsoft\Windows\Windows Error Reporting'
        _HKLM = _winreg.HKEY_LOCAL_MACHINE
        _HKCU = _winreg.HKEY_CURRENT_USER
        _REG_DWORD = _winreg.REG_DWORD
        _REG_SZ = _winreg.REG_SZ
    else:
        POST_MORTEM_DEBUGGER_KEY = "/%s/SOFTWARE/Microsoft/Windows NT/CurrentVersion/AeDebug/%s"
        WOW64_POST_MORTEM_DEBUGGER_KEY = "/%s/SOFTWARE/Wow6432Node/Microsoft/Windows NT/CurrentVersion/AeDebug/%s"
        WINDOWS_ERROR_REPORTING_KEY = "/%s/SOFTWARE/Microsoft/Windows/Windows Error Reporting/%s"
        WOW64_WINDOWS_ERROR_REPORTING_KEY = "/%s/SOFTWARE/Wow6432Node/Microsoft/Windows/Windows Error Reporting/%s"
        _HKLM = "HKLM"
        _HKCU = "HKCU"
        _REG_DWORD = "-d"
        _REG_SZ = "-s"

    previous_debugger_values = {}
    previous_wow64_debugger_values = {}

    previous_error_reporting_values = {}
    previous_wow64_error_reporting_values = {}

    def __init__(self, host, port_name, **kwargs):
        ApplePort.__init__(self, host, port_name, **kwargs)
        if port_name.split('-') > 1:
            self._os_version = VersionNameMap.map(host.platform).from_name(port_name.split('-')[1])[1]
        else:
            self._os_version = self.host.platform.os_version

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
        version_name_map = VersionNameMap.map(self.host.platform)
        if self._os_version < self.VERSION_MIN or self._os_version > self.VERSION_MAX:
            fallback_versions = [self._os_version]
        else:
            sorted_versions = sorted(version_name_map.mapping_for_platform(platform=self.port_name).values())
            fallback_versions = sorted_versions[sorted_versions.index(self._os_version):]
        fallback_names = ['win-' + version_name_map.to_name(version, platform=self.port_name).lower().replace(' ', '') for version in fallback_versions]
        fallback_names.append('win')

        # FIXME: The AppleWin port falls back to AppleMac for some results.  Eventually we'll have a shared 'apple' port.
        if self.get_option('webkit_test_runner'):
            fallback_names.insert(0, 'win-wk2')
            fallback_names.append('mac-wk2')
            # Note we do not add 'wk2' here, even though it's included in _skipped_search_paths().
        # FIXME: Perhaps we should get this list from MacPort?
        fallback_names.append('mac')
        result = map(self._webkit_baseline_path, fallback_names)
        if apple_additions() and getattr(apple_additions(), "layout_tests_path", None):
            result.insert(0, self._filesystem.join(apple_additions().layout_tests_path(), self.port_name))
        return result

    def setup_environ_for_server(self, server_name=None):
        env = super(WinPort, self).setup_environ_for_server(server_name)
        env['XML_CATALOG_FILES'] = ''  # work around missing /etc/catalog <rdar://problem/4292995>
        return env

    def environment_for_api_tests(self):
        env = super(WinPort, self).environment_for_api_tests()
        for variable in ['SYSTEMROOT', 'WEBKIT_LIBRARIES']:
            self._copy_value_from_environ_if_set(env, variable)
        return env

    def operating_system(self):
        return 'win'

    def _port_flag_for_scripts(self):
        if self.get_option('architecture') == 'x86_64':
            return '--64-bit'
        return None

    def show_results_html_file(self, results_filename):
        self._run_script('run-safari', [abspath_to_uri(SystemHost().platform, results_filename)])

    def _runtime_feature_list(self):
        supported_features_command = [self._path_to_driver(), '--print-supported-features']
        try:
            output = self._executive.run_command(supported_features_command, ignore_errors=True)
        except OSError as e:
            _log.warn("Exception running driver: %s, %s.  Driver must be built before calling WebKitPort.test_expectations()." % (supported_features_command, e))
            return None

        # Note: win/DumpRenderTree.cpp does not print a leading space before the features_string.
        match_object = re.match("SupportedFeatures:\s*(?P<features_string>.*)\s*", output)
        if not match_object:
            return None
        return match_object.group('features_string').split(' ')

    def _build_path(self, *comps):
        """Returns the full path to the test driver (DumpRenderTree)."""
        root_directory = self.get_option('_cached_root') or self.get_option('root')
        if not root_directory:
            ApplePort._build_path(self, *comps)  # Sets option _cached_root
            binary_directory = 'bin32'
            if self.get_option('architecture') == 'x86_64':
                binary_directory = 'bin64'
            root_directory = self._filesystem.join(self.get_option('_cached_root'), binary_directory)
            self.set_option('_cached_root', root_directory)

        return self._filesystem.join(root_directory, *comps)

    def is_cygwin(self):
        """Return whether current platform is Cygwin or not"""
        return self.host.platform.is_cygwin()

    # Note: These are based on the stock XAMPP locations for these files.
    def _uses_apache(self):
        return True

    def _path_to_apache(self):
        root = os.environ.get('XAMPP_ROOT', 'C:\\xampp')
        path = self._filesystem.join(root, 'apache', 'bin', 'httpd.exe')
        if self._filesystem.exists(path):
            return path
        _log.error('Could not find apache in the expected location. (path=%s)' % path)
        return None

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    def _path_to_image_diff(self):
        if self.is_cygwin():
            return super(WinPort, self)._path_to_image_diff()

        return self._build_path('ImageDiff.exe')

    API_TEST_BINARY_NAMES = ['TestWTF.exe', 'TestWebCore.exe', 'TestWebKitLegacy.exe']

    def path_to_api_test_binaries(self):
        return {binary.split('.')[0]: self._build_path(binary) for binary in self.API_TEST_BINARY_NAMES}

    def test_search_path(self):
        test_fallback_names = [path for path in self.baseline_search_path() if not path.startswith(self._webkit_baseline_path('mac'))]
        return map(self._webkit_baseline_path, test_fallback_names)

    def _ntsd_location(self):
        if 'PROGRAMFILES' not in os.environ:
            return None
        possible_paths = [self._filesystem.join(os.environ['PROGRAMFILES'], "Windows Kits", "10", "Debuggers", "x64", "ntsd.exe"),
            self._filesystem.join(os.environ['PROGRAMFILES'], "Windows Kits", "8.1", "Debuggers", "x64", "ntsd.exe"),
            self._filesystem.join(os.environ['PROGRAMFILES'], "Windows Kits", "8.0", "Debuggers", "x64", "ntsd.exe")]
        if self.get_option('architecture') == 'x86_64':
            possible_paths.append(self._filesystem.join("{0} (x86)".format(os.environ['PROGRAMFILES']), "Windows Kits", "10", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join("{0} (x86)".format(os.environ['PROGRAMFILES']), "Windows Kits", "8.1", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join("{0} (x86)".format(os.environ['PROGRAMFILES']), "Windows Kits", "8.0", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join("{0} (x86)".format(os.environ['PROGRAMFILES']), "Debugging Tools for Windows (x64)", "ntsd.exe"))
        else:
            possible_paths.append(self._filesystem.join(os.environ['PROGRAMFILES'], "Debugging Tools for Windows (x86)", "ntsd.exe"))
        possible_paths.append(self._filesystem.join(os.environ['SYSTEMROOT'], "system32", "ntsd.exe"))
        if 'ProgramW6432' in os.environ:
            possible_paths.append(self._filesystem.join(os.environ['ProgramW6432'], "Windows Kits", "10", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join(os.environ['ProgramW6432'], "Windows Kits", "8.1", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join(os.environ['ProgramW6432'], "Windows Kits", "8.0", "Debuggers", "x64", "ntsd.exe"))
            possible_paths.append(self._filesystem.join(os.environ['ProgramW6432'], "Debugging Tools for Windows (x64)", "ntsd.exe"))
        for path in possible_paths:
            expanded_path = self._filesystem.expanduser(path)
            _log.debug("Considering '%s'" % expanded_path)
            if self._filesystem.exists(expanded_path):
                _log.debug("Using ntsd located in '%s'" % path)
                return expanded_path
        return None

    def create_debugger_command_file(self):
        debugger_temp_directory = str(self._filesystem.mkdtemp())
        command_file = self._filesystem.join(debugger_temp_directory, "debugger-commands.txt")
        commands = ''.join(['.logopen /t "%s\\%s.txt"\n' % (cygpath(self.results_directory()), self.CRASH_LOG_PREFIX),
            '.srcpath "%s"\n' % cygpath(self._webkit_finder.webkit_base()),
            '!analyze -vv\n',
            '~*kpn\n',
            'q\n'])
        self._filesystem.write_text_file(command_file, commands)
        return command_file

    def read_registry_value(self, reg_path, arch, root, key):
        if sys.platform.startswith('win'):
            _log.debug("Trying to read %s\\%s" % (reg_path, key))
            try:
                registry_key = _winreg.OpenKey(root, reg_path)
                value = _winreg.QueryValueEx(registry_key, key)
                _winreg.CloseKey(registry_key)
            except WindowsError as ex:
                _log.debug("Unable to read %s\\%s: %s" % (reg_path, key, str(ex)))
                return ['', self._REG_SZ]
        else:
            registry_key = reg_path % (root, key)
            _log.debug("Reading %s" % (registry_key))
            read_registry_command = ["regtool", arch, "get", registry_key]
            int_value = self._executive.run_command(read_registry_command, ignore_errors=True)
            # regtool doesn't return the type of the entry, so need this ugly hack:
            if reg_path in (self.WINDOWS_ERROR_REPORTING_KEY, self.WOW64_WINDOWS_ERROR_REPORTING_KEY):
                _log.debug("I got {0}".format(int_value))
                try:
                    value = [int(int_value), self._REG_DWORD]
                except:
                    value = [0, self._REG_DWORD]
            else:
                value = [int_value.rstrip(), self._REG_SZ]

        _log.debug("I got back ({0}) of type ({1})".format(value[0], value[1]))
        return value

    def write_registry_value(self, reg_path, arch, root, key, regType, value):
        if sys.platform.startswith('win'):
            _log.debug("Trying to write %s\\%s = %s" % (reg_path, key, value))
            try:
                registry_key = _winreg.OpenKey(root, reg_path, 0, _winreg.KEY_WRITE)
            except WindowsError:
                try:
                    _log.debug("Key doesn't exist -- must create it.")
                    registry_key = _winreg.CreateKeyEx(root, reg_path, 0, _winreg.KEY_WRITE)
                except WindowsError as ex:
                    _log.error("Error setting (%s) %s\key: %s to value: %s.  Error=%s." % (arch, root, key, value, str(ex)))
                    _log.error("You many need to adjust permissions on the %s\\%s key." % (reg_path, key))
                    return False

            _log.debug("Writing {0} of type {1} to {2}\\{3}".format(value, regType, registry_key, key))
            _winreg.SetValueEx(registry_key, key, 0, regType, value)
            _winreg.CloseKey(registry_key)
        else:
            registry_key = reg_path % (root, key)
            _log.debug("Writing to %s" % registry_key)

            set_reg_value_command = ["regtool", arch, "set", regType, str(registry_key), str(value)]
            rc = self._executive.run_command(set_reg_value_command, return_exit_code=True)
            if rc == 2:
                add_reg_value_command = ["regtool", arch, "add", regType, str(registry_key)]
                rc = self._executive.run_command(add_reg_value_command, return_exit_code=True)
                if rc == 0:
                    rc = self._executive.run_command(set_reg_value_command, return_exit_code=True)
            if rc:
                _log.warn("Error setting (%s) %s\key: %s to value: %s.  Error=%s." % (arch, root, key, value, str(rc)))
                _log.warn("You many need to adjust permissions on the %s key." % registry_key)
                return False

        # On Windows Vista/7 with UAC enabled, regtool will fail to modify the registry, but will still
        # return a successful exit code. So we double-check here that the value we tried to write to the
        # registry was really written.
        check_value = self.read_registry_value(reg_path, arch, root, key)
        if check_value[0] != value or check_value[1] != regType:
            _log.warn("Reg update reported success, but value of key %s did not change." % key)
            _log.warn("Wanted to set it to ({0}, {1}), but got {2})".format(value, regType, check_value))
            _log.warn("You many need to adjust permissions on the %s\\%s key." % (reg_path, key))
            return False

        return True

    def setup_crash_log_saving(self):
        if '_NT_SYMBOL_PATH' not in os.environ:
            _log.warning("The _NT_SYMBOL_PATH environment variable is not set. Using Microsoft Symbol Server.")
            os.environ['_NT_SYMBOL_PATH'] = 'SRV*http://msdl.microsoft.com/download/symbols'

        # Add build path to symbol path
        os.environ['_NT_SYMBOL_PATH'] += ";" + self._build_path()

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
        debugger_options = '"{0}" -p %ld -e %ld -g -noio -lines -cf "{1}"'.format(cygpath(ntsd_path), cygpath(command_file))
        registry_settings = {'Debugger': [debugger_options, self._REG_SZ], 'Auto': ["1", self._REG_SZ]}
        for key, value in registry_settings.iteritems():
            for arch in ["--wow32", "--wow64"]:
                self.previous_debugger_values[(arch, self._HKLM, key)] = self.read_registry_value(self.POST_MORTEM_DEBUGGER_KEY, arch, self._HKLM, key)
                self.previous_wow64_debugger_values[(arch, self._HKLM, key)] = self.read_registry_value(self.WOW64_POST_MORTEM_DEBUGGER_KEY, arch, self._HKLM, key)
                self.write_registry_value(self.POST_MORTEM_DEBUGGER_KEY, arch, self._HKLM, key, value[1], value[0])
                self.write_registry_value(self.WOW64_POST_MORTEM_DEBUGGER_KEY, arch, self._HKLM, key, value[1], value[0])

    def restore_crash_log_saving(self):
        for key, value in self.previous_debugger_values.iteritems():
            self.write_registry_value(self.POST_MORTEM_DEBUGGER_KEY, key[0], key[1], key[2], value[1], value[0])
        for key, value in self.previous_wow64_debugger_values.iteritems():
            self.write_registry_value(self.WOW64_POST_MORTEM_DEBUGGER_KEY, key[0], key[1], key[2], value[1], value[0])

    def prevent_error_dialogs(self):
        registry_settings = {'DontShowUI': [1, self._REG_DWORD], 'Disabled': [1, self._REG_DWORD]}
        for key, value in registry_settings.iteritems():
            for root in [self._HKLM, self._HKCU]:
                for arch in ["--wow32", "--wow64"]:
                    self.previous_error_reporting_values[(arch, root, key)] = self.read_registry_value(self.WINDOWS_ERROR_REPORTING_KEY, arch, root, key)
                    self.previous_wow64_error_reporting_values[(arch, root, key)] = self.read_registry_value(self.WOW64_WINDOWS_ERROR_REPORTING_KEY, arch, root, key)
                    self.write_registry_value(self.WINDOWS_ERROR_REPORTING_KEY, arch, root, key, value[1], value[0])
                    self.write_registry_value(self.WOW64_WINDOWS_ERROR_REPORTING_KEY, arch, root, key, value[1], value[0])

    def allow_error_dialogs(self):
        for key, value in self.previous_error_reporting_values.iteritems():
            self.write_registry_value(self.WINDOWS_ERROR_REPORTING_KEY, key[0], key[1], key[2], value[1], value[0])
        for key, value in self.previous_wow64_error_reporting_values.iteritems():
            self.write_registry_value(self.WOW64_WINDOWS_ERROR_REPORTING_KEY, key[0], key[1], key[2], value[1], value[0])

    def delete_sem_locks(self):
        os.system("rm -rf /dev/shm/sem.*")

    def delete_preference_files(self):
        try:
            preferences_files = self._filesystem.join(os.environ['APPDATA'], "Apple Computer/Preferences", "com.apple.DumpRenderTree*")
            filelist = glob.glob(preferences_files)
            for file in filelist:
                self._filesystem.remove(file)
        except:
            _log.warn("Failed to delete preference files.")

    def setup_test_run(self, device_type=None):
        atexit.register(self.restore_crash_log_saving)
        self.setup_crash_log_saving()
        self.prevent_error_dialogs()
        self.delete_sem_locks()
        self.delete_preference_files()
        super(WinPort, self).setup_test_run(device_type)

    def clean_up_test_run(self):
        self.allow_error_dialogs()
        self.restore_crash_log_saving()
        super(WinPort, self).clean_up_test_run()

    def path_to_crash_logs(self):
        return self.results_directory()

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, time_fn=None, sleep_fn=None, wait_for_log=True, target_host=None):
        # Note that we do slow-spin here and wait, since it appears the time
        # ReportCrash takes to actually write and flush the file varies when there are
        # lots of simultaneous crashes going on.
        # FIXME: Should most of this be moved into CrashLogs()?
        time_fn = time_fn or time.time
        sleep_fn = sleep_fn or time.sleep
        crash_log = ''
        crash_logs = CrashLogs(target_host or self.host, self.path_to_crash_logs(), crash_logs_to_skip=self._crash_logs_to_skip_for_host.get(target_host or self.host, []))
        now = time_fn()
        # FIXME: delete this after we're sure this code is working ...
        _log.debug('looking for crash log for %s:%s' % (name, str(pid)))
        deadline = now + 5 * int(self.get_option('child_processes', 1))
        while not crash_log and now <= deadline:
            # If the system_pid hasn't been determined yet, just try with the passed in pid.  We'll be checking again later
            system_pid = self._executive.pid_to_system_pid.get(pid)
            if system_pid == None:
                break  # We haven't mapped cygwin pid->win pid yet
            crash_log = crash_logs.find_newest_log(name, system_pid, include_errors=True, newer_than=newer_than)
            if not wait_for_log:
                break
            if not crash_log or not [line for line in crash_log.splitlines() if line.startswith('quit:')]:
                sleep_fn(0.1)
                now = time_fn()

        if not crash_log:
            return (stderr, None)
        return (stderr, crash_log)

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        """Since crash logs can take a long time to be written out if the system is
           under stress do a second pass at the end of the test run.

           crashes: test_name -> pid, process_name tuple of crashed process
           start_time: time the tests started at.  We're looking for crash
               logs after that time.
        """
        crash_logs = {}
        for (test_name, process_name, pid) in crashed_processes:
            # Passing None for output.  This is a second pass after the test finished so
            # if the output had any logging we would have already collected it.
            crash_log = self._get_crash_log(process_name, pid, None, None, start_time, wait_for_log=False)[1]
            if crash_log:
                crash_logs[test_name] = crash_log
        return crash_logs

    def check_httpd(self):
        if not super(WinPort, self).check_httpd():
            return False

        path = self._path_to_apache()
        if not path:
            return False

        # To launch Apache as a daemon, service installation is required.
        exit_code = self._executive.run_command([path, '-k', 'install', '-T'], return_exit_code=True)
        # 0=success, 2=already installed, 720005=permission error, etc.
        if exit_code not in (0, 2):
            _log.error('Could not install httpd as a service. Perhaps you forgot to run as adminstrator? (exit code={})'.format(exit_code))
            return False

        return True


class WinCairoPort(WinPort):
    port_name = "wincairo"

    DEFAULT_ARCHITECTURE = 'x86_64'

    def default_baseline_search_path(self):
        version_name_map = VersionNameMap.map(self.host.platform)
        if self._os_version < self.VERSION_MIN or self._os_version > self.VERSION_MAX:
            fallback_versions = [self._os_version]
        else:
            sorted_versions = sorted(version_name_map.mapping_for_platform(platform=self.port_name).values())
            fallback_versions = sorted_versions[sorted_versions.index(self._os_version):]
        fallback_names = ['wincairo-' + version_name_map.to_name(version, platform=self.port_name).lower().replace(' ', '') for version in fallback_versions]
        fallback_names.append('wincairo')
        return map(self._webkit_baseline_path, fallback_names)
