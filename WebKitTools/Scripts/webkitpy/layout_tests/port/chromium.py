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

"""Chromium implementations of the Port interface."""

from __future__ import with_statement

import codecs
import logging
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import webbrowser

import base
import http_server

from webkitpy.common.system.executive import Executive
from webkitpy.layout_tests.layout_package import test_files
from webkitpy.layout_tests.layout_package import test_expectations

# Chromium DRT on OSX uses WebKitDriver.
if sys.platform == 'darwin':
    import webkit

import websocket_server

_log = logging.getLogger("webkitpy.layout_tests.port.chromium")


# FIXME: This function doesn't belong in this package.
def check_file_exists(path_to_file, file_description, override_step=None,
                      logging=True):
    """Verify the file is present where expected or log an error.

    Args:
        file_name: The (human friendly) name or description of the file
            you're looking for (e.g., "HTTP Server"). Used for error logging.
        override_step: An optional string to be logged if the check fails.
        logging: Whether or not log the error messages."""
    if not os.path.exists(path_to_file):
        if logging:
            _log.error('Unable to find %s' % file_description)
            _log.error('    at %s' % path_to_file)
            if override_step:
                _log.error('    %s' % override_step)
                _log.error('')
        return False
    return True


class ChromiumPort(base.Port):
    """Abstract base class for Chromium implementations of the Port class."""

    def __init__(self, port_name=None, options=None, **kwargs):
        base.Port.__init__(self, port_name, options, **kwargs)
        self._chromium_base_dir = None

    def baseline_path(self):
        return self._webkit_baseline_path(self._name)

    def check_build(self, needs_http):
        result = True

        dump_render_tree_binary_path = self._path_to_driver()
        result = check_file_exists(dump_render_tree_binary_path,
                                    'test driver') and result
        if result and self._options.build:
            result = self._check_driver_build_up_to_date(
                self._options.configuration)
        else:
            _log.error('')

        helper_path = self._path_to_helper()
        if helper_path:
            result = check_file_exists(helper_path,
                                       'layout test helper') and result

        if self._options.pixel_tests:
            result = self.check_image_diff(
                'To override, invoke with --no-pixel-tests') and result

        return result

    def check_sys_deps(self, needs_http):
        cmd = [self._path_to_driver(), '--check-layout-test-sys-deps']
        if self._executive.run_command(cmd, return_exit_code=True):
            _log.error('System dependencies check failed.')
            _log.error('To override, invoke with --nocheck-sys-deps')
            _log.error('')
            return False
        return True

    def check_image_diff(self, override_step=None, logging=True):
        image_diff_path = self._path_to_image_diff()
        return check_file_exists(image_diff_path, 'image diff exe',
                                 override_step, logging)

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None, tolerance=0):
        executable = self._path_to_image_diff()
        if diff_filename:
            cmd = [executable, '--diff', expected_filename, actual_filename,
                   diff_filename]
        else:
            cmd = [executable, expected_filename, actual_filename]

        result = True
        try:
            if self._executive.run_command(cmd, return_exit_code=True) == 0:
                return False
        except OSError, e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                _compare_available = False
            else:
                raise e
        return result

    def driver_name(self):
        return "test_shell"

    def path_from_chromium_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        Chromium source tree and the list of path components in |*comps|."""
        if not self._chromium_base_dir:
            abspath = os.path.abspath(__file__)
            offset = abspath.find('third_party')
            if offset == -1:
                self._chromium_base_dir = os.path.join(
                    abspath[0:abspath.find('WebKitTools')],
                    'WebKit', 'chromium')
            else:
                self._chromium_base_dir = abspath[0:offset]
        return os.path.join(self._chromium_base_dir, *comps)

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
            'chromium', 'test_expectations.txt')

    def results_directory(self):
        try:
            return self.path_from_chromium_base('webkit',
                self._options.configuration, self._options.results_directory)
        except AssertionError:
            return self._build_path(self._options.configuration,
                                    self._options.results_directory)

    def setup_test_run(self):
        # Delete the disk cache if any to ensure a clean test run.
        dump_render_tree_binary_path = self._path_to_driver()
        cachedir = os.path.split(dump_render_tree_binary_path)[0]
        cachedir = os.path.join(cachedir, "cache")
        if os.path.exists(cachedir):
            shutil.rmtree(cachedir)

    def show_results_html_file(self, results_filename):
        uri = self.filename_to_uri(results_filename)
        if self._options.use_drt:
            # FIXME: This should use User.open_url
            webbrowser.open(uri, new=1)
        else:
            # Note: Not thread safe: http://bugs.python.org/issue2320
            subprocess.Popen([self._path_to_driver(), uri])

    def create_driver(self, image_path, options):
        """Starts a new Driver and returns a handle to it."""
        if self._options.use_drt and sys.platform == 'darwin':
            return webkit.WebKitDriver(self, image_path, options, executive=self._executive)
        if self._options.use_drt:
            options += ['--test-shell']
        else:
            options += ['--layout-tests']
        return ChromiumDriver(self, image_path, options, executive=self._executive)

    def start_helper(self):
        helper_path = self._path_to_helper()
        if helper_path:
            _log.debug("Starting layout helper %s" % helper_path)
            # Note: Not thread safe: http://bugs.python.org/issue2320
            self._helper = subprocess.Popen([helper_path],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=None)
            is_ready = self._helper.stdout.readline()
            if not is_ready.startswith('ready'):
                _log.error("layout_test_helper failed to be ready")

    def stop_helper(self):
        if self._helper:
            _log.debug("Stopping layout test helper")
            self._helper.stdin.write("x\n")
            self._helper.stdin.close()
            # wait() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            self._helper.wait()

    def test_base_platform_names(self):
        return ('linux', 'mac', 'win')

    def test_expectations(self):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        expectations_path = self.path_to_test_expectations_file()
        with codecs.open(expectations_path, "r", "utf-8") as file:
            return file.read()

    def test_expectations_overrides(self):
        # FIXME: This drt_overrides handling should be removed when we switch
        # from tes_shell to DRT.
        drt_overrides = ''
        if self._options.use_drt:
            drt_overrides_path = self.path_from_webkit_base('LayoutTests',
                'platform', 'chromium', 'drt_expectations.txt')
            if os.path.exists(drt_overrides_path):
                with codecs.open(drt_overrides_path, "r", "utf-8") as file:
                    drt_overrides = file.read()

        try:
            overrides_path = self.path_from_chromium_base('webkit', 'tools',
                'layout_tests', 'test_expectations.txt')
        except AssertionError:
            return None
        if not os.path.exists(overrides_path):
            return None
        with codecs.open(overrides_path, "r", "utf-8") as file:
            return file.read() + drt_overrides

    def skipped_layout_tests(self, extra_test_files=None):
        expectations_str = self.test_expectations()
        overrides_str = self.test_expectations_overrides()
        test_platform_name = self.test_platform_name()
        is_debug_mode = False

        all_test_files = test_files.gather_test_files(self, '*')
        if extra_test_files:
            all_test_files.update(extra_test_files)

        expectations = test_expectations.TestExpectations(
            self, all_test_files, expectations_str, test_platform_name,
            is_debug_mode, is_lint_mode=True,
            tests_are_present=False, overrides=overrides_str)
        tests_dir = self.layout_tests_dir()
        return [self.relative_test_filename(test)
                for test in expectations.get_tests_with_result_type(test_expectations.SKIP)]

    def test_platform_names(self):
        return self.test_base_platform_names() + ('win-xp',
            'win-vista', 'win-7')

    def test_platform_name_to_name(self, test_platform_name):
        if test_platform_name in self.test_platform_names():
            return 'chromium-' + test_platform_name
        raise ValueError('Unsupported test_platform_name: %s' %
                         test_platform_name)

    def test_repository_paths(self):
        # Note: for JSON file's backward-compatibility we use 'chrome' rather
        # than 'chromium' here.
        repos = super(ChromiumPort, self).test_repository_paths()
        repos.append(('chrome', self.path_from_chromium_base()))
        return repos

    #
    # PROTECTED METHODS
    #
    # These routines should only be called by other methods in this file
    # or any subclasses.
    #

    def _check_driver_build_up_to_date(self, configuration):
        if configuration in ('Debug', 'Release'):
            try:
                debug_path = self._path_to_driver('Debug')
                release_path = self._path_to_driver('Release')

                debug_mtime = os.stat(debug_path).st_mtime
                release_mtime = os.stat(release_path).st_mtime

                if (debug_mtime > release_mtime and configuration == 'Release' or
                    release_mtime > debug_mtime and configuration == 'Debug'):
                    _log.warning('You are not running the most '
                                 'recent DumpRenderTree binary. You need to '
                                 'pass --debug or not to select between '
                                 'Debug and Release.')
                    _log.warning('')
            # This will fail if we don't have both a debug and release binary.
            # That's fine because, in this case, we must already be running the
            # most up-to-date one.
            except OSError:
                pass
        return True

    def _chromium_baseline_path(self, platform):
        if platform is None:
            platform = self.name()
        return self.path_from_webkit_base('LayoutTests', 'platform', platform)

    def _path_to_image_diff(self):
        binary_name = 'image_diff'
        if self._options.use_drt:
            binary_name = 'ImageDiff'
        return self._build_path(self._options.configuration, binary_name)


class ChromiumDriver(base.Driver):
    """Abstract interface for test_shell."""

    def __init__(self, port, image_path, options, executive=Executive()):
        self._port = port
        self._configuration = port._options.configuration
        # FIXME: _options is very confusing, because it's not an Options() element.
        # FIXME: These don't need to be passed into the constructor, but could rather
        # be passed into .start()
        self._options = options
        self._image_path = image_path
        self._executive = executive

    def start(self):
        # FIXME: Should be an error to call this method twice.
        cmd = []
        # FIXME: We should not be grabbing at self._port._options.wrapper directly.
        cmd += self._command_wrapper(self._port._options.wrapper)
        cmd += [self._port._path_to_driver()]
        if self._options:
            cmd += self._options

        # We need to pass close_fds=True to work around Python bug #2320
        # (otherwise we can hang when we kill DumpRenderTree when we are running
        # multiple threads). See http://bugs.python.org/issue2320 .
        # Note that close_fds isn't supported on Windows, but this bug only
        # shows up on Mac and Linux.
        close_flag = sys.platform not in ('win32', 'cygwin')
        self._proc = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT,
                                      close_fds=close_flag)

    def poll(self):
        # poll() is not threadsafe and can throw OSError due to:
        # http://bugs.python.org/issue1731717
        return self._proc.poll()

    def _write_command_and_read_line(self, input=None):
        """Returns a tuple: (line, did_crash)"""
        try:
            if input:
                if isinstance(input, unicode):
                    # TestShell expects utf-8
                    input = input.encode("utf-8")
                self._proc.stdin.write(input)
            # DumpRenderTree text output is always UTF-8.  However some tests
            # (e.g. webarchive) may spit out binary data instead of text so we
            # don't bother to decode the output (for either DRT or test_shell).
            line = self._proc.stdout.readline()
            # We could assert() here that line correctly decodes as UTF-8.
            return (line, False)
        except IOError, e:
            _log.error("IOError communicating w/ test_shell: " + str(e))
            return (None, True)

    def _test_shell_command(self, uri, timeoutms, checksum):
        cmd = uri
        if timeoutms:
            cmd += ' ' + str(timeoutms)
        if checksum:
            cmd += ' ' + checksum
        cmd += "\n"
        return cmd

    def run_test(self, uri, timeoutms, checksum):
        output = []
        error = []
        crash = False
        timeout = False
        actual_uri = None
        actual_checksum = None

        start_time = time.time()

        cmd = self._test_shell_command(uri, timeoutms, checksum)
        (line, crash) = self._write_command_and_read_line(input=cmd)

        while not crash and line.rstrip() != "#EOF":
            # Make sure we haven't crashed.
            if line == '' and self.poll() is not None:
                # This is hex code 0xc000001d, which is used for abrupt
                # termination. This happens if we hit ctrl+c from the prompt
                # and we happen to be waiting on test_shell.
                # sdoyon: Not sure for which OS and in what circumstances the
                # above code is valid. What works for me under Linux to detect
                # ctrl+c is for the subprocess returncode to be negative
                # SIGINT. And that agrees with the subprocess documentation.
                if (-1073741510 == self._proc.returncode or
                    - signal.SIGINT == self._proc.returncode):
                    raise KeyboardInterrupt
                crash = True
                break

            # Don't include #URL lines in our output
            if line.startswith("#URL:"):
                actual_uri = line.rstrip()[5:]
                if uri != actual_uri:
                    # GURL capitalizes the drive letter of a file URL.
                    if (not re.search("^file:///[a-z]:", uri) or
                        uri.lower() != actual_uri.lower()):
                        _log.fatal("Test got out of sync:\n|%s|\n|%s|" %
                                   (uri, actual_uri))
                        raise AssertionError("test out of sync")
            elif line.startswith("#MD5:"):
                actual_checksum = line.rstrip()[5:]
            elif line.startswith("#TEST_TIMED_OUT"):
                timeout = True
                # Test timed out, but we still need to read until #EOF.
            elif actual_uri:
                output.append(line)
            else:
                error.append(line)

            (line, crash) = self._write_command_and_read_line(input=None)

        return (crash, timeout, actual_checksum, ''.join(output),
                ''.join(error))

    def stop(self):
        if self._proc:
            self._proc.stdin.close()
            self._proc.stdout.close()
            if self._proc.stderr:
                self._proc.stderr.close()
            if sys.platform not in ('win32', 'cygwin'):
                # Closing stdin/stdout/stderr hangs sometimes on OS X,
                # (see __init__(), above), and anyway we don't want to hang
                # the harness if test_shell is buggy, so we wait a couple
                # seconds to give test_shell a chance to clean up, but then
                # force-kill the process if necessary.
                KILL_TIMEOUT = 3.0
                timeout = time.time() + KILL_TIMEOUT
                # poll() is not threadsafe and can throw OSError due to:
                # http://bugs.python.org/issue1731717
                while self._proc.poll() is None and time.time() < timeout:
                    time.sleep(0.1)
                # poll() is not threadsafe and can throw OSError due to:
                # http://bugs.python.org/issue1731717
                if self._proc.poll() is None:
                    _log.warning('stopping test driver timed out, '
                                 'killing it')
                    self._executive.kill_process(self._proc.pid)
