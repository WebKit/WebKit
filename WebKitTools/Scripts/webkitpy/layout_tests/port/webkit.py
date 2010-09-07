#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi <rgabor@inf.u-szeged.hu>, University of Szeged
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


from __future__ import with_statement

import codecs
import logging
import os
import re
import shutil
import signal
import sys
import time
import webbrowser
import operator
import tempfile
import shutil

from webkitpy.common.system.executive import Executive

import webkitpy.common.system.ospath as ospath
import webkitpy.layout_tests.port.base as base
import webkitpy.layout_tests.port.server_process as server_process

_log = logging.getLogger("webkitpy.layout_tests.port.webkit")


class WebKitPort(base.Port):
    """WebKit implementation of the Port class."""

    def __init__(self, port_name=None, options=None, **kwargs):
        base.Port.__init__(self, port_name, options, **kwargs)
        self._cached_build_root = None
        self._cached_apache_path = None

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
        if self._options.build and not self._build_driver():
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
        return True

    def check_image_diff(self, override_step=None, logging=True):
        image_diff_path = self._path_to_image_diff()
        if not os.path.exists(image_diff_path):
            _log.error("ImageDiff was not found at %s" % image_diff_path)
            return False
        return True

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None, tolerance=0.1):
        """Return True if the two files are different. Also write a delta
        image of the two images into |diff_filename| if it is not None."""

        # FIXME: either expose the tolerance argument as a command-line
        # parameter, or make it go away and always use exact matches.

        # Handle the case where the test didn't actually generate an image.
        actual_length = os.stat(actual_filename).st_size
        if actual_length == 0:
            if diff_filename:
                shutil.copyfile(actual_filename, expected_filename)
            return True

        sp = self._diff_image_request(expected_filename, actual_filename, tolerance)
        return self._diff_image_reply(sp, expected_filename, diff_filename)

    def _diff_image_request(self, expected_filename, actual_filename, tolerance):
        command = [self._path_to_image_diff(), '--tolerance', str(tolerance)]
        sp = server_process.ServerProcess(self, 'ImageDiff', command)

        actual_length = os.stat(actual_filename).st_size
        with open(actual_filename) as file:
            actual_file = file.read()
        expected_length = os.stat(expected_filename).st_size
        with open(expected_filename) as file:
            expected_file = file.read()
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
            with open(diff_filename, 'w') as file:
                file.write(output)
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

    def create_driver(self, image_path, options):
        return WebKitDriver(self, image_path, options, executive=self._executive)

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

    def _runtime_feature_list(self):
        """Return the supported features of DRT. If a port doesn't support
        this DRT switch, it has to override this method to return None"""
        driver_path = self._path_to_driver()
        feature_list = ' '.join(os.popen(driver_path + " --print-supported-features 2>&1").readlines())
        if "SupportedFeatures:" in feature_list:
            return feature_list
        return None

    def _supported_symbol_list(self):
        """Return the supported symbols of WebCore."""
        webcore_library_path = self._path_to_webcore_library()
        if not webcore_library_path:
            return None
        symbol_list = ' '.join(os.popen("nm " + webcore_library_path).readlines())
        return symbol_list

    def _directories_for_features(self):
        """Return the supported feature dictionary. The keys are the
        features and the values are the directories in lists."""
        directories_for_features = {
            "Accelerated Compositing": ["compositing"],
            "3D Rendering": ["animations/3d", "transforms/3d"],
        }
        return directories_for_features

    def _directories_for_symbols(self):
        """Return the supported feature dictionary. The keys are the
        symbols and the values are the directories in lists."""
        directories_for_symbol = {
            "MathMLElement": ["mathml"],
            "GraphicsLayer": ["compositing"],
            "WebCoreHas3DRendering": ["animations/3d", "transforms/3d"],
            "WebGLShader": ["fast/canvas/webgl"],
            "WMLElement": ["http/tests/wml", "fast/wml", "wml"],
            "parseWCSSInputProperty": ["fast/wcss"],
            "isXHTMLMPDocument": ["fast/xhtmlmp"],
        }
        return directories_for_symbol

    def _skipped_tests_for_unsupported_features(self):
        """Return the directories of unsupported tests. Search for the
        symbols in the symbol_list, if found add the corresponding
        directories to the skipped directory list."""
        feature_list = self._runtime_feature_list()
        directories = self._directories_for_features()

        # if DRT feature detection not supported
        if not feature_list:
            feature_list = self._supported_symbol_list()
            directories = self._directories_for_symbols()

        if not feature_list:
            return []

        skipped_directories = [directories[feature]
                              for feature in directories.keys()
                              if feature not in feature_list]
        return reduce(operator.add, skipped_directories)

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
        unsupported_feature_tests = self._skipped_tests_for_unsupported_features()
        return disabled_feature_tests + webarchive_tests + unsupported_feature_tests

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
            with codecs.open(filename, "r", "utf-8") as skipped_file:
                tests_to_skip.extend(self._tests_from_skipped_file(skipped_file))
        return tests_to_skip

    def test_expectations(self):
        # The WebKit mac port uses a combination of a test_expectations file
        # and 'Skipped' files.
        expectations_path = self.path_to_test_expectations_file()
        with codecs.open(expectations_path, "r", "utf-8") as file:
            return file.read() + self._skips()

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

    def _build_path(self, *comps):
        if not self._cached_build_root:
            self._cached_build_root = self._webkit_build_directory([
                "--configuration",
                self.flag_from_configuration(self._options.configuration),
            ])
        return os.path.join(self._cached_build_root, *comps)

    def _path_to_driver(self):
        return self._build_path('DumpRenderTree')

    def _path_to_webcore_library(self):
        return None

    def _path_to_helper(self):
        return None

    def _path_to_image_diff(self):
        return self._build_path('ImageDiff')

    def _path_to_wdiff(self):
        # FIXME: This does not exist on a default Mac OS X Leopard install.
        return 'wdiff'

    def _path_to_apache(self):
        if not self._cached_apache_path:
            # The Apache binary path can vary depending on OS and distribution
            # See http://wiki.apache.org/httpd/DistrosDefaultLayout
            for path in ["/usr/sbin/httpd", "/usr/sbin/apache2"]:
                if os.path.exists(path):
                    self._cached_apache_path = path
                    break

            if not self._cached_apache_path:
                _log.error("Could not find apache. Not installed or unknown path.")

        return self._cached_apache_path


class WebKitDriver(base.Driver):
    """WebKit implementation of the DumpRenderTree interface."""

    def __init__(self, port, image_path, driver_options, executive=Executive()):
        self._port = port
        # FIXME: driver_options is never used.
        self._image_path = image_path
        self._driver_tempdir = tempfile.mkdtemp(prefix='DumpRenderTree-')

    def __del__(self):
        shutil.rmtree(self._driver_tempdir)

    def start(self):
        command = []
        # FIXME: We should not be grabbing at self._port._options.wrapper directly.
        command += self._command_wrapper(self._port._options.wrapper)
        command += [self._port._path_to_driver(), '-']
        if self._image_path:
            command.append('--pixel-tests')
        environment = self._port.setup_environ_for_server()
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        environment['DUMPRENDERTREE_TEMP'] = self._driver_tempdir
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

        self._server_process.write(command)

        have_seen_content_type = False
        actual_image_hash = None
        output = str()  # Use a byte array for output, even though it should be UTF-8.
        image = str()

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
                # Note: Text output from DumpRenderTree is always UTF-8.
                # However, some tests (e.g. webarchives) spit out binary
                # data instead of text.  So to make things simple, we
                # always treat the output as binary.
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
            with open(self._image_path, "wb") as image_file:
                image_file.write(image)

        error_lines = self._server_process.error.splitlines()
        # FIXME: This is a hack.  It is unclear why sometimes
        # we do not get any error lines from the server_process
        # probably we are not flushing stderr.
        if error_lines and error_lines[-1] == "#EOF":
            error_lines.pop()  # Remove the expected "#EOF"
        error = "\n".join(error_lines)
        # FIXME: This seems like the wrong section of code to be doing
        # this reset in.
        self._server_process.error = ""
        return (self._server_process.crashed,
                self._server_process.timed_out,
                actual_image_hash,
                output,
                error)

    def stop(self):
        if self._server_process:
            self._server_process.stop()
            self._server_process = None
