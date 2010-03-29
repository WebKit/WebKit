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

import logging
import os
import pdb
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
import webbrowser

import base
import server_process

import webkitpy
import webkitpy.common.system.executive as executive

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
        build_drt_command = [self.script_path("build-dumprendertree")]
        if self._options.target == "Debug":
            build_drt_command.append('--debug')
        if executive.run_command(build_drt_command, return_exit_code=True):
            return False

        driver_path = self._path_to_driver()
        if not os.path.exists(driver_path):
            _log.error("DumpRenderTree was not found at %s" % driver_path)
            return False

        image_diff_path = self._path_to_image_diff()
        if not os.path.exists(image_diff_path):
            _log.error("ImageDiff was not found at %s" % image_diff_path)
            return False

        java_tests_path = os.path.join(self.layout_tests_dir(), "java")
        build_java = ["/usr/bin/make", "-C", java_tests_path]
        if executive.run_command(build_java, return_exit_code=True):
            _log.error("Failed to build Java support files: %s" % build_java)
            return False

        return True

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None):
        """Return True if the two files are different. Also write a delta
        image of the two images into |diff_filename| if it is not None."""

        # Handle the case where the test didn't actually generate an image.
        actual_length = os.stat(actual_filename).st_size
        if actual_length == 0:
            if diff_filename:
                shutil.copyfile(actual_filename, expected_filename)
            return True

        sp = self._diff_image_request(expected_filename, actual_filename)
        return self._diff_image_reply(sp, expected_filename, diff_filename)

    def _diff_image_request(self, expected_filename, actual_filename):
        # FIXME: either expose the tolerance argument as a command-line
        # parameter, or make it go away and aways use exact matches.
        cmd = [self._path_to_image_diff(), '--tolerance', '0.1']
        sp = server_process.ServerProcess(self, 'ImageDiff', cmd)

        actual_length = os.stat(actual_filename).st_size
        actual_file = open(actual_filename).read()
        expected_length = os.stat(expected_filename).st_size
        expected_file = open(expected_filename).read()
        sp.write('Content-Length: %d\n%sContent-Length: %d\n%s' %
                 (actual_length, actual_file, expected_length, expected_file))

        return sp

    def _diff_image_reply(self, sp, expected_filename, diff_filename):
        timeout = 2.0
        deadline = time.time() + timeout
        output = sp.read_line(timeout)
        while not sp.timed_out and not sp.crashed and output:
            if output.startswith('Content-Length'):
                m = re.match('Content-Length: (\d+)', output)
                content_length = int(m.group(1))
                timeout = deadline - time.time()
                output = sp.read(timeout, content_length)
                break
            elif output.startswith('diff'):
                break
            else:
                timeout = deadline - time.time()
                output = sp.read_line(deadline)

        result = True
        if output.startswith('diff'):
            m = re.match('diff: (.+)% (passed|failed)', output)
            if m.group(2) == 'passed':
                result = False
        elif output and diff_filename:
            open(diff_filename, 'w').write(output)
        elif sp.timed_out:
            _log.error("ImageDiff timed out on %s" % expected_filename)
        elif sp.crashed:
            _log.error("ImageDiff crashed")
        sp.stop()
        return result

    def num_cores(self):
        return int(os.popen2("sysctl -n hw.ncpu")[1].read())

    def path_to_test_expectations_file(self):
        return self.path_from_webkit_base('LayoutTests', 'platform',
           'mac', 'test_expectations.txt')

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
        # The WebKit mac port uses a combination of a test_expectations file
        # and 'Skipped' files.
        expectations_file = self.path_to_test_expectations_file()
        expectations = file(expectations_file, "r").read()
        return expectations + self._skips()

    def _skips(self):
        # Each Skipped file contains a list of files
        # or directories to be skipped during the test run. The total list
        # of tests to skipped is given by the contents of the generic
        # Skipped file found in platform/X plus a version-specific file
        # found in platform/X-version. Duplicate entries are allowed.
        # This routine reads those files and turns contents into the
        # format expected by test_expectations.

        # Use a set to allow duplicates
        tests_to_skip = set(self._expectations_from_skipped_files())

        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._tests_for_disabled_features())
        skip_lines = map(lambda test_path: "BUG_SKIPPED SKIP : %s = FAIL" %
                                test_path, tests_to_skip)
        return "\n".join(skip_lines)

    def test_platform_name(self):
        return 'mac' + self.version()

    def test_platform_names(self):
        return ('mac', 'mac-tiger', 'mac-leopard', 'mac-snowleopard')

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
        return self._build_path('ImageDiff')

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

        cmd += ['arch', '-i386', port._path_to_driver(), '-']

        if image_path:
            cmd.append('--pixel-tests')
        env = os.environ
        env['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        self._sproc = server_process.ServerProcess(self._port,
            "DumpRenderTree", cmd, env)

    def poll(self):
        return self._sproc.poll()

    def restart(self):
        self._sproc.stop()
        self._sproc.start()
        return

    def returncode(self):
        return self._proc.returncode()

    def run_test(self, uri, timeoutms, image_hash):
        if uri.startswith("file:///"):
            cmd = uri[7:]
        else:
            cmd = uri

        if image_hash:
            cmd += "'" + image_hash
        cmd += "\n"

        # pdb.set_trace()
        sp = self._sproc
        sp.write(cmd)

        have_seen_content_type = False
        actual_image_hash = None
        output = ''
        image = ''

        timeout = int(timeoutms) / 1000.0
        deadline = time.time() + timeout
        line = sp.read_line(timeout)
        while not sp.timed_out and not sp.crashed and line.rstrip() != "#EOF":
            if (line.startswith('Content-Type:') and not
                have_seen_content_type):
                have_seen_content_type = True
            else:
                output += line
            line = sp.read_line(timeout)
            timeout = deadline - time.time()

        # Now read a second block of text for the optional image data
        remaining_length = -1
        HASH_HEADER = 'ActualHash: '
        LENGTH_HEADER = 'Content-Length: '
        line = sp.read_line(timeout)
        while not sp.timed_out and not sp.crashed and line.rstrip() != "#EOF":
            if line.startswith(HASH_HEADER):
                actual_image_hash = line[len(HASH_HEADER):].strip()
            elif line.startswith('Content-Type:'):
                pass
            elif line.startswith(LENGTH_HEADER):
                timeout = deadline - time.time()
                content_length = int(line[len(LENGTH_HEADER):])
                image = sp.read(timeout, content_length)
            timeout = deadline - time.time()
            line = sp.read_line(timeout)

        if self._image_path and len(self._image_path):
            image_file = file(self._image_path, "wb")
            image_file.write(image)
            image_file.close()
        return (sp.crashed, sp.timed_out, actual_image_hash, output, sp.error)

    def stop(self):
        if self._sproc:
            self._sproc.stop()
            self._sproc = None
