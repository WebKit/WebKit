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

import logging
import os
import shutil
import signal
import subprocess
import sys
import time

import base
import http_server
import websocket_server


class ChromiumPort(base.Port):
    """Abstract base class for Chromium implementations of the Port class."""

    def __init__(self, port_name=None, options=None):
        base.Port.__init__(self, port_name, options)
        self._chromium_base_dir = None

    def baseline_path(self):
        return self._chromium_baseline_path(self._name)

    def check_sys_deps(self):
        result = True
        test_shell_binary_path = self._path_to_driver()
        if os.path.exists(test_shell_binary_path):
            proc = subprocess.Popen([test_shell_binary_path,
                                     '--check-layout-test-sys-deps'])
            if proc.wait() != 0:
                logging.error("Aborting because system dependencies check "
                              "failed.")
                logging.error("To override, invoke with --nocheck-sys-deps")
                result = False
        else:
            logging.error('test driver is not found at %s' %
                          test_shell_binary_path)
            result = False

        image_diff_path = self._path_to_image_diff()
        if (not os.path.exists(image_diff_path) and not
            self._options.no_pixel_tests):
            logging.error('image diff not found at %s' % image_diff_path)
            logging.error("To override, invoke with --no-pixel-tests")
            result = False

        return result

    def compare_text(self, actual_text, expected_text):
        return actual_text != expected_text

    def path_from_chromium_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        Chromium source tree and the list of path components in |*comps|."""
        if not self._chromium_base_dir:
            abspath = os.path.abspath(__file__)
            self._chromium_base_dir = abspath[0:abspath.find('third_party')]
        return os.path.join(self._chromium_base_dir, *comps)

    def results_directory(self):
        return self.path_from_chromium_base('webkit', self._options.target,
                                            self._options.results_directory)

    def setup_test_run(self):
        # Delete the disk cache if any to ensure a clean test run.
        test_shell_binary_path = self._path_to_driver()
        cachedir = os.path.split(test_shell_binary_path)[0]
        cachedir = os.path.join(cachedir, "cache")
        if os.path.exists(cachedir):
            shutil.rmtree(cachedir)

    def show_results_html_file(self, results_filename):
        subprocess.Popen([self._path_to_driver(),
                          self.filename_to_uri(results_filename)])

    def start_driver(self, image_path, options):
        """Starts a new Driver and returns a handle to it."""
        return ChromiumDriver(self, image_path, options)

    def start_helper(self):
        helper_path = self._path_to_helper()
        if helper_path:
            logging.debug("Starting layout helper %s" % helper_path)
            self._helper = subprocess.Popen([helper_path],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=None)
            is_ready = self._helper.stdout.readline()
            if not is_ready.startswith('ready'):
                logging.error("layout_test_helper failed to be ready")

    def stop_helper(self):
        if self._helper:
            logging.debug("Stopping layout test helper")
            self._helper.stdin.write("x\n")
            self._helper.stdin.close()
            self._helper.wait()

    def test_base_platform_names(self):
        return ('linux', 'mac', 'win')

    def test_expectations(self, options=None):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        expectations_file = self.path_from_chromium_base('webkit', 'tools',
            'layout_tests', 'test_expectations.txt')
        return file(expectations_file, "r").read()

    def test_platform_names(self):
        return self.test_base_platform_names() + ('win-xp',
            'win-vista', 'win-7')

    #
    # PROTECTED METHODS
    #
    # These routines should only be called by other methods in this file
    # or any subclasses.
    #

    def _chromium_baseline_path(self, platform):
        if platform is None:
            platform = self.name()
        return self.path_from_chromium_base('webkit', 'data', 'layout_tests',
            'platform', platform, 'LayoutTests')


class ChromiumDriver(base.Driver):
    """Abstract interface for the DumpRenderTree interface."""

    def __init__(self, port, image_path, options):
        self._port = port
        self._options = options
        self._target = port._options.target
        self._image_path = image_path

        cmd = []
        # Hook for injecting valgrind or other runtime instrumentation,
        # used by e.g. tools/valgrind/valgrind_tests.py.
        wrapper = os.environ.get("BROWSER_WRAPPER", None)
        if wrapper != None:
            cmd += [wrapper]
        if self._port._options.wrapper:
            # This split() isn't really what we want -- it incorrectly will
            # split quoted strings within the wrapper argument -- but in
            # practice it shouldn't come up and the --help output warns
            # about it anyway.
            cmd += self._options.wrapper.split()
        cmd += [port._path_to_driver(), '--layout-tests']
        if options:
            cmd += options
        self._proc = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT)

    def poll(self):
        return self._proc.poll()

    def returncode(self):
        return self._proc.returncode

    def run_test(self, uri, timeoutms, checksum):
        output = []
        error = []
        crash = False
        timeout = False
        actual_uri = None
        actual_checksum = None

        start_time = time.time()
        cmd = uri
        if timeoutms:
            cmd += ' ' + str(timeoutms)
        if checksum:
            cmd += ' ' + checksum
        cmd += "\n"

        self._proc.stdin.write(cmd)
        line = self._proc.stdout.readline()
        while line.rstrip() != "#EOF":
            # Make sure we haven't crashed.
            if line == '' and self.poll() is not None:
                # This is hex code 0xc000001d, which is used for abrupt
                # termination. This happens if we hit ctrl+c from the prompt
                # and we happen to be waiting on the test_shell.
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
                    logging.fatal("Test got out of sync:\n|%s|\n|%s|" %
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

            line = self._proc.stdout.readline()

        return (crash, timeout, actual_checksum, ''.join(output),
                ''.join(error))

    def stop(self):
        if self._proc:
            self._proc.stdin.close()
            self._proc.stdout.close()
            if self._proc.stderr:
                self._proc.stderr.close()
            if (sys.platform not in ('win32', 'cygwin') and
                not self._proc.poll()):
                # Closing stdin/stdout/stderr hangs sometimes on OS X.
                null = open(os.devnull, "w")
                subprocess.Popen(["kill", "-9",
                                 str(self._proc.pid)], stderr=null)
                null.close()
