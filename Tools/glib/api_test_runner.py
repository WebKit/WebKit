#!/usr/bin/env python
#
# Copyright (C) 2011, 2012, 2017 Igalia S.L.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.

import os
import errno
import sys
import re
from signal import SIGKILL, SIGSEGV
from glib_test_runner import GLibTestRunner

top_level_directory = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(top_level_directory, "Tools", "glib"))
import common
from webkitpy.common.host import Host
from webkitpy.common.test_expectations import TestExpectations
from webkitpy.common.timeout_context import Timeout

if os.name == 'posix' and sys.version_info[0] < 3:
    try:
        import subprocess32 as subprocess
    except ImportError:
        import subprocess
else:
    import subprocess

class TestRunner(object):
    TEST_DIRS = []

    def __init__(self, port, options, tests=[]):
        self._options = options

        self._port = Host().port_factory.get(port)
        self._driver = self._create_driver()

        if self._options.debug:
            self._build_type = "Debug"
        elif self._options.release:
            self._build_type = "Release"
        else:
            self._build_type = self._port.default_configuration()
        common.set_build_types((self._build_type,))

        self._programs_path = common.binary_build_path()
        expectations_file = os.path.join(common.top_level_path(), "Tools", "TestWebKitAPI", "glib", "TestExpectations.json")
        self._expectations = TestExpectations(self._port.name(), expectations_file, self._build_type)
        self._tests = self._get_tests(tests)
        self._disabled_tests = []

    def _test_programs_base_dir(self):
        return os.path.join(self._programs_path, "TestWebKitAPI")

    def _get_tests_from_dir(self, test_dir):
        if not os.path.isdir(test_dir):
            return []

        tests = []
        for test_file in os.listdir(test_dir):
            if not test_file.lower().startswith("test"):
                continue
            test_path = os.path.join(test_dir, test_file)
            if os.path.isfile(test_path) and os.access(test_path, os.X_OK):
                tests.append(test_path)
        return tests

    def _get_tests(self, initial_tests):
        tests = []
        for test in initial_tests:
            if os.path.isdir(test):
                tests.extend(self._get_tests_from_dir(test))
            else:
                tests.append(test)
        if tests:
            return tests

        tests = []
        for test_dir in self.TEST_DIRS:
            absolute_test_dir = os.path.join(self._test_programs_base_dir(), test_dir)
            tests.extend(self._get_tests_from_dir(absolute_test_dir))
        return tests

    def _create_driver(self, port_options=[]):
        self._port._display_server = self._options.display_server
        driver = self._port.create_driver(worker_number=0, no_timeout=True)._make_driver(pixel_tests=False)
        if not driver.check_driver(self._port):
            raise RuntimeError("Failed to check driver %s" % driver.__class__.__name__)
        return driver

    def _setup_testing_environment(self):
        self._test_env = self._driver._setup_environ_for_test()
        self._test_env["TEST_WEBKIT_API_WEBKIT2_RESOURCES_PATH"] = common.top_level_path("Tools", "TestWebKitAPI", "Tests", "WebKit")
        self._test_env["TEST_WEBKIT_API_WEBKIT2_INJECTED_BUNDLE_PATH"] = common.library_build_path()
        self._test_env["WEBKIT_EXEC_PATH"] = self._programs_path

    def _tear_down_testing_environment(self):
        if self._driver:
            self._driver.stop()

    def _test_cases_to_skip(self, test_program):
        if self._options.skipped_action != 'skip':
            return []

        return self._expectations.skipped_subtests(os.path.basename(test_program))

    def _should_run_test_program(self, test_program):
        for disabled_test in self._disabled_tests:
            if test_program.endswith(disabled_test):
                return False

        if self._options.skipped_action != 'skip':
            return True

        return os.path.basename(test_program) not in self._expectations.skipped_tests()

    def _kill_process(self, pid):
        try:
            os.kill(pid, SIGKILL)
        except OSError:
            # Process already died.
            pass

    def _waitpid(self, pid):
        while True:
            try:
                dummy, status = os.waitpid(pid, 0)
                if os.WIFSIGNALED(status):
                    return -os.WTERMSIG(status)
                if os.WIFEXITED(status):
                    return os.WEXITSTATUS(status)

                # Should never happen
                raise RuntimeError("Unknown child exit status!")
            except (OSError, IOError) as e:
                if e.errno == errno.EINTR:
                    continue
                if e.errno == errno.ECHILD:
                    # This happens if SIGCLD is set to be ignored or waiting
                    # for child processes has otherwise been disabled for our
                    # process.  This child is dead, we can't get the status.
                    return 0
                raise

    def _run_test_glib(self, test_program):
        timeout = self._options.timeout

        def is_slow_test(test, subtest):
            return self._expectations.is_slow(test, subtest)

        return GLibTestRunner(test_program, timeout, is_slow_test, timeout * 10).run(skipped=self._test_cases_to_skip(test_program), env=self._test_env)

    def _run_test_qt(self, test_program):
        env = self._test_env
        env['XDG_SESSION_TYPE'] = 'wayland'
        env['QML2_IMPORT_PATH'] = common.library_build_path('qml')

        name = os.path.basename(test_program)
        if not hasattr(subprocess, 'TimeoutExpired'):
            print("Can't run WPEQt test in Python2 without subprocess32")
            return {name: "FAIL"}

        try:
            output = subprocess.check_output([test_program, ], stderr=subprocess.STDOUT,
                                             env=env, timeout=self._options.timeout)
        except subprocess.CalledProcessError, exc:
            print(exc.output)
            if exc.returncode > 0:
                result = "FAIL"
            elif exc.returncode < 0:
                result = "CRASH"
        except subprocess.TimeoutExpired, exp:
            result = "TIMEOUT"
            print(exp.output)
        else:
            result = "PASS"
            print("**PASS** %s" % name)
        return {name: result}

    def _get_tests_from_google_test_suite(self, test_program):
        try:
            output = subprocess.check_output([test_program, '--gtest_list_tests'], env=self._test_env)
        except subprocess.CalledProcessError:
            sys.stderr.write("ERROR: could not list available tests for binary %s.\n" % (test_program))
            sys.stderr.flush()
            return 1

        skipped_test_cases = self._test_cases_to_skip(test_program)

        tests = []
        prefix = None
        for line in output.split('\n'):
            if not line.startswith('  '):
                prefix = line
                continue
            else:
                test_name = prefix + line.strip()
                if not test_name in skipped_test_cases:
                    tests.append(test_name)
        return tests

    def _run_google_test(self, test_program, subtest):
        command = [test_program, '--gtest_filter=%s' % (subtest)]
        timeout = self._options.timeout
        if self._expectations.is_slow(os.path.basename(test_program), subtest):
            timeout *= 10

        pid, fd = os.forkpty()
        if pid == 0:
            os.execvpe(command[0], command, self._test_env)
            sys.exit(0)

        with Timeout(timeout):
            try:
                common.parse_output_lines(fd, sys.stdout.write)
                status = self._waitpid(pid)
            except RuntimeError:
                self._kill_process(pid)
                sys.stdout.write("**TIMEOUT** %s\n" % subtest)
                sys.stdout.flush()
                return {subtest: "TIMEOUT"}

        if status == -SIGSEGV:
            sys.stdout.write("**CRASH** %s\n" % subtest)
            sys.stdout.flush()
            return {subtest: "CRASH"}

        if status != 0:
            return {subtest: "FAIL"}

        return {subtest: "PASS"}

    def _run_google_test_suite(self, test_program):
        result = {}
        for subtest in self._get_tests_from_google_test_suite(test_program):
            result.update(self._run_google_test(test_program, subtest))
        return result

    def is_glib_test(self, test_program):
        raise NotImplementedError

    def is_google_test(self, test_program):
        raise NotImplementedError

    def is_qt_test(self, test_program):
        raise NotImplementedError

    def _run_test(self, test_program):
        if self.is_glib_test(test_program):
            return self._run_test_glib(test_program)

        if self.is_google_test(test_program):
            return self._run_google_test_suite(test_program)

        if self.is_qt_test(test_program):
            return self._run_test_qt(test_program)

        return {}

    def run_tests(self):
        if not self._tests:
            sys.stderr.write("ERROR: tests not found in %s.\n" % (self._test_programs_base_dir()))
            sys.stderr.flush()
            return 1

        self._setup_testing_environment()

        # Remove skipped tests now instead of when we find them, because
        # some tests might be skipped while setting up the test environment.
        self._tests = [test for test in self._tests if self._should_run_test_program(test)]

        crashed_tests = {}
        failed_tests = {}
        timed_out_tests = {}
        passed_tests = {}
        try:
            for test in self._tests:
                results = self._run_test(test)
                for test_case, result in results.iteritems():
                    if result in self._expectations.get_expectation(os.path.basename(test), test_case):
                        continue

                    if result == "FAIL":
                        failed_tests.setdefault(test, []).append(test_case)
                    elif result == "TIMEOUT":
                        timed_out_tests.setdefault(test, []).append(test_case)
                    elif result == "CRASH":
                        crashed_tests.setdefault(test, []).append(test_case)
                    elif result == "PASS":
                        passed_tests.setdefault(test, []).append(test_case)
        finally:
            self._tear_down_testing_environment()

        def report(tests, title, base_dir):
            if not tests:
                return
            sys.stdout.write("\nUnexpected %s (%d)\n" % (title, sum(len(value) for value in tests.itervalues())))
            for test in tests:
                sys.stdout.write("    %s\n" % (test.replace(base_dir, '', 1)))
                for test_case in tests[test]:
                    sys.stdout.write("        %s\n" % (test_case))
            sys.stdout.flush()

        report(failed_tests, "failures", self._test_programs_base_dir())
        report(crashed_tests, "crashes", self._test_programs_base_dir())
        report(timed_out_tests, "timeouts", self._test_programs_base_dir())
        report(passed_tests, "passes", self._test_programs_base_dir())

        return len(failed_tests) + len(timed_out_tests)


def add_options(option_parser):
    option_parser.add_option('-r', '--release',
                             action='store_true', dest='release',
                             help='Run in Release')
    option_parser.add_option('-d', '--debug',
                             action='store_true', dest='debug',
                             help='Run in Debug')
    option_parser.add_option('--skipped', action='store', dest='skipped_action',
                             choices=['skip', 'ignore', 'only'], default='skip',
                             metavar='skip|ignore|only',
                             help='Specifies how to treat the skipped tests')
    option_parser.add_option('-t', '--timeout',
                             action='store', type='int', dest='timeout', default=5,
                             help='Time in seconds until a test times out')
