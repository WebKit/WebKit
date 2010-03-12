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

import fcntl
import logging
import os
import pdb
import platform
import select
import signal
import subprocess
import sys
import time
import webbrowser

import base

import webkitpy
from webkitpy import executive

_log = logging.getLogger("webkitpy.layout_tests.port.mac")


class MacPort(base.Port):
    """WebKit Mac implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        if port_name is None:
            port_name = 'mac' + self.version()
        base.Port.__init__(self, port_name, options)
        self._cached_build_root = None

    def baseline_path(self):
        return self._webkit_baseline_path(self._name)

    def baseline_search_path(self):
        dirs = []
        if self._name == 'mac-tiger':
            dirs.append(self._webkit_baseline_path(self._name))
        if self._name in ('mac-tiger', 'mac-leopard'):
            dirs.append(self._webkit_baseline_path('mac-leopard'))
        if self._name in ('mac-tiger', 'mac-leopard', 'mac-snowleopard'):
            dirs.append(self._webkit_baseline_path('mac-snowleopard'))
        dirs.append(self._webkit_baseline_path('mac'))
        return dirs

    def check_build(self, needs_http):
        if executive.run_command([self.script_path("build-dumprendertree")],
                                 return_exit_code=True):
            return False

        driver_path = self._path_to_driver()
        if not os.path.exists(driver_path):
            _log.error("DumpRenderTree was not found at %s" % driver_path)
            return False

        # This should also validate that the ImageDiff path is valid
        # (once this script knows how to use ImageDiff).
        # https://bugs.webkit.org/show_bug.cgi?id=34826
        return True

    def num_cores(self):
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())

    def results_directory(self):
        return ('/tmp/run-chromium-webkit-tests-' +
                self._options.results_directory)

    def setup_test_run(self):
        # This port doesn't require any specific configuration.
        pass

    def show_results_html_file(self, results_filename):
        uri = self.filename_to_uri(results_filename)
        webbrowser.open(uri, new=1)

    def start_driver(self, image_path, options):
        """Starts a new Driver and returns a handle to it."""
        return MacDriver(self, image_path, options)

    def test_base_platform_names(self):
        # At the moment we don't use test platform names, but we have
        # to return something.
        return ('mac',)

    def _skipped_file_paths(self):
        # FIXME: This method will need to be made work for non-mac platforms and moved into base.Port.
        skipped_files = []
        if self._name in ('mac-tiger', 'mac-leopard', 'mac-snowleopard'):
            skipped_files.append(os.path.join(
                self._webkit_baseline_path(self._name), 'Skipped'))
        skipped_files.append(os.path.join(self._webkit_baseline_path('mac'),
                                          'Skipped'))
        return skipped_files

    def _tests_for_other_platforms(self):
        # The original run-webkit-tests builds up a "whitelist" of tests to run, and passes that to DumpRenderTree.
        # run-chromium-webkit-tests assumes we run *all* tests and test_expectations.txt functions as a blacklist.
        # FIXME: This list could be dynamic based on platform name and pushed into base.Port.
        return [
            "platform/chromium",
            "platform/gtk",
            "platform/qt",
            "platform/win",
        ]

    def _tests_for_disabled_features(self):
        # FIXME: This should use the feature detection from webkitperl/features.pm to match run-webkit-tests.
        # For now we hard-code a list of features known to be disabled on the Mac platform.
        disabled_feature_tests = [
            "fast/xhtmlmp",
            "http/tests/wml",
            "mathml",
            "wml",
        ]
        # FIXME: webarchive tests expect to read-write from -expected.webarchive files instead of .txt files.
        # This script doesn't know how to do that yet, so pretend they're just "disabled".
        webarchive_tests = [
            "webarchive",
            "svg/webarchive",
            "http/tests/webarchive",
            "svg/custom/image-with-prefix-in-webarchive.svg",
        ]
        return disabled_feature_tests + webarchive_tests

    def _tests_from_skipped_file(self, skipped_file):
        tests_to_skip = []
        for line in skipped_file.readlines():
            line = line.strip()
            if line.startswith('#') or not len(line):
                continue
            tests_to_skip.append(line)
        return tests_to_skip

    def _expectations_from_skipped_files(self):
        tests_to_skip = []
        for filename in self._skipped_file_paths():
            if not os.path.exists(filename):
                _log.warn("Failed to open Skipped file: %s" % filename)
                continue
            skipped_file = file(filename)
            tests_to_skip.extend(self._tests_from_skipped_file(skipped_file))
            skipped_file.close()
        return tests_to_skip

    def test_expectations(self):
        # The WebKit mac port uses 'Skipped' files at the moment. Each
        # file contains a list of files or directories to be skipped during
        # the test run. The total list of tests to skipped is given by the
        # contents of the generic Skipped file found in platform/X plus
        # a version-specific file found in platform/X-version. Duplicate
        # entries are allowed. This routine reads those files and turns
        # contents into the format expected by test_expectations.
        tests_to_skip = set(self._expectations_from_skipped_files()) # Use a set to allow duplicates
        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._tests_for_disabled_features())
        expectations = map(lambda test_path: "BUG_SKIPPED SKIP : %s = FAIL" % test_path, tests_to_skip)
        return "\n".join(expectations)

    def test_platform_name(self):
        # At the moment we don't use test platform names, but we have
        # to return something.
        return 'mac'

    def test_platform_names(self):
        # At the moment we don't use test platform names, but we have
        # to return something.
        return ('mac',)

    def version(self):
        os_version_string = platform.mac_ver()[0]  # e.g. "10.5.6"
        if not os_version_string:
            return '-leopard'
        release_version = int(os_version_string.split('.')[1])
        if release_version == 4:
            return '-tiger'
        elif release_version == 5:
            return '-leopard'
        elif release_version == 6:
            return '-snowleopard'
        return ''

    #
    # PROTECTED METHODS
    #

    def _build_path(self, *comps):
        if not self._cached_build_root:
            self._cached_build_root = executive.run_command([self.script_path("webkit-build-directory"), "--top-level"]).rstrip()
        return os.path.join(self._cached_build_root, self._options.target, *comps)

    def _kill_process(self, pid):
        """Forcefully kill the process.

        Args:
        pid: The id of the process to be killed.
        """
        os.kill(pid, signal.SIGKILL)

    def _kill_all_process(self, process_name):
        # On Mac OS X 10.6, killall has a new constraint: -SIGNALNAME or
        # -SIGNALNUMBER must come first.  Example problem:
        #   $ killall -u $USER -TERM lighttpd
        #   killall: illegal option -- T
        # Use of the earlier -TERM placement is just fine on 10.5.
        null = open(os.devnull)
        subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'),
                        process_name], stderr=null)
        null.close()

    def _path_to_apache(self):
        return '/usr/sbin/httpd'

    def _path_to_apache_config_file(self):
        return os.path.join(self.layout_tests_dir(), 'http', 'conf',
                            'apache2-httpd.conf')

    def _path_to_driver(self):
        return self._build_path('DumpRenderTree')

    def _path_to_helper(self):
        return None

    def _path_to_image_diff(self):
        return self._build_path('image_diff') # FIXME: This is wrong and should be "ImageDiff", but having the correct path causes other parts of the script to hang.

    def _path_to_wdiff(self):
        return 'wdiff' # FIXME: This does not exist on a default Mac OS X Leopard install.

    def _shut_down_http_server(self, server_pid):
        """Shut down the lighttpd web server. Blocks until it's fully
        shut down.

        Args:
            server_pid: The process ID of the running server.
        """
        # server_pid is not set when "http_server.py stop" is run manually.
        if server_pid is None:
            # TODO(mmoss) This isn't ideal, since it could conflict with
            # lighttpd processes not started by http_server.py,
            # but good enough for now.
            self._kill_all_process('httpd')
        else:
            try:
                os.kill(server_pid, signal.SIGTERM)
                # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
            except OSError:
                # Sometimes we get a bad PID (e.g. from a stale httpd.pid
                # file), so if kill fails on the given PID, just try to
                # 'killall' web servers.
                self._shut_down_http_server(None)


class MacDriver(base.Driver):
    """implementation of the DumpRenderTree interface."""

    def __init__(self, port, image_path, driver_options):
        self._port = port
        self._driver_options = driver_options
        self._target = port._options.target
        self._image_path = image_path
        self._stdout_fd = None
        self._cmd = None
        self._env = None
        self._proc = None
        self._read_buffer = ''

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
        # FIXME: Using arch here masks any possible file-not-found errors from a non-existant driver executable.
        cmd += ['arch', '-i386', port._path_to_driver(), '-']

        # FIXME: This is a hack around our lack of ImageDiff support for now.
        if not self._port._options.no_pixel_tests:
            _log.warn("This port does not yet support pixel tests.")
            self._port._options.no_pixel_tests = True
            #cmd.append('--pixel-tests')

        #if driver_options:
        #    cmd += driver_options
        env = os.environ
        env['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        self._cmd = cmd
        self._env = env
        self.restart()

    def poll(self):
        return self._proc.poll()

    def restart(self):
        self.stop()
        # We need to pass close_fds=True to work around Python bug #2320
        # (otherwise we can hang when we kill test_shell when we are running
        # multiple threads). See http://bugs.python.org/issue2320 .
        self._proc = subprocess.Popen(self._cmd, stdin=subprocess.PIPE,
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE,
                                      env=self._env,
                                      close_fds=True)

    def returncode(self):
        return self._proc.returncode

    def run_test(self, uri, timeoutms, image_hash):
        output = []
        error = []
        image = ''
        crash = False
        timeout = False
        actual_uri = None
        actual_image_hash = None

        if uri.startswith("file:///"):
            cmd = uri[7:]
        else:
            cmd = uri

        if image_hash:
            cmd += "'" + image_hash
        cmd += "\n"

        self._proc.stdin.write(cmd)
        self._stdout_fd = self._proc.stdout.fileno()
        fl = fcntl.fcntl(self._stdout_fd, fcntl.F_GETFL)
        fcntl.fcntl(self._stdout_fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

        stop_time = time.time() + (int(timeoutms) / 1000.0)
        resp = ''
        (timeout, line) = self._read_line(timeout, stop_time)
        resp += line
        have_seen_content_type = False
        while not timeout and line.rstrip() != "#EOF":
            # Make sure we haven't crashed.
            if line == '' and self.poll() is not None:
                # This is hex code 0xc000001d, which is used for abrupt
                # termination. This happens if we hit ctrl+c from the prompt
                # and we happen to be waiting on the test_shell.
                # sdoyon: Not sure for which OS and in what circumstances the
                # above code is valid. What works for me under Linux to detect
                # ctrl+c is for the subprocess returncode to be negative
                # SIGINT. And that agrees with the subprocess documentation.
                if (-1073741510 == self.returncode() or
                    - signal.SIGINT == self.returncode()):
                    raise KeyboardInterrupt
                crash = True
                break

            elif (line.startswith('Content-Type:') and not
                  have_seen_content_type):
                have_seen_content_type = True
                pass
            else:
                output.append(line)

            (timeout, line) = self._read_line(timeout, stop_time)
            resp += line

        # Now read a second block of text for the optional image data
        image_length = 0
        (timeout, line) = self._read_line(timeout, stop_time)
        resp += line
        HASH_HEADER = 'ActualHash: '
        LENGTH_HEADER = 'Content-Length: '
        while not timeout and not crash and line.rstrip() != "#EOF":
            if line == '' and self.poll() is not None:
                if (-1073741510 == self.returncode() or
                    - signal.SIGINT == self.returncode()):
                    raise KeyboardInterrupt
                crash = True
                break
            elif line.startswith(HASH_HEADER):
                actual_image_hash = line[len(HASH_HEADER):].strip()
            elif line.startswith('Content-Type:'):
                pass
            elif line.startswith(LENGTH_HEADER):
                image_length = int(line[len(LENGTH_HEADER):])
            elif image_length:
                image += line

            (timeout, line) = self._read_line(timeout, stop_time, image_length)
            resp += line

        if timeout:
            self.restart()

        if self._image_path and len(self._image_path):
            image_file = file(self._image_path, "wb")
            image_file.write(image)
            image_file.close()

        return (crash, timeout, actual_image_hash,
                ''.join(output), ''.join(error))

    def stop(self):
        if self._proc:
            self._proc.stdin.close()
            self._proc.stdout.close()
            if self._proc.stderr:
                self._proc.stderr.close()
            if sys.platform not in ('win32', 'cygwin'):
                # Closing stdin/stdout/stderr hangs sometimes on OS X,
                # (see restart(), above), and anyway we don't want to hang
                # the harness if test_shell is buggy, so we wait a couple
                # seconds to give test_shell a chance to clean up, but then
                # force-kill the process if necessary.
                KILL_TIMEOUT = 3.0
                timeout = time.time() + KILL_TIMEOUT
                while self._proc.poll() is None and time.time() < timeout:
                    time.sleep(0.1)
                if self._proc.poll() is None:
                    _log.warning('stopping test driver timed out, '
                                 'killing it')
                    null = open(os.devnull, "w")
                    subprocess.Popen(["kill", "-9",
                                     str(self._proc.pid)], stderr=null)
                    null.close()

    def _read_line(self, timeout, stop_time, image_length=0):
        now = time.time()
        read_fds = []

        # first check to see if we have a line already read or if we've
        # read the entire image
        if image_length and len(self._read_buffer) >= image_length:
            out = self._read_buffer[0:image_length]
            self._read_buffer = self._read_buffer[image_length:]
            return (timeout, out)

        idx = self._read_buffer.find('\n')
        if not image_length and idx != -1:
            out = self._read_buffer[0:idx + 1]
            self._read_buffer = self._read_buffer[idx + 1:]
            return (timeout, out)

        # If we've timed out, return just what we have, if anything
        if timeout or now >= stop_time:
            out = self._read_buffer
            self._read_buffer = ''
            return (True, out)

        (read_fds, write_fds, err_fds) = select.select(
            [self._stdout_fd], [], [], stop_time - now)
        try:
            if timeout or len(read_fds) == 1:
                self._read_buffer += self._proc.stdout.read()
        except IOError, e:
            read = []
        return self._read_line(timeout, stop_time)
