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

import base64
import logging
import operator
import os
import re
import signal
import sys
import time
import webbrowser

from webkitpy.common.system import ospath
from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.port import server_process

_log = logging.getLogger("webkitpy.layout_tests.port.webkit")


class WebKitPort(base.Port):
    """WebKit implementation of the Port class."""

    def __init__(self, **kwargs):
        base.Port.__init__(self, **kwargs)
        self._cached_apache_path = None

        # FIXME: disable pixel tests until they are run by default on the
        # build machines.
        if not hasattr(self._options, "pixel_tests") or self._options.pixel_tests == None:
            self._options.pixel_tests = False

    def baseline_path(self):
        return self._webkit_baseline_path(self._name)

    def baseline_search_path(self):
        return [self._webkit_baseline_path(self._name)]

    def path_to_test_expectations_file(self):
        return self._filesystem.join(self._webkit_baseline_path(self._name),
                                     'test_expectations.txt')

    def _build_driver(self):
        configuration = self.get_option('configuration')
        return self._config.build_dumprendertree(configuration)

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not self._filesystem.exists(driver_path):
            _log.error("DumpRenderTree was not found at %s" % driver_path)
            return False
        return True

    def check_build(self, needs_http):
        if self.get_option('build') and not self._build_driver():
            return False
        if not self._check_driver():
            return False
        if self.get_option('pixel_tests'):
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
        if not self._filesystem.exists(image_diff_path):
            _log.error("ImageDiff was not found at %s" % image_diff_path)
            return False
        return True

    def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None):
        """Return True if the two files are different. Also write a delta
        image of the two images into |diff_filename| if it is not None."""

        # Handle the case where the test didn't actually generate an image.
        # FIXME: need unit tests for this.
        if not actual_contents and not expected_contents:
            return False
        if not actual_contents or not expected_contents:
            return True

        sp = self._diff_image_request(expected_contents, actual_contents)
        return self._diff_image_reply(sp, diff_filename)

    def _diff_image_request(self, expected_contents, actual_contents):
        # FIXME: There needs to be a more sane way of handling default
        # values for options so that you can distinguish between a default
        # value of None and a default value that wasn't set.
        if self.get_option('tolerance') is not None:
            tolerance = self.get_option('tolerance')
        else:
            tolerance = 0.1
        command = [self._path_to_image_diff(), '--tolerance', str(tolerance)]
        sp = server_process.ServerProcess(self, 'ImageDiff', command)

        sp.write('Content-Length: %d\n%sContent-Length: %d\n%s' %
                 (len(actual_contents), actual_contents,
                  len(expected_contents), expected_contents))

        return sp

    def _diff_image_reply(self, sp, diff_filename):
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
            self._filesystem.write_binary_file(diff_filename, output)
        elif sp.timed_out:
            _log.error("ImageDiff timed out")
        elif sp.crashed:
            _log.error("ImageDiff crashed")
        sp.stop()
        return result

    def default_results_directory(self):
        # Results are store relative to the built products to make it easy
        # to have multiple copies of webkit checked out and built.
        return self._build_path('layout-test-results')

    def setup_test_run(self):
        # This port doesn't require any specific configuration.
        pass

    def create_driver(self, worker_number):
        return WebKitDriver(self, worker_number)

    def _tests_for_other_platforms(self):
        # By default we will skip any directory under LayoutTests/platform
        # that isn't in our baseline search path (this mirrors what
        # old-run-webkit-tests does in findTestsToRun()).
        # Note this returns LayoutTests/platform/*, not platform/*/*.
        entries = self._filesystem.glob(self._webkit_baseline_path('*'))
        dirs_to_skip = []
        for entry in entries:
            if self._filesystem.isdir(entry) and not entry in self.baseline_search_path():
                basename = self._filesystem.basename(entry)
                dirs_to_skip.append('platform/%s' % basename)
        return dirs_to_skip

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
            "WebGLShader": ["fast/canvas/webgl", "compositing/webgl", "http/tests/canvas/webgl"],
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

    def _tests_from_skipped_file_contents(self, skipped_file_contents):
        tests_to_skip = []
        for line in skipped_file_contents.split('\n'):
            line = line.strip()
            if line.startswith('#') or not len(line):
                continue
            tests_to_skip.append(line)
        return tests_to_skip

    def _skipped_file_paths(self):
        return [self._filesystem.join(self._webkit_baseline_path(self._name), 'Skipped')]

    def _expectations_from_skipped_files(self):
        tests_to_skip = []
        for filename in self._skipped_file_paths():
            if not self._filesystem.exists(filename):
                _log.warn("Failed to open Skipped file: %s" % filename)
                continue
            skipped_file_contents = self._filesystem.read_text_file(filename)
            tests_to_skip.extend(self._tests_from_skipped_file_contents(skipped_file_contents))
        return tests_to_skip

    def test_expectations(self):
        # The WebKit mac port uses a combination of a test_expectations file
        # and 'Skipped' files.
        expectations_path = self.path_to_test_expectations_file()
        return self._filesystem.read_text_file(expectations_path) + self._skips()

    def _skips(self):
        # Each Skipped file contains a list of files
        # or directories to be skipped during the test run. The total list
        # of tests to skipped is given by the contents of the generic
        # Skipped file found in platform/X plus a version-specific file
        # found in platform/X-version. Duplicate entries are allowed.
        # This routine reads those files and turns contents into the
        # format expected by test_expectations.

        tests_to_skip = self.skipped_layout_tests()
        skip_lines = map(lambda test_path: "BUG_SKIPPED SKIP : %s = FAIL" %
                                test_path, tests_to_skip)
        return "\n".join(skip_lines)

    def skipped_layout_tests(self):
        # Use a set to allow duplicates
        tests_to_skip = set(self._expectations_from_skipped_files())
        tests_to_skip.update(self._tests_for_other_platforms())
        tests_to_skip.update(self._tests_for_disabled_features())
        return tests_to_skip

    def _build_path(self, *comps):
        return self._filesystem.join(self._config.build_directory(
            self.get_option('configuration')), *comps)

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
                if self._filesystem.exists(path):
                    self._cached_apache_path = path
                    break

            if not self._cached_apache_path:
                _log.error("Could not find apache. Not installed or unknown path.")

        return self._cached_apache_path


class WebKitDriver(base.Driver):
    """WebKit implementation of the DumpRenderTree interface."""

    def __init__(self, port, worker_number):
        self._worker_number = worker_number
        self._port = port
        self._driver_tempdir = port._filesystem.mkdtemp(prefix='DumpRenderTree-')

    def __del__(self):
        self._port._filesystem.rmtree(str(self._driver_tempdir))

    def cmd_line(self):
        cmd = self._command_wrapper(self._port.get_option('wrapper'))
        cmd.append(self._port._path_to_driver())
        if self._port.get_option('pixel_tests'):
            cmd.append('--pixel-tests')
        cmd.extend(self._port.get_option('additional_drt_flag', []))
        cmd.append('-')
        return cmd

    def start(self):
        environment = self._port.setup_environ_for_server()
        environment['DYLD_FRAMEWORK_PATH'] = self._port._build_path()
        environment['DUMPRENDERTREE_TEMP'] = str(self._driver_tempdir)
        self._server_process = server_process.ServerProcess(self._port,
            "DumpRenderTree", self.cmd_line(), environment)

    def poll(self):
        return self._server_process.poll()

    def restart(self):
        self._server_process.stop()
        self._server_process.start()
        return

    # FIXME: This function is huge.
    def run_test(self, driver_input):
        uri = self._port.filename_to_uri(driver_input.filename)
        if uri.startswith("file:///"):
            command = uri[7:]
        else:
            command = uri

        if driver_input.image_hash:
            command += "'" + driver_input.image_hash
        command += "\n"

        start_time = time.time()
        self._server_process.write(command)

        text = None
        image = None
        actual_image_hash = None
        audio = None
        deadline = time.time() + int(driver_input.timeout) / 1000.0

        # First block is either text or audio
        block = self._read_block(deadline)
        if block.content_type == 'audio/wav':
            audio = block.decoded_content
        else:
            text = block.decoded_content

        # Now read an optional second block of image data
        block = self._read_block(deadline)
        if block.content and block.content_type == 'image/png':
            image = block.decoded_content
            actual_image_hash = block.content_hash

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
        return base.DriverOutput(text, image, actual_image_hash, audio,
            crash=self._server_process.crashed, test_time=time.time() - start_time,
            timeout=self._server_process.timed_out, error=error)

    def _read_block(self, deadline):
        LENGTH_HEADER = 'Content-Length: '
        HASH_HEADER = 'ActualHash: '
        TYPE_HEADER = 'Content-Type: '
        ENCODING_HEADER = 'Content-Transfer-Encoding: '
        content_type = None
        encoding = None
        content_hash = None
        content_length = None

        # Content is treated as binary data even though the text output
        # is usually UTF-8.
        content = ''
        timeout = deadline - time.time()
        line = self._server_process.read_line(timeout)
        while (not self._server_process.timed_out
               and not self._server_process.crashed
               and line.rstrip() != "#EOF"):
            if line.startswith(TYPE_HEADER) and content_type is None:
                content_type = line.split()[1]
            elif line.startswith(ENCODING_HEADER) and encoding is None:
                encoding = line.split()[1]
            elif line.startswith(LENGTH_HEADER) and content_length is None:
                timeout = deadline - time.time()
                content_length = int(line[len(LENGTH_HEADER):])
                # FIXME: Technically there should probably be another blank
                # line here, but DRT doesn't write one.
                content = self._server_process.read(timeout, content_length)
            elif line.startswith(HASH_HEADER):
                content_hash = line.split()[1]
            else:
                content += line
            line = self._server_process.read_line(timeout)
            timeout = deadline - time.time()
        return ContentBlock(content_type, encoding, content_hash, content)

    def stop(self):
        if self._server_process:
            self._server_process.stop()
            self._server_process = None


class ContentBlock(object):
    def __init__(self, content_type, encoding, content_hash, content):
        self.content_type = content_type
        self.encoding = encoding
        self.content_hash = content_hash
        self.content = content
        if self.encoding == 'base64':
            self.decoded_content = base64.b64decode(content)
        else:
            self.decoded_content = content
