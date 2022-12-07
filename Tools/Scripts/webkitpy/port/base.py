# -*- coding: utf-8 -*-
# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#
# Copyright (c) 2017 Michał Bultrowicz (https://gist.github.com/butla/2d9a4c0f35ea47b7452156c96a4e7b12)
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Abstract base class of Port-specific entry points for the layout tests
test infrastructure (the Port and Driver classes)."""

import argparse
import difflib
import logging
import os
import optparse
import re
import socket
import sys
import time

from collections import OrderedDict
from webkitcorepy import string_utils, decorators
from webkitscmpy import local

from webkitpy.common import read_checksum_from_png
from webkitpy.common.memoized import memoized
from webkitpy.common.prettypatch import PrettyPatch
from webkitpy.common.system import path, pemfile
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.version_name_map import PUBLIC_TABLE, INTERNAL_TABLE, VersionNameMap
from webkitpy.common.wavediff import WaveDiff
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.port import config as port_config
from webkitpy.port import driver
from webkitpy.port import image_diff
from webkitpy.port import server_process
from webkitpy.port.factory import PortFactory
from webkitpy.port.image_diff import ImageDiffResult
from webkitpy.layout_tests.servers import apache_http_server, http_server, http_server_base
from webkitpy.layout_tests.servers import web_platform_test_server
from webkitpy.layout_tests.servers import websocket_server

_log = logging.getLogger(__name__)


class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package."""

    # Subclasses override this. This should indicate the basic implementation
    # part of the port name, e.g., 'win', 'gtk'; there is probably (?) one unique value per class.

    # FIXME: We should probably rename this to something like 'implementation_name'.
    port_name = None

    # Test names resemble unix relative paths, and use '/' as a directory separator.
    TEST_PATH_SEPARATOR = '/'

    ALL_BUILD_TYPES = ('debug', 'release')

    DEFAULT_ARCHITECTURE = 'x86'
    DEVICE_TYPE = None
    DEFAULT_DEVICE_TYPES = []

    # Do test runners support alias hostnames such as web-platform.test
    supports_localhost_aliases = False

    helper = None
    _web_platform_test_server = None
    _websocket_secure_server = None
    _websocket_server = None

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        """Return a fully-specified port name that can be used to construct objects."""
        # Subclasses will usually override this.
        options = options or {}
        assert port_name.startswith(cls.port_name)
        if getattr(options, 'webkit_test_runner', False) and not '-wk2' in port_name:
            return port_name + '-wk2'
        return port_name

    def __init__(self, host, port_name, options=None, **kwargs):

        # This value may be different from cls.port_name by having version modifiers
        # and other fields appended to it (for example, 'mac-wk2' or 'win').
        self._name = port_name

        # These are default values that should be overridden in a subclasses.
        self._os_version = None

        # FIXME: Ideally we'd have a package-wide way to get a
        # well-formed options object that had all of the necessary
        # options defined on it.
        self._options = options or optparse.Values()

        if self._name and '-wk2' in self._name:
            self._options.webkit_test_runner = True

        self.host = host
        self._executive = host.executive
        self._filesystem = host.filesystem
        self._webkit_finder = WebKitFinder(host.filesystem)
        self._config = port_config.Config(self._executive, self._filesystem, self.port_name)
        self.pretty_patch = PrettyPatch(self._executive, self.path_from_webkit_base(), self._filesystem)

        self._http_server = None
        self._image_differ = None
        self._server_process_constructor = server_process.ServerProcess  # overridable for testing
        self._test_runner_process_constructor = server_process.ServerProcess

        self.set_option_default('gather-expected-crash-logs', True)

        if not hasattr(options, 'configuration') or not options.configuration:
            self.set_option_default('configuration', self.default_configuration())
        self._test_configuration = None
        self._results_directory = None
        self._root_was_set = hasattr(options, 'root') and options.root
        self._jhbuild_wrapper = []
        self._layout_tests_dir = hasattr(options, 'layout_tests_dir') and options.layout_tests_dir and self._filesystem.abspath(options.layout_tests_dir)
        self._w3c_resource_files = None
        self._display_server = None

    def target_host(self, worker_number=None):
        return self.host

    def architecture(self):
        return self.get_option('architecture') or self.DEFAULT_ARCHITECTURE

    def set_architecture(self, arch):
        self.set_option('architecture', arch)

    def additional_drt_flag(self):
        return []

    def supports_per_test_timeout(self):
        return True

    def supports_layout_tests(self):
        return True

    def default_pixel_tests(self):
        # FIXME: Disable until they are run by default on build.webkit.org.
        return False

    def default_timeout_ms(self):
        return 30 * 1000

    def driver_stop_timeout(self):
        """ Returns the amount of time in seconds to wait before killing the process in driver.stop()."""
        # We want to wait for at least 3 seconds, but if we are really slow, we want to be slow on cleanup as
        # well (for things like ASAN, Valgrind, etc.)
        return 3.0 * float(self.get_option('time_out_ms', '0')) / self.default_timeout_ms()

    def should_retry_crashes(self):
        return False

    def default_child_processes(self, **kwargs):
        """Return the number of DumpRenderTree instances to use for this port."""
        return self._executive.cpu_count()

    def max_child_processes(self, device_type=None):
        """Forbid the user from specifying more than this number of child processes"""
        if device_type:
            return 0
        return float('inf')

    def supported_device_types(self):
        # An empty list would indicate a port was incapable of running tests.
        return [None]

    def baseline_path(self):
        """Return the absolute path to the directory to store new baselines in for this port."""
        # FIXME: remove once all callers are calling either baseline_version_dir() or baseline_platform_dir()
        return self.baseline_version_dir()

    def baseline_platform_dir(self):
        """Return the absolute path to the default (version-independent) platform-specific results."""
        return self._filesystem.join(self.layout_tests_dir(), 'platform', self.port_name)

    def baseline_version_dir(self):
        """Return the absolute path to the platform-and-version-specific results."""
        baseline_search_paths = self.baseline_search_path()
        return baseline_search_paths[0]

    def baseline_search_path(self, device_type=None):
        return self.get_option('additional_platform_directory', []) + self._compare_baseline() + self.default_baseline_search_path(device_type=device_type)

    def default_baseline_search_path(self, device_type=None):
        """Return a list of absolute paths to directories to search under for
        baselines. The directories are searched in order."""
        search_paths = []
        if self.get_option('webkit_test_runner'):
            search_paths.append(self._wk2_port_name())
        search_paths.append(self.name())
        if self.name() != self.port_name:
            search_paths.append(self.port_name)
        return list(map(self._webkit_baseline_path, search_paths))

    @memoized
    def _compare_baseline(self):
        factory = PortFactory(self.host)
        target_port = self.get_option('compare_port')
        if target_port:
            return factory.get(target_port).default_baseline_search_path()
        return []

    def all_baseline_search_paths(self, device_type=None):
        paths = (
            set(self.get_option("additional_platform_directory", []))
            | set(self._compare_baseline())
            | set(self._filesystem.glob(self._webkit_baseline_path("*")))
        )

        assert {p for p in paths if self._filesystem.isdir(p)}.issuperset(
            {
                p
                for p in self.baseline_search_path(device_type=device_type)
                if self._filesystem.isdir(p)
            }
        )

        return paths

    def check_build(self):
        """This routine is used to ensure that the build is up to date
        and all the needed binaries are present."""
        # If we're using a pre-built copy of WebKit (--root), we assume it also includes a build of DRT.
        if not self._root_was_set and self.get_option('build') and not self._build_driver():
            return False
        if self.get_option('install') and not self._check_driver():
            return False
        if not self.check_image_diff():
            return self._build_image_diff()
        return True

    def check_api_test_build(self, canonicalized_binaries=None):
        if not canonicalized_binaries:
            canonicalized_binaries = self.path_to_api_test_binaries().keys()
        if not self._root_was_set and self.get_option('build') and not self._build_api_tests(wtf_only=(canonicalized_binaries == ['TestWTF'])):
            return False

        for binary, path in self.path_to_api_test_binaries().items():
            if binary not in canonicalized_binaries:
                continue
            if not self._filesystem.exists(path):
                _log.error('{} was not found at {}'.format(os.path.basename(path), path))
                return False
        return True

    def environment_for_api_tests(self):
        return self.setup_environ_for_server()

    def _check_driver(self):
        driver_path = self._path_to_driver()
        if not self._filesystem.exists(driver_path):
            _log.error("%s was not found at %s" % (self.driver_name(), driver_path))
            return False
        return True

    def check_sys_deps(self):
        """If the port needs to do some runtime checks to ensure that the
        tests can be run successfully, it should override this routine.
        This step can be skipped with --nocheck-sys-deps.

        Returns whether the system is properly configured."""
        return True

    def check_image_diff(self, override_step=None, logging=True):
        """This routine is used to check whether image_diff binary exists."""
        image_diff_path = self._path_to_image_diff()
        if not self._filesystem.exists(image_diff_path):
            if logging:
                _log.error("ImageDiff was not found at %s" % image_diff_path)
            return False
        return True

    def check_httpd(self):
        if self._uses_apache():
            httpd_path = self._path_to_apache()
        else:
            httpd_path = self._path_to_lighttpd()

        try:
            server_name = self._filesystem.basename(httpd_path)
            env = self.setup_environ_for_server(server_name)
            if self._executive.run_command([httpd_path, "-v"], env=env, return_exit_code=True) != 0:
                _log.error("httpd seems broken. Cannot run http tests.")
                return False
            return True
        except OSError:
            _log.error("No httpd found. Cannot run http tests.")
            return False

    def do_text_results_differ(self, expected_text, actual_text):
        return expected_text != actual_text

    def do_audio_results_differ(self, expected_audio, actual_audio):
        if expected_audio == actual_audio:
            return False
        return not WaveDiff(expected_audio, actual_audio).filesAreIdenticalWithinTolerance()

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        """Compare two images and return a tuple of an image diff, a percentage difference (0-100), and an error string.

        |tolerance| should be a percentage value (0.0 - 100.0).
        If it is omitted, the port default tolerance value is used.

        If an error occurs (like ImageDiff isn't found, or crashes, we log an error and return True (for a diff).
        """
        if not actual_contents and not expected_contents:
            return ImageDiffResult(passed=True, diff_image=None, difference=0)

        if not actual_contents or not expected_contents:
            return ImageDiffResult(passed=False, diff_image=b'', difference=1)

        if not self._image_differ:
            self._image_differ = image_diff.ImageDiffer(self)

        self.set_option_default('tolerance', 0.1)
        if tolerance is None:
            tolerance = self.get_option('tolerance')
        return self._image_differ.diff_image(expected_contents, actual_contents, tolerance)

    def diff_text(self, expected_text, actual_text, expected_filename, actual_filename):
        """Returns a string containing the diff of the two text strings
        in 'unified diff' format."""
        expected_filename = string_utils.decode(string_utils.encode(expected_filename), target_type=str)
        actual_filename = string_utils.decode(string_utils.encode(actual_filename), target_type=str)
        diff = difflib.unified_diff(expected_text.splitlines(True),
                                    actual_text.splitlines(True),
                                    expected_filename,
                                    actual_filename)
        result = ""
        for line in diff:
            result += line
            if not line.endswith('\n'):
                result += '\n No newline at end of file\n'
        return result

    def check_for_leaks(self, process_name, process_id):
        # Subclasses should check for leaks in the running process
        # and print any necessary warnings if leaks are found.
        # FIXME: We should consider moving much of this logic into
        # Executive and make it platform-specific instead of port-specific.
        pass

    def print_leaks_summary(self):
        # Subclasses can override this to print a summary of leaks found
        # while running the layout tests.
        pass

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        if self.get_option('webkit_test_runner'):
            return 'WebKitTestRunner'
        return 'DumpRenderTree'

    def expected_baselines_by_extension(self, test_name):
        """Returns a dict mapping baseline suffix to relative path for each baseline in
        a test. For reftests, it returns ".==" or ".!=" instead of the suffix."""
        # FIXME: The name similarity between this and expected_baselines() below, is unfortunate.
        # We should probably rename them both.
        baseline_dict = {}
        reference_files = self.reference_files(test_name)
        if reference_files:
            # FIXME: How should this handle more than one type of reftest?
            baseline_dict['.' + reference_files[0][0]] = self.relative_test_filename(reference_files[0][1])

        for extension in self.baseline_extensions():
            path = self.expected_filename(test_name, extension, return_default=False)
            baseline_dict[extension] = self.relative_test_filename(path) if path else path

        return baseline_dict

    def baseline_extensions(self):
        """Returns a tuple of all of the non-reftest baseline extensions we use. The extensions include the leading '.'."""
        return ('.wav', '.webarchive', '.txt', '.png')

    def _expected_baselines_for_suffixes(self, test_name, suffixes, all_baselines=False, device_type=None):
        baseline_search_path = self.baseline_search_path(device_type=device_type) + [self.layout_tests_dir()]
        fs = self._filesystem

        baseline_ext_parts = fs.splitext(test_name)

        baseline_name_root = baseline_ext_parts[0]
        if len(baseline_ext_parts) > 1:
            if '?' in baseline_ext_parts[1]:
                baseline_name_root += '_' + baseline_ext_parts[1].split('?')[1]
            if '#' in baseline_ext_parts[1]:
                baseline_name_root += '_' + baseline_ext_parts[1].split('#')[1]
        baseline_name_root += '-expected'

        baselines = []
        for platform_dir in baseline_search_path:
            unsuffixed = fs.join(platform_dir, baseline_name_root)
            for suffix in suffixes:
                if fs.exists(unsuffixed + suffix):
                    baseline_filename = baseline_name_root + suffix
                    baselines.append((platform_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        if baselines:
            return baselines

        for suffix in suffixes:
            baselines.append((None, baseline_name_root + suffix))
        return baselines

    def expected_baselines(self, test_name, suffix, all_baselines=False, device_type=None):
        """Given a test name, finds where the baseline results are located.

        Args:
        test_name: name of test file (usually a relative path under LayoutTests/)
        suffix: file suffix of the expected results, including dot; e.g.
            '.txt' or '.png'.  This should not be None, but may be an empty
            string.
        all_baselines: If True, return an ordered list of all baseline paths
            for the given platform. If False, return only the first one.
        Returns
        a list of ( platform_dir, results_filename ), where
            platform_dir - abs path to the top of the results tree (or test
                tree)
            results_filename - relative path from top of tree to the results
                file
            (port.join() of the two gives you the full path to the file,
                unless None was returned.)
        Return values will be in the format appropriate for the current
        platform (e.g., "\\" for path separators on Windows). If the results
        file is not found, then None will be returned for the directory,
        but the expected relative pathname will still be returned.

        This routine is generic but lives here since it is used in
        conjunction with the other baseline and filename routines that are
        platform specific.
        """
        return self._expected_baselines_for_suffixes(test_name, [suffix], all_baselines=all_baselines, device_type=device_type)

    def expected_filename(self, test_name, suffix, return_default=True, device_type=None):
        """Given a test name, returns an absolute path to its expected results.

        If no expected results are found in any of the searched directories,
        the directory in which the test itself is located will be returned.
        The return value is in the format appropriate for the platform
        (e.g., "\\" for path separators on windows).

        Args:
        test_name: name of test file (usually a relative path under LayoutTests/)
        suffix: file suffix of the expected results, including dot; e.g. '.txt'
            or '.png'.  This should not be None, but may be an empty string.
        platform: the most-specific directory name to use to build the
            search list of directories; e.g. 'mountainlion-wk2'
        return_default: if True, returns the path to the generic expectation if nothing
            else is found; if False, returns None.

        This routine is generic but is implemented here to live alongside
        the other baseline and filename manipulation routines.
        """
        platform_dir, baseline_filename = self.expected_baselines(test_name, suffix, device_type=device_type)[0]
        if platform_dir or return_default:
            return self._filesystem.join(platform_dir or self.layout_tests_dir(), baseline_filename)
        return None

    def expected_checksum(self, test_name, device_type=None):
        """Returns the checksum of the image we expect the test to produce, or None if it is a text-only test."""
        png_path = self.expected_filename(test_name, '.png', device_type=device_type)

        if self._filesystem.exists(png_path):
            with self._filesystem.open_binary_file_for_reading(png_path) as filehandle:
                return read_checksum_from_png.read_checksum(filehandle)

        return None

    def expected_image(self, test_name, device_type=None):
        """Returns the image we expect the test to produce."""
        baseline_path = self.expected_filename(test_name, '.png', device_type=device_type)
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_audio(self, test_name, device_type=None):
        baseline_path = self.expected_filename(test_name, '.wav', device_type=device_type)
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def is_unexpected_crash(self, test_name):
        from webkitpy.layout_tests.models.test_expectations import TestExpectations, CRASH
        expectations = TestExpectations(self, [test_name, ])
        expectations.parse_all_expectations()
        return CRASH not in expectations.filtered_expectations_for_test(test_name, False, False)

    def expected_text(self, test_name, device_type=None):
        """Returns the text output we expect the test to produce, or None
        if we don't expect there to be any text output.
        End-of-line characters are normalized to '\n'."""
        # FIXME: DRT output is actually utf-8, but since we don't decode the
        # output from DRT (instead treating it as a binary string), we read the
        # baselines as a binary string, too.
        baseline_path = self.expected_filename(test_name, '.txt', device_type=device_type)
        if not self._filesystem.exists(baseline_path):
            baseline_path = self.expected_filename(test_name, '.webarchive', device_type=device_type)
            if not self._filesystem.exists(baseline_path):
                return None
        text = string_utils.decode(self._filesystem.read_binary_file(baseline_path), target_type=str)
        return text.replace("\r\n", "\n")

    _supported_reference_extensions = set(['.html', '.xml', '.xhtml', '.htm', '.svg', '.xht'])

    def reference_files(self, test_name, device_type=None):
        """Return a list of expectation (== or !=) and filename pairs"""

        if self.get_option('treat_ref_tests_as_pixel_tests'):
            return []

        result = []
        suffixes = []
        for part1 in ['', '-mismatch']:
            for part2 in self._supported_reference_extensions:
                suffixes.append(part1 + part2)
        for platform_dir, baseline_filename in self._expected_baselines_for_suffixes(test_name, suffixes, device_type=device_type):
            if not platform_dir:
                continue
            result.append((
                '!=' if '-mismatch.' in baseline_filename else '==',
                self._filesystem.join(platform_dir, baseline_filename),
            ))
        return result

    def potential_test_names_from_expected_file(self, path):
        """Return potential test names if any from a potential expected file path, relative to LayoutTests directory."""

        if not '-expected.' in path:
            return None

        if path.startswith('platform' + self._filesystem.sep):
            steps = path.split(self._filesystem.sep)
            path = self._filesystem.join(self._filesystem.sep.join(steps[2:]))

        return [self.host.filesystem.relpath(test, self.layout_tests_dir()) for test in self._filesystem.glob(re.sub('-expected.*', '.*', self._filesystem.join(self.layout_tests_dir(), path))) if self._filesystem.isfile(test)]

    def test_key(self, test_name):
        """Turns a test name into a list with two sublists, the natural key of the
        dirname, and the natural key of the basename.

        This can be used when sorting paths so that files in a directory.
        directory are kept together rather than being mixed in with files in
        subdirectories."""
        dirname, basename = self.split_test(test_name)
        return (self._natural_sort_key(dirname + self.TEST_PATH_SEPARATOR), self._natural_sort_key(basename))

    def _natural_sort_key(self, string_to_split):
        """ Turns a string into a list of string and number chunks, i.e. "z23a" -> ["z", 23, "a"]

        This can be used to implement "natural sort" order. See:
        http://www.codinghorror.com/blog/2007/12/sorting-for-humans-natural-sort-order.html
        http://nedbatchelder.com/blog/200712.html#e20071211T054956
        """
        def tryint(val):
            try:
                return int(val)
            except ValueError:
                return val

        return [(tryint(chunk), chunk) for chunk in re.split(r'(\d+)', string_to_split)]

    def test_dirs(self):
        """Returns the list of top-level test directories."""
        layout_tests_dir = self.layout_tests_dir()
        return filter(lambda x: self._filesystem.isdir(self._filesystem.join(layout_tests_dir, x)),
                      self._filesystem.listdir(layout_tests_dir))

    @memoized
    def test_isfile(self, test_name):
        """Return True if the test name refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        return self._filesystem.isfile(self.abspath_for_test(test_name))

    @memoized
    def test_isdir(self, test_name):
        """Return True if the test name refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        return self._filesystem.isdir(self.abspath_for_test(test_name))

    @memoized
    def test_exists(self, test_name):
        """Return True if the test name refers to an existing test or baseline."""
        # Used by test_expectations.py to determine if an entry refers to a
        # valid test and by printing.py to determine if baselines exist.
        if self.test_isfile(test_name) or self.test_isdir(test_name):
            return True
        if '?' in test_name or '#' in test_name:
            fs = self._filesystem
            ext_parts = fs.splitext(test_name)
            test_name = ext_parts[0]
            if len(ext_parts) > 1 and '?' in ext_parts[1]:
                test_name += ext_parts[1].split('?')[0]
            if len(ext_parts) > 1 and '#' in ext_parts[1]:
                test_name += ext_parts[1].split('#')[0]
            return self.test_isfile(test_name)
        return False

    def split_test(self, test_name):
        """Splits a test name into the 'directory' part and the 'basename' part."""
        index = test_name.rfind(self.TEST_PATH_SEPARATOR)
        if index < 1:
            return ('', test_name)
        return (test_name[0:index], test_name[index:])

    def normalize_test_name(self, test_name):
        """Returns a normalized version of the test name or test directory."""
        if test_name.endswith(self.TEST_PATH_SEPARATOR):
            return test_name
        if self.test_isdir(test_name):
            return test_name + self.TEST_PATH_SEPARATOR
        return test_name

    def driver_cmd_line_for_logging(self):
        """Prints the DRT command line that will be used."""
        driver = self.create_driver(0)
        return driver.cmd_line(self.get_option('pixel_tests'), [])

    def update_baseline(self, baseline_path, data):
        """Updates the baseline for a test.

        Args:
            baseline_path: the actual path to use for baseline, not the path to
              the test. This function is used to update either generic or
              platform-specific baselines, but we can't infer which here.
            data: contents of the baseline.
        """
        self._filesystem.write_binary_file(baseline_path, data)

    # FIXME: update callers to create a finder and call it instead of these next five routines (which should be protected).
    def webkit_base(self):
        return self._webkit_finder.webkit_base()

    def path_from_webkit_base(self, *comps):
        return self._webkit_finder.path_from_webkit_base(*comps)

    def path_to_script(self, script_name):
        return self._webkit_finder.path_to_script(script_name)

    def layout_tests_dir(self):
        if self._layout_tests_dir:
            return self._layout_tests_dir
        return self._webkit_finder.layout_tests_dir()

    def perf_tests_dir(self):
        return self._webkit_finder.perf_tests_dir()

    def skipped_layout_tests(self, device_type=None):
        """Returns tests skipped outside of the TestExpectations files."""
        return set(self._tests_for_other_platforms(device_type=device_type)) | set(
            self.get_option("ignore_tests", [])
        )

    @memoized
    def skipped_perf_tests(self):
        filename = self._filesystem.join(self.perf_tests_dir(), "Skipped")
        if not self._filesystem.exists(filename):
            _log.debug("Skipped does not exist: %s" % filename)
            return []

        skipped_file_contents = self._filesystem.read_text_file(filename)
        tests_to_skip = []
        for line_number, line in enumerate(skipped_file_contents.split('\n')):
            match = re.match(r'^\s*(\[(?P<platforms>[\w ]*?)\])?\s*(?P<test>[\w\-\/\.]+?)?\s*(?P<comment>\#.*)?$', line)
            if not match:
                _log.error("Syntax error at line %d in %s: %s" % (line_number + 1, filename, line))
            else:
                platform_names = list(filter(lambda token: token, match.group('platforms').lower().split(' '))) if match.group('platforms') else []
                test_name = match.group('test')
                if test_name and (not platform_names or self.port_name in platform_names or self._name in platform_names):
                    tests_to_skip.append(test_name)

        return tests_to_skip

    def skips_perf_test(self, test_name):
        for test_or_category in self.skipped_perf_tests():
            if test_or_category == test_name:
                return True
            category = self._filesystem.join(self.perf_tests_dir(), test_or_category)
            if self._filesystem.isdir(category) and test_name.startswith(test_or_category):
                return True
        return False

    def name(self):
        """Returns a name that uniquely identifies this particular type of port
        (e.g., "mac-snowleopard" or "chromium-linux-x86_x64" and can be passed
        to factory.get() to instantiate the port."""
        return self._name

    def operating_system(self):
        # Subclasses should override this default implementation.
        return 'mac'

    @memoized
    def version_name(self):
        """Returns a string indicating the version of a given platform, e.g.
        'leopard' or 'xp'.

        This is used to help identify the exact port when parsing test
        expectations, determining search paths, and logging information."""
        if self._os_version is None:
            return None
        result = VersionNameMap.map(self.host.platform).to_name(self._os_version, table=PUBLIC_TABLE)
        if not result:
            result = VersionNameMap.map(self.host.platform).to_name(self._os_version, table=INTERNAL_TABLE)
        return result

    def get_option(self, name, default_value=None):
        return getattr(self._options, name, default_value)

    def set_option(self, name, value):
        setattr(self._options, name, value)
        return self.get_option(name) == value

    def set_option_default(self, name, default_value):
        if isinstance(self._options, argparse.Namespace):
            if not hasattr(self._options, name):
                setattr(self._options, name, default_value)
                return True
            elif not self.get_option(name):
                self.set_option(name, default_value)
        else:
            return self._options.ensure_value(name, default_value)

    @memoized
    def path_to_generic_test_expectations_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'TestExpectations')

    @memoized
    def path_to_test_expectations_file(self):
        """Update the test expectations to the passed-in string.

        This is used by the rebaselining tool. Raises NotImplementedError
        if the port does not use expectations files."""

        # FIXME: We need to remove this when we make rebaselining work with multiple files and just generalize expectations_files().

        # test_expectations are always in mac/ not mac-leopard/ by convention, hence we use port_name instead of name().
        return self._filesystem.join(self._webkit_baseline_path(self.port_name), 'TestExpectations')

    def relative_test_filename(self, filename):
        """Returns a test_name a relative unix-style path for a filename under the LayoutTests
        directory. Ports may legitimately return abspaths here if no relpath makes sense."""
        # Ports that run on windows need to override this method to deal with
        # filenames with backslashes in them.
        if filename.startswith(self.layout_tests_dir()):
            return self.host.filesystem.relpath(filename, self.layout_tests_dir()).replace(self.host.filesystem.sep, self.TEST_PATH_SEPARATOR)
        else:
            return self.host.filesystem.abspath(filename)

    @memoized
    def abspath_for_test(self, test_name, target_host=None):
        """Returns the full path to the file for a given test name. This is the
        inverse of relative_test_filename() if no target_host is specified."""
        host = target_host or self.host
        return host.filesystem.join(host.filesystem.map_base_host_path(self.layout_tests_dir()), test_name.replace(self.TEST_PATH_SEPARATOR, self.host.filesystem.sep))

    def jsc_results_directory(self):
        return self._build_path()

    def bindings_results_directory(self):
        return self._build_path()

    def results_directory(self):
        """Absolute path to the place to store the test results (uses --results-directory)."""
        if not self._results_directory:
            option_val = self.get_option('results_directory') or self.default_results_directory()
            self._results_directory = self._filesystem.abspath(option_val)
        return self._results_directory

    def perf_results_directory(self):
        return self._build_path()

    def python_unittest_results_directory(self):
        return self._build_path('python-unittest-results')

    def default_results_directory(self):
        """Absolute path to the default place to store the test results."""
        # Results are store relative to the built products to make it easy
        # to have multiple copies of webkit checked out and built.
        return self._build_path('layout-test-results')

    def setup_test_run(self, device_type=None):
        """Perform port-specific work at the beginning of a test run."""
        pass

    def clean_up_test_run(self):
        """Perform port-specific work at the end of a test run."""
        if self._image_differ:
            self._image_differ.stop()
            self._image_differ = None

    # FIXME: os.environ access should be moved to onto a common/system class to be more easily mockable.
    def _value_or_default_from_environ(self, name, default=None):
        if name in os.environ:
            return os.environ[name]
        return default

    def _copy_value_from_environ_if_set(self, clean_env, name):
        if name in os.environ:
            clean_env[name] = os.environ[name]

    def port_adjust_environment_for_test_driver(self, env):
        return env

    def setup_environ_for_server(self, server_name=None):
        # We intentionally copy only a subset of os.environ when
        # launching subprocesses to ensure consistent test results.
        clean_env = {}
        # Note: don't set here driver specific variables (related to X11, Wayland, etc.)
        # Use the driver _setup_environ_for_test() method for that.
        variables_to_copy = [
            # For Linux:
            'ALSA_CARD',
            'DBUS_SESSION_BUS_ADDRESS',
            'LANG',
            'LD_LIBRARY_PATH',
            'TERM',
            'TZ',
            'XDG_DATA_DIRS',
            'XDG_RUNTIME_DIR',

            # Darwin:
            'DYLD_FRAMEWORK_PATH',
            'DYLD_LIBRARY_PATH',
            '__XPC_DYLD_FRAMEWORK_PATH',
            '__XPC_DYLD_LIBRARY_PATH',
            'JSC_useKernTCSM',
            '__XPC_JSC_useKernTCSM',

            # CYGWIN:
            'HOMEDRIVE',
            'HOMEPATH',
            '_NT_SYMBOL_PATH',

            # Windows:
            'COMSPEC',
            'SYSTEMDRIVE',
            'SYSTEMROOT',
            'WEBKIT_LIBRARIES',

            # Most ports (?):
            'HOME',
            'PATH',
            'WEBKIT_TESTFONTS',
            'WEBKIT_OUTPUTDIR',

        ]
        for variable in variables_to_copy:
            self._copy_value_from_environ_if_set(clean_env, variable)

        for string_variable in self.get_option('additional_env_var', []):
            [name, value] = string_variable.split('=', 1)
            clean_env[name] = value

        # FIXME: Some tests fail if the time zone is not set to US/Pacific (<https://webkit.org/b/186612>)
        clean_env['TZ'] = 'US/Pacific'

        return clean_env

    def _clear_global_caches_and_temporary_files(self):
        pass

    @staticmethod
    def _append_value_colon_separated(env, name, value):
        assert os.pathsep not in value
        if name in env and env[name]:
            env[name] = env[name] + os.pathsep + value
        else:
            env[name] = value

    def show_results_html_file(self, results_filename):
        """This routine should display the HTML file pointed at by
        results_filename in a users' browser."""
        return self.host.user.open_url(path.abspath_to_uri(self.host.platform, results_filename))

    def create_driver(self, worker_number, no_timeout=False):
        """Return a newly created Driver subclass for starting/stopping the test driver."""
        return driver.DriverProxy(self, worker_number, self._driver_class(), pixel_tests=self.get_option('pixel_tests'), no_timeout=no_timeout)

    def start_helper(self, pixel_tests=False, prefer_integrated_gpu=False):
        """If a port needs to reconfigure graphics settings or do other
        things to ensure a known test configuration, it should override this
        method."""
        return True

    def reset_preferences(self):
        """If a port needs to reset platform-specific persistent preference
        storage, it should override this method."""
        pass

    def ports_to_forward(self):
        ports = []
        if self._http_server:
            ports.extend(self._http_server.ports_to_forward())
        if Port._websocket_server:
            ports.extend(Port._websocket_server.ports_to_forward())
        if Port._websocket_server:
            ports.extend(Port._websocket_secure_server.ports_to_forward())
        if Port._web_platform_test_server:
            ports.extend(Port._web_platform_test_server.ports_to_forward())
        return ports

    def start_http_server(self, additional_dirs=None):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a web server to be running."""
        assert not self._http_server, 'Already running an http server.'
        if not self.check_httpd():
            return

        http_port = self.get_option('http_port')
        if self._uses_apache():
            server = apache_http_server.LayoutTestApacheHttpd(self, self.results_directory(), additional_dirs=additional_dirs, port=http_port)
        else:
            server = http_server.Lighttpd(self, self.results_directory(), additional_dirs=additional_dirs, port=http_port)

        server.start()
        self._http_server = server

    def is_http_server_running(self):
        if self._http_server:
            return True
        return http_server_base.is_http_server_running()

    def is_websocket_server_running(self):
        if Port._websocket_server:
            return True
        return websocket_server.is_web_socket_server_running()

    def is_wpt_server_running(self):
        if Port._web_platform_test_server:
            return True
        return web_platform_test_server.is_wpt_server_running(self)

    def start_websocket_server(self):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a websocket server to be running."""
        assert not Port._websocket_server, 'Already running a websocket server.'

        server = websocket_server.PyWebSocket(self, self.results_directory())
        server.start()
        Port._websocket_server = server

        websocket_server_temporary_directory = self._filesystem.mkdtemp(prefix='webkitpy-websocket-server')
        Port._websocket_server_temporary_directory = websocket_server_temporary_directory

        pem_file = self._filesystem.join(self.layout_tests_dir(), "http", "conf", "webkit-httpd.pem")
        pem = pemfile.load(self._filesystem, pem_file)
        certificate_file = self._filesystem.join(str(websocket_server_temporary_directory), 'webkit-httpd.crt')
        self._filesystem.write_text_file(certificate_file, pem.certificate)
        private_key_file = self._filesystem.join(str(websocket_server_temporary_directory), 'webkit-httpd.key')
        self._filesystem.write_text_file(private_key_file, pem.private_key)

        secure_server = Port._websocket_secure_server = websocket_server.PyWebSocket(self, self.results_directory(),
            use_tls=True, port=websocket_server.PyWebSocket.DEFAULT_WSS_PORT, private_key=private_key_file, certificate=certificate_file)
        secure_server.start()
        Port._websocket_secure_server = secure_server

    def start_web_platform_test_server(self, additional_dirs=None, number_of_servers=None):
        assert not Port._web_platform_test_server, 'Already running a Web Platform Test server.'

        Port._web_platform_test_server = web_platform_test_server.WebPlatformTestServer(self, "wptwk")
        Port._web_platform_test_server.start()

        # Wait until a 5 seconds timeout happens or the HTTP server has actually finally started.
        # https://gist.github.com/butla/2d9a4c0f35ea47b7452156c96a4e7b12
        port = Port._web_platform_test_server.first_port(self)
        if port is None:
            return
        host = 'localhost'
        start_time = time.perf_counter()
        timeout = 5
        while True:
            try:
                with socket.create_connection((host, port), timeout=timeout):
                    break
            except OSError:
                time.sleep(0.01)
                if time.perf_counter() - start_time >= timeout:
                    raise TimeoutError('Waited too long for the port {} on host {} to start accepting '
                                       'connections.'.format(port, host))

    def web_platform_test_server_doc_root(self):
        return web_platform_test_server.doc_root(self).replace('\\', self.TEST_PATH_SEPARATOR) + self.TEST_PATH_SEPARATOR

    def web_platform_test_server_base_http_url(self, localhost_only=False):
        return web_platform_test_server.base_http_url(self, localhost_only)

    def web_platform_test_server_base_https_url(self, localhost_only=False):
        return web_platform_test_server.base_https_url(self, localhost_only)

    def http_server_supports_ipv6(self):
        # Cygwin is the only platform to still use Apache 1.3, which only supports IPV4.
        # Once it moves to Apache 2, we can drop this method altogether.
        if self.host.platform.is_cygwin():
            return False
        return True

    def stop_helper(self):
        """Shut down the test helper if it is running. Do nothing if
        it isn't, or it isn't available."""
        if Port.helper:
            _log.debug("Stopping LayoutTestHelper")
            try:
                Port.helper.stdin.write(b"x\n")
                Port.helper.stdin.close()
                Port.helper.wait()
            except IOError as e:
                _log.debug("IOError raised while stopping helper: %s" % str(e))
            Port.helper = None

    def stop_http_server(self):
        """Shut down the http server if it is running. Do nothing if it isn't."""
        if self._http_server:
            self._http_server.stop()
            self._http_server = None

    def stop_websocket_server(self):
        """Shut down the websocket server if it is running. Do nothing if it isn't."""
        if Port._websocket_server:
            Port._websocket_server.stop()
            Port._websocket_server = None
        if Port._websocket_secure_server:
            Port._websocket_secure_server.stop()
            Port._websocket_secure_server = None
        if Port._websocket_server_temporary_directory:
            self._filesystem.rmtree(str(Port._websocket_server_temporary_directory))

    def stop_web_platform_test_server(self):
        if Port._web_platform_test_server:
            Port._web_platform_test_server.stop()
            Port._web_platform_test_server = None

    def exit_code_from_summarized_results(self, unexpected_results):
        """Given summarized results, compute the exit code to be returned by new-run-webkit-tests.
        Bots turn red when this function returns a non-zero value. By default, return the number of regressions
        to avoid turning bots red by flaky failures, unexpected passes, and missing results"""
        # Don't turn bots red for flaky failures, unexpected passes, and missing results.
        return unexpected_results['num_regressions']

    #
    # TEST EXPECTATION-RELATED METHODS
    #

    def test_configuration(self):
        """Returns the current TestConfiguration for the port."""
        if not self._test_configuration:
            self._test_configuration = TestConfiguration(self.version_name(), self.architecture(), self._options.configuration.lower())
        return self._test_configuration

    # FIXME: Belongs on a Platform object.
    @memoized
    def all_test_configurations(self):
        """Returns a list of TestConfiguration instances, representing all available
        test configurations for this port."""
        return self._generate_all_test_configurations()

    # FIXME: Belongs on a Platform object.
    def configuration_specifier_macros(self):
        """Ports may provide a way to abbreviate configuration specifiers to conveniently
        refer to them as one term or alias specific values to more generic ones. For example:

        (xp, vista, win7) -> win # Abbreviate all Windows versions into one namesake.
        (lucid) -> linux  # Change specific name of the Linux distro to a more generic term.

        Returns a dictionary, each key representing a macro term ('win', for example),
        and value being a list of valid configuration specifiers (such as ['xp', 'vista', 'win7'])."""
        return {}

    def all_baseline_variants(self):
        """Returns a list of platform names sufficient to cover all the baselines.

        The list should be sorted so that a later platform  will reuse
        an earlier platform's baselines if they are the same (e.g.,
        'snowleopard' should precede 'leopard')."""
        raise NotImplementedError

    def uses_test_expectations_file(self):
        # This is different from checking test_expectations() is None, because
        # some ports have Skipped files which are returned as part of test_expectations().
        for path in self.default_baseline_search_path():
            if self._filesystem.exists(self._filesystem.join(path, 'TestExpectations')):
                return True
        return False

    def warn_if_bug_missing_in_test_expectations(self):
        return False

    def expectations_dict(self, device_type=None):
        """Returns an OrderedDict of name -> expectations strings.
        The names are expected to be (but not required to be) paths in the filesystem.
        If the name is a path, the file can be considered updatable for things like rebaselining,
        so don't use names that are paths if they're not paths.
        Generally speaking the ordering should be files in the filesystem in cascade order
        (TestExpectations followed by Skipped, if the port honors both formats),
        then any built-in expectations (e.g., from compile-time exclusions), then --additional-expectations options."""
        # FIXME: rename this to test_expectations() once all the callers are updated to know about the ordered dict.
        expectations = OrderedDict()

        for path in self.expectations_files(device_type=device_type):
            if self._filesystem.exists(path):
                expectations[path] = self._filesystem.read_text_file(path)

        for path in self.get_option('additional_expectations', []):
            expanded_path = self._filesystem.expanduser(path)
            if self._filesystem.exists(expanded_path):
                _log.debug("reading additional_expectations from path '%s'" % path)
                expectations[path] = self._filesystem.read_text_file(expanded_path)
            else:
                _log.warning("additional_expectations path '%s' does not exist" % path)
        return expectations

    def _port_specific_expectations_files(self, **kwargs):
        # Unlike baseline_search_path, we only want to search [WK2-PORT, PORT-VERSION, PORT] and any directories
        # included via --additional-platform-directory, not the full casade.
        search_paths = [self.port_name]

        non_wk2_name = self.name().replace('-wk2', '')
        if non_wk2_name != self.port_name:
            search_paths.append(non_wk2_name)

        if self.get_option('webkit_test_runner'):
            # Because nearly all of the skipped tests for WebKit 2 are due to cross-platform
            # issues, all wk2 ports share a skipped list under platform/wk2.
            search_paths.extend(["wk2", self._wk2_port_name()])

        search_paths.extend(self.get_option("additional_platform_directory", []))

        return [self._filesystem.join(self._webkit_baseline_path(d), 'TestExpectations') for d in search_paths]

    def expectations_files(self, device_type=None):
        return [self.path_to_generic_test_expectations_file()] + self._port_specific_expectations_files(device_type=device_type)

    def repository_paths(self):
        """Returns a list of (repository_name, repository_path) tuples of its depending code base.
        By default it returns a list that only contains a ('WebKit', <webkitRepositoryPath>) tuple."""

        # We use LayoutTest directory here because webkit_base isn't a part of WebKit repository in Chromium port
        # where turnk isn't checked out as a whole.
        repository_paths = [('WebKit', self.layout_tests_dir())]
        if self.get_option('additional_repository_name') and self.get_option('additional_repository_path'):
            repository_paths += [(self._options.additional_repository_name, self._options.additional_repository_path)]
        return repository_paths

    def allowed_hosts(self):
        return self.get_option("allowed_host", [])

    def internal_feature(self):
        return self.get_option("internal_feature", [])

    def experimental_feature(self):
        return self.get_option("experimental_feature", [])

    def localhost_aliases(self):
        if not self.supports_localhost_aliases or self.get_option("disable_wpt_hostname_aliases"):
            return []

        # Documented here: https://github.com/web-platform-tests/wpt/blob/master/docs/writing-tests/server-features.md#tests-involving-multiple-origins
        domains = []
        for domain in ("web-platform.test", "not-web-platform.test"):
            domains.append(domain)
            for subdomain in ("www", "www1", "www2", "xn--n8j6ds53lwwkrqhv28a", "xn--lve-6lad"):
                domains.append(subdomain + "." + domain)

        return domains

    def default_configuration(self):
        return self._config.default_configuration()

    #
    # PROTECTED ROUTINES
    #
    # The routines below should only be called by routines in this class
    # or any of its subclasses.
    #

    def _uses_apache(self):
        return True

    # FIXME: This does not belong on the port object.
    @memoized
    def _path_to_apache(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module."""
        # The Apache binary path can vary depending on OS and distribution
        # See http://wiki.apache.org/httpd/DistrosDefaultLayout
        for path in ["/usr/sbin/httpd", "/usr/sbin/apache2", "/usr/bin/httpd"]:
            if self._filesystem.exists(path):
                return path
        _log.error("Could not find apache. Not installed or unknown path.")
        return None

    # FIXME: This belongs on some platform abstraction instead of Port.
    def _is_redhat_based(self):
        return self._filesystem.exists('/etc/redhat-release')

    def _is_debian_based(self):
        return self._filesystem.exists('/etc/debian_version')

    def _is_arch_based(self):
        return self._filesystem.exists('/etc/arch-release')

    def _is_flatpak(self):
        return self._filesystem.exists('/.flatpak-info')

    def _apache_version(self):
        config = self._executive.run_command([self._path_to_apache(), '-v'])
        return re.sub(r'(?:.|\n)*Server version: Apache/(\d+\.\d+)(?:.|\n)*', r'\1', config)

    # We pass sys_platform into this method to make it easy to unit test.
    def _apache_config_file_name_for_platform(self, sys_platform):
        if sys_platform in ['cygwin', 'win32']:
            return 'win-httpd-' + self._apache_version() + '.conf'
        if sys_platform == 'darwin':
            return 'apache' + self._apache_version() + '-darwin-httpd.conf'
        if sys_platform.startswith('linux'):
            if self._is_redhat_based():
                return 'fedora-httpd-' + self._apache_version() + '.conf'
            if self._is_debian_based():
                return 'debian-httpd-' + self._apache_version() + '.conf'
            if self._is_arch_based():
                return 'archlinux-httpd.conf'
            if self._is_flatpak():
                return 'flatpak-httpd.conf'
        # All platforms use apache2 except for CYGWIN (and Mac OS X Tiger and prior, which we no longer support).
        return 'apache' + self._apache_version() + '-httpd.conf'

    def _path_to_apache_config_file(self):
        """Returns the full path to the apache configuration file.

        If the WEBKIT_HTTP_SERVER_CONF_PATH environment variable is set, its
        contents will be used instead.

        This is needed only by ports that use the apache_http_server module."""
        config_file_from_env = os.environ.get('WEBKIT_HTTP_SERVER_CONF_PATH')
        if config_file_from_env:
            if not self._filesystem.exists(config_file_from_env):
                raise IOError('%s was not found on the system' % config_file_from_env)
            return config_file_from_env

        config_file_name = self._apache_config_file_name_for_platform(sys.platform)
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', config_file_name)

    def _build_path(self, *comps):
        root_directory = self.get_option('_cached_root') or self.get_option('root')
        if not root_directory:
            root_directory = self._config.build_directory(self.get_option('configuration'))
            build_directory = self.get_option('build_directory')
            if build_directory:
                root_directory = self._filesystem.join(build_directory, root_directory.split('/')[-1])

            # We take advantage of the behavior that self._options is passed by reference to worker
            # subprocesses to use it as data store to cache the computed root directory path. This
            # avoids making each worker subprocess compute this path again which is slow because of
            # the call to config.build_directory().
            #
            # FIXME: This is like decorating this function with @memoized, but more annoying and fragile;
            # there should be another way to propagate precomputed values to workers without modifying
            # the options list.
            self.set_option('_cached_root', root_directory)

        if sys.platform.startswith('win') or sys.platform == 'cygwin':
            return self._filesystem.join(root_directory, *comps)

        return self._filesystem.join(self._filesystem.abspath(root_directory), *comps)

    def _path_to_driver(self, configuration=None):
        """Returns the full path to the test driver (DumpRenderTree)."""
        local_driver_path = self._build_path(self.driver_name())
        if sys.platform.startswith('win'):
            base = os.path.splitext(local_driver_path)[0]
            local_driver_path = base + ".exe"
        return local_driver_path

    def _driver_tempdir(self, target_host=None):
        host = target_host or self.host
        return host.filesystem.mkdtemp(prefix='{}s-'.format(self.driver_name()))

    def _path_to_user_cache_directory(self, suffix=None):
        return None

    def _path_to_webcore_library(self):
        """Returns the full path to a built copy of WebCore."""
        return None

    def _path_to_helper(self):
        """Returns the full path to the layout_test_helper binary, which
        is used to help configure the system for the test run, or None
        if no helper is needed.

        This is likely only used by start/stop_helper()."""
        return None

    def _path_to_default_image_diff(self):
        """Returns the full path to the default ImageDiff binary, or None if it is not available."""
        return self._build_path('ImageDiff')

    def run_minibrowser(self, args):
        # FIXME: Migrate to webkitpy based run-minibrowser. https://bugs.webkit.org/show_bug.cgi?id=213464
        miniBrowser = self.path_to_script("old-run-minibrowser")
        args.append(self._config.flag_for_configuration(self.get_option('configuration')))
        args.append("--%s" % self.get_option('platform'))
        return self._executive.run_command([miniBrowser] + args, stdout=None, cwd=self.webkit_base(), return_stderr=False, decode_output=False, ignore_errors=True)

    @decorators.Memoize()
    def _path_to_image_diff(self):
        """Returns the full path to the image_diff binary, or None if it is not available.

        This is likely used only by diff_image()"""
        default_image_diff = self._path_to_default_image_diff()
        if self._filesystem.exists(default_image_diff):
            return default_image_diff
        built_image_diff = self._filesystem.join(self._config.build_directory(self.get_option('configuration'), for_host=True), 'ImageDiff')
        _log.debug('ImageDiff not found at {}, using {} instead'.format(default_image_diff, built_image_diff))
        return built_image_diff

    API_TEST_BINARY_NAMES = ['TestWTF', 'TestWebKitAPI']

    def path_to_api_test_binaries(self):
        return {binary: self._build_path(binary) for binary in self.API_TEST_BINARY_NAMES}

    def _path_to_lighttpd(self):
        """Returns the path to the LigHTTPd binary.

        This is needed only by ports that use the http_server.py module."""
        raise NotImplementedError('Port._path_to_lighttpd')

    def _path_to_lighttpd_modules(self):
        """Returns the path to the LigHTTPd modules directory.

        This is needed only by ports that use the http_server.py module."""
        raise NotImplementedError('Port._path_to_lighttpd_modules')

    def _webkit_baseline_path(self, platform):
        """Return the  full path to the top of the baseline tree for a
        given platform."""
        return self._filesystem.join(self.layout_tests_dir(), 'platform', platform)

    # FIXME: Belongs on a Platform object.
    def _generate_all_test_configurations(self):
        """Generates a list of TestConfiguration instances, representing configurations
        for a platform across all OSes, architectures, build and graphics types."""
        raise NotImplementedError('Port._generate_test_configurations')

    def _driver_class(self):
        """Returns the port's driver implementation."""
        return driver.Driver

    def path_to_crash_logs(self):
        raise NotImplementedError

    def _get_crash_log(self, name, pid, stdout, stderr, newer_than, target_host=None):
        name_str = name or '<unknown process name>'
        pid_str = str(pid or '<unknown>')
        stdout_lines = (stdout or '<empty>').decode('utf8', 'replace').splitlines()
        stderr_lines = (stderr or '<empty>').decode('utf8', 'replace').splitlines()
        return (stderr, 'crash log for %s (pid %s):\n%s\n%s\n' % (name_str, pid_str,
            '\n'.join(('STDOUT: ' + l) for l in stdout_lines),
            '\n'.join(('STDERR: ' + l) for l in stderr_lines)))

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        pass

    def look_for_new_samples(self, unresponsive_processes, start_time):
        pass

    def sample_process(self, name, pid, target_host=None):
        pass

    def _in_flatpak_sandbox(self):
        return self._filesystem.exists("/.flatpak-info")

    def _should_use_jhbuild(self):
        if self._in_flatpak_sandbox():
            return False

        suffix = ""
        if self.port_name:
            suffix = self.port_name.upper()
        return self._filesystem.exists(self.path_from_webkit_base('WebKitBuild', 'Dependencies%s' % suffix))

    # FIXME: Eventually we should standarize port naming, and make this method smart enough
    # to use for all port configurations (including architectures, graphics types, etc).
    def _port_flag_for_scripts(self):
        # This is overrriden by ports which need a flag passed to scripts to distinguish the use of that port.
        return None

    # This is modeled after webkitdirs.pm argumentsForConfiguration() from old-run-webkit-tests
    def _arguments_for_configuration(self):
        config_args = []
        config_args.append(self._config.flag_for_configuration(self.get_option('configuration')))
        # FIXME: We may need to add support for passing --32-bit like old-run-webkit-tests had.
        port_flag = self._port_flag_for_scripts()
        if port_flag:
            config_args.append(port_flag)
        return config_args

    def _run_script(self, script_name, args=None, include_configuration_arguments=True, decode_output=True, env=None):
        run_script_command = [self.path_to_script(script_name)]
        if include_configuration_arguments:
            run_script_command.extend(self._arguments_for_configuration())
        if args:
            run_script_command.extend(args)
        output = self._executive.run_command(run_script_command, cwd=self.webkit_base(), decode_output=decode_output, env=env)
        _log.debug('Output of %s:\n%s' % (run_script_command, string_utils.encode(output, target_type=str) if decode_output else output))
        return output

    def _build_driver(self):
        environment = self.host.copy_current_environment()
        env = environment.to_dictionary()

        # FIXME: We build both DumpRenderTree and WebKitTestRunner for WebKitTestRunner runs because
        # DumpRenderTree includes TestNetscapePlugin. It should be factored out into its own project.
        try:
            self._run_script("build-dumprendertree", args=self._build_driver_flags(), env=env)
            if self.get_option('webkit_test_runner'):
                self._run_script("build-webkittestrunner", args=self._build_driver_flags(), env=env)
        except ScriptError as e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _build_api_tests(self, wtf_only=False):
        environment = self.host.copy_current_environment().to_dictionary()
        try:
            self._run_script('build-api-tests', args=(['--wtf-only'] if wtf_only else []) + self._build_driver_flags(), env=environment)
        except ScriptError as e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _build_image_diff(self):
        environment = self.host.copy_current_environment()
        env = environment.to_dictionary()
        try:
            self._run_script("build-imagediff", env=env)
            self._path_to_image_diff.clear()
        except ScriptError as e:
            _log.error(e.message_with_output(output_limit=None))
            return False
        return True

    def _build_driver_flags(self):
        return []

    def test_search_path(self, device_type=None):
        return self.baseline_search_path(device_type=device_type)

    def _tests_for_other_platforms(self, device_type=None):
        # By default we will skip any directory under LayoutTests/platform
        # that isn't in our baseline search path (this mirrors what
        # old-run-webkit-tests does in findTestsToRun()).
        # Note this returns LayoutTests/platform/*, not platform/*/*.
        entries = self.all_baseline_search_paths(device_type=device_type)
        dirs_to_skip = []
        for entry in entries:
            if self._filesystem.isdir(entry) and entry not in self.test_search_path(device_type=device_type):
                basename = self._filesystem.basename(entry)
                dirs_to_skip.append('platform/%s' % basename)
        return dirs_to_skip

    def _wk2_port_name(self):
        # By current convention, the WebKit2 name is always mac-wk2, win-wk2, not mac-leopard-wk2, etc,
        return "%s-wk2" % self.port_name

    def logging_patterns_to_strip(self):
        return []

    def stderr_patterns_to_strip(self):
        return []

    def logging_detectors_to_strip_text_start(self, test_name):
        return []

    def test_expectations_file_position(self):
        # By default baseline search path schema is i.e. port-wk2 -> wk2 -> port -> generic, so port expectations file is at second to last position.
        return 1

    def did_spawn_worker(self, worker_number):
        # This is overridden by ports that need to do work in the parent process after a worker subprocess is spawned,
        # such as closing file descriptors that were implicitly cloned to the worker.
        pass

    def configuration_for_upload(self, host=None):
        from webkitpy.results.upload import Upload

        configuration = self.test_configuration()
        host = self.host or host

        if self.get_option('guard_malloc'):
            style = 'guard-malloc'
        elif self._config.asan:
            style = 'asan'
        else:
            style = configuration.build_type

        return Upload.create_configuration(
            platform=host.platform.os_name,
            version=str(host.platform.os_version),
            version_name=host.platform.os_version_name(INTERNAL_TABLE) or host.platform.os_version_name(),
            architecture=configuration.architecture,
            style=style,
            sdk=host.platform.build_version(),
            flavor=self.get_option('result_report_flavor'),
            model=self.get_option('model'),
        )

    @memoized
    def commits_for_upload(self):
        from webkitpy.results.upload import Upload

        repos = {}
        if port_config.apple_additions() and getattr(port_config.apple_additions(), 'repos', False):
            repos = {
                name: local.Scm.from_path(pth)
                for name, pth in port_config.apple_additions().repos().items()
            }

        if 'webkit' not in repos:
            try:
                repos['webkit'] = local.Scm.from_path(self.host.filesystem.getcwd())
            except OSError:
                repos['webkit'] = local.Scm.from_path(self.host.filesystem.dirname(__file__))

        commits = []
        for repo_id, repo in repos.items():
            if repo.is_git:
                # Git commits are completely defined locally, so upload the fully defined commit.
                commit = repo.commit()
                commit = commit.Encoder().default(commit)
                commit['repository_id'] = repo_id
                commits.append(commit)

            else:
                # Subversion commits require network requests to become fully defined, so provide partial commits
                # and let the backend handle them.
                commit = repo.commit(include_log=False, include_identifier=False)
                commits.append(Upload.create_commit(
                    repository_id=repo_id,
                    id=str(commit.revision or commit.hash),
                    branch=commit.branch,
                ))
        return commits
