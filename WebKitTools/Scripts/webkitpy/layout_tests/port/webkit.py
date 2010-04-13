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

"""WebKit implementations of the Port interface."""

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

import webkitpy.common.system.ospath as ospath
import webkitpy.layout_tests.port.base as base
import webkitpy.layout_tests.port.server_process as server_process

_log = logging.getLogger("webkitpy.layout_tests.port.webkit")


class WebKitPort(base.Port):
    """WebKit implementation of the Port class."""

    def __init__(self, port_name=None, options=None):
        base.Port.__init__(self, port_name, options)
        self._cached_build_root = None

        # FIXME: disable pixel tests until they are run by default on the
        # build machines.
        if options and (not hasattr(options, "pixel_tests") or
           options.pixel_tests is None):
            options.pixel_tests = False

    def baseline_path(self):
        return self._webkit_baseline_path(self._name)

    def baseline_search_path(self):
        return [self._webkit_baseline_path(self._name)]

    def path_to_test_expectations_file(self):
        return os.path.join(self._webkit_baseline_path(self._name),
                            'test_expectations.txt')

    # Only needed by ports which maintain versioned test expectations (like mac-tiger vs. mac-leopard)
    def version(self):
        return ''

    def _build_driver(self):
        return not self._executive.run_command([
            self.script_path("build-dumprendertree"),
            self.flag_from_configuration(self._options.configuration),
        ], return_exit_code=True)

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not os.path.exists(driver_path):
            _log.error("DumpRenderTree was not found at %s" % driver_path)
            return False
        return True

    def check_build(self, needs_http):
        if not self._build_driver():
            return False
        if not self._check_driver():
            return False
        if self._options.pixel_tests:
            if not self.check_image_diff():
                return False
        if not self._check_port_build():
            return False
        return True

    def _check_port_build(self):
        # Ports can override this method to do additional checks.
        pass

    def check_image_diff(self, override_step=None, logging=True):
        image_diff_path = self._path_to_image_diff()
        if not os.path.exists(image_diff_path):
            _log.error("ImageDiff was not found at %s" % image_diff_path)
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
        command = [self._path_to_image_diff(), '--tolerance', '0.1']
        sp = server_process.ServerProcess(self, 'ImageDiff', command)

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
            open(diff_filename, 'w').write(output)  # FIXME: This leaks a file handle.
        elif sp.timed_out:
            _log.error("ImageDiff timed out on %s" % expected_filename)
        elif sp.crashed:
            _log.error("ImageDiff crashed")
        sp.stop()
        return result

    def results_directory(self):
        # Results are store relative to the built products to make it easy
        # to have multiple copies of webkit checked out and built.
        return self._build_path(self._options.results_directory)

    def setup_test_run(self):
        # This port doesn't require any specific configuration.
        pass

    def show_results_html_file(self, results_filename):
        uri = self.filename_to_uri(results_filename)
        # FIXME: We should open results in the version of WebKit we built.
        webbrowser.open(uri, new=1)

    def start_driver(self, image_path, options):
        return WebKitDriver(self, image_path, options)

    def test_base_platform_names(self):
        # At the moment we don't use test platform names, but we have
        # to return something.
        return ('mac', 'win')

    def _tests_for_other_platforms(self):
        raise NotImplementedError('WebKitPort._tests_for_other_platforms')
        # The original run-webkit-tests builds up a "whitelist" of tests to
        # run, and passes that to DumpRenderTree. new-run-webkit-tests assumes
        # we run *all* tests and test_expectations.txt functions as a
        # blacklist.
        # FIXME: This list could be dynamic based on platform name and
        # pushed into base.Port.
        return [
            "platform/chromium",
            "platform/gtk",
            "platform/qt",
            "platform/win",
        ]

    def _tests_for_disabled_features(self):
        # FIXME: This should use the feature detection from
        # webkitperl/features.pm to match run-webkit-tests.
        # For now we hard-code a list of features known to be disabled on
        # the Mac platform.
        disabled_feature_tests = [
            "fast/xhtmlmp",
            "http/tests/wml",
            "mathml",
            "wml",
        ]
        # FIXME: webarchive tests expect to read-write from
        # -expected.webarchive files instead of .txt files.
        # This script doesn't know how to do that yet, so pretend they're
        # just "disabled".
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

    def _skipped_file_paths(self):
        return [os.path.join(self._webkit_baseline_path(self._name),
                                                        'Skipped')]

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
        return self._name + self.version()

    def test_platform_names(self):
        return self.test_base_platform_names() + (
            'mac-tiger', 'mac-leopard', 'mac-snowleopard')

    def default_configuration(self):
        # This is a bit of a hack. This state exists in a much nicer form in
        # perl-land.
        configuration = ospath.relpath(
            self._webkit_build_directory(["--configuration"]),
            self._webkit_build_directory(["--top-level"]))
        assert(configuration == "Debug" or configuration == "Release")
        return configuration

    def _webkit_build_directory(self, args):
        args = [self.script_path("webkit-build-directory")] + args
        return self._executive.run_command(args).rstrip()

    def _build_path(self, *comps):
        if not self._cached_build_root:
            self._cached_build_root = self._webkit_build_directory([
                "--configuration",
                self.flag_from_configuration(self._options.configuration),
            ])
        return os.path.join(self._cached_build_root, *comps)

    def _path_to_driver(self):
        return self._build_path('DumpRenderTree')

    def _path_to_helper(self):
        return None

    def _path_to_image_diff(self):
        return self._build_path('ImageDiff')

    def _path_to_wdiff(self):
        # FIXME: This does not exist on a default Mac OS X Leopard install.
        return 'wdiff'

    def _path_to_apache(self):
        return '/usr/sbin/httpd'


class WebKitDriver(base.Driver):
    """WebKit implementation of the DumpRenderTree interface."""

    def __init__(self, port, image_path, driver_options):
        self._port = port
        self._driver_options = driver_options
        self._image_path = image_path

        command = []
        # Hook for injecting valgrind or other runtime instrumentation,
        # used by e.g. tools/valgrind/valgrind_tests.py.
        wrapper = os.environ.get("BROWSER_WRAPPER", None)
        if wrapper != None:
            command += [wrapper]
        if self._port._options.wrapper:
            # This split() isn't really what we want -- it incorrectly will
            # split quoted strings within the wrapper argument -- but in
            # practice it shouldn't come up and the --help output warns
            # about it anyway.
            # FIXME: Use a real shell parser.
            command += self._options.wrapper.split()

        command += [port._path_to_driver(), '-']

        if image_path:
            command.append('--pixel-tests')
        environment = os.environ
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        self._server_process = server_process.ServerProcess(self._port,
            "DumpRenderTree", command, environment)

    def poll(self):
        return self._server_process.poll()

    def restart(self):
        self._server_process.stop()
        self._server_process.start()
        return

    def returncode(self):
        return self._server_process.returncode()

    # FIXME: This function is huge.
    def run_test(self, uri, timeoutms, image_hash):
        if uri.startswith("file:///"):
            command = uri[7:]
        else:
            command = uri

        if image_hash:
            command += "'" + image_hash
        command += "\n"

        # pdb.set_trace()
        self._server_process.write(command)

        have_seen_content_type = False
        actual_image_hash = None
        output = ''
        image = ''

        timeout = int(timeoutms) / 1000.0
        deadline = time.time() + timeout
        line = self._server_process.read_line(timeout)
        while (not self._server_process.timed_out
               and not self._server_process.crashed
               and line.rstrip() != "#EOF"):
            if (line.startswith('Content-Type:') and not
                have_seen_content_type):
                have_seen_content_type = True
            else:
                output += line
            line = self._server_process.read_line(timeout)
            timeout = deadline - time.time()

        # Now read a second block of text for the optional image data
        remaining_length = -1
        HASH_HEADER = 'ActualHash: '
        LENGTH_HEADER = 'Content-Length: '
        line = self._server_process.read_line(timeout)
        while (not self._server_process.timed_out
               and not self._server_process.crashed
               and line.rstrip() != "#EOF"):
            if line.startswith(HASH_HEADER):
                actual_image_hash = line[len(HASH_HEADER):].strip()
            elif line.startswith('Content-Type:'):
                pass
            elif line.startswith(LENGTH_HEADER):
                timeout = deadline - time.time()
                content_length = int(line[len(LENGTH_HEADER):])
                image = self._server_process.read(timeout, content_length)
            timeout = deadline - time.time()
            line = self._server_process.read_line(timeout)

        if self._image_path and len(self._image_path):
            image_file = file(self._image_path, "wb")
            image_file.write(image)
            image_file.close()
        return (self._server_process.crashed,
                self._server_process.timed_out,
                actual_image_hash,
                output,
                self._server_process.error)

    def stop(self):
        if self._server_process:
            self._server_process.stop()
            self._server_process = None
