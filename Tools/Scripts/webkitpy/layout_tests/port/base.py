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

"""Abstract base class of Port-specific entrypoints for the layout tests
test infrastructure (the Port and Driver classes)."""

from __future__ import with_statement

import cgi
import difflib
import errno
import os
import re

from webkitpy.common.memoized import memoized
from webkitpy.common.system import path


# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None

from webkitpy.common import find_files
from webkitpy.common.system import logutils
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.layout_tests import read_checksum_from_png
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port import config as port_config
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import http_lock
from webkitpy.layout_tests.servers import apache_http_server
from webkitpy.layout_tests.servers import http_server
from webkitpy.layout_tests.servers import websocket_server

_log = logutils.get_logger(__file__)


class DummyOptions(object):
    """Fake implementation of optparse.Values. Cloned from webkitpy.tool.mocktool.MockOptions."""

    def __init__(self, *args, **kwargs):
        # The caller can set option values using keyword arguments. We don't
        # set any values by default because we don't know how this
        # object will be used. Generally speaking unit tests should
        # subclass this or provider wrapper functions that set a common
        # set of options.
        for key, value in kwargs.items():
            self.__dict__[key] = value


# FIXME: This class should merge with WebKitPort now that Chromium behaves mostly like other webkit ports.
class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package."""

    port_name = None  # Subclasses override this

    # Test names resemble unix relative paths, and use '/' as a directory separator.
    TEST_PATH_SEPARATOR = '/'

    ALL_BUILD_TYPES = ('debug', 'release')

    def __init__(self, host, port_name=None, options=None, config=None, **kwargs):

        # These are default values that should be overridden in a subclasses.
        self._name = port_name or self.port_name  # Subclasses may append a -VERSION (like mac-leopard) or other qualifiers.
        self._version = ''
        self._architecture = 'x86'
        self._graphics_type = 'cpu'

        # FIXME: Ideally we'd have a package-wide way to get a
        # well-formed options object that had all of the necessary
        # options defined on it.
        self._options = options or DummyOptions()

        self.host = host
        self._executive = host.executive
        self._filesystem = host.filesystem
        self._config = config or port_config.Config(self._executive, self._filesystem)

        self._helper = None
        self._http_server = None
        self._websocket_server = None
        self._http_lock = None  # FIXME: Why does this live on the port object?

        # Python's Popen has a bug that causes any pipes opened to a
        # process that can't be executed to be leaked.  Since this
        # code is specifically designed to tolerate exec failures
        # to gracefully handle cases where wdiff is not installed,
        # the bug results in a massive file descriptor leak. As a
        # workaround, if an exec failure is ever experienced for
        # wdiff, assume it's not available.  This will leak one
        # file descriptor but that's better than leaking each time
        # wdiff would be run.
        #
        # http://mail.python.org/pipermail/python-list/
        #    2008-August/505753.html
        # http://bugs.python.org/issue3210
        self._wdiff_available = None

        # FIXME: prettypatch.py knows this path, why is it copied here?
        self._pretty_patch_path = self.path_from_webkit_base("Websites", "bugs.webkit.org", "PrettyPatch", "prettify.rb")
        self._pretty_patch_available = None

        self.set_option_default('configuration', self.default_configuration())
        self._test_configuration = None
        self._reftest_list = {}
        self._multiprocessing_is_available = (multiprocessing is not None)
        self._results_directory = None

    def wdiff_available(self):
        if self._wdiff_available is None:
            self._wdiff_available = self.check_wdiff(logging=False)
        return self._wdiff_available

    def pretty_patch_available(self):
        if self._pretty_patch_available is None:
            self._pretty_patch_available = self.check_pretty_patch(logging=False)
        return self._pretty_patch_available

    def default_child_processes(self):
        """Return the number of DumpRenderTree instances to use for this port."""
        cpu_count = self._executive.cpu_count()
        # Make sure we have enough ram to support that many instances:
        free_memory = self.host.platform.free_bytes_memory()
        if free_memory:
            bytes_per_drt = 200 * 1024 * 1024  # Assume each DRT needs 200MB to run.
            supportable_instances = max(free_memory / bytes_per_drt, 1)  # Always use one process, even if we don't have space for it.
            if supportable_instances < cpu_count:
                # FIXME: The Printer isn't initialized when this is called, so using _log would just show an unitialized logger error.
                print "This machine could support %s child processes, but only has enough memory for %s." % (cpu_count, supportable_instances)
            return min(supportable_instances, cpu_count)
        return cpu_count

    def default_worker_model(self):
        if self._multiprocessing_is_available:
            return 'processes'
        return 'inline'

    def baseline_path(self):
        """Return the absolute path to the directory to store new baselines in for this port."""
        baseline_search_paths = self.get_option('additional_platform_directory', []) + self.baseline_search_path()
        return baseline_search_paths[0]

    def baseline_search_path(self):
        """Return a list of absolute paths to directories to search under for
        baselines. The directories are searched in order."""
        raise NotImplementedError('Port.baseline_search_path')

    def check_build(self, needs_http):
        """This routine is used to ensure that the build is up to date
        and all the needed binaries are present."""
        raise NotImplementedError('Port.check_build')

    def check_sys_deps(self, needs_http):
        """If the port needs to do some runtime checks to ensure that the
        tests can be run successfully, it should override this routine.
        This step can be skipped with --nocheck-sys-deps.

        Returns whether the system is properly configured."""
        if needs_http:
            return self.check_httpd()
        return True

    def check_image_diff(self, override_step=None, logging=True):
        """This routine is used to check whether image_diff binary exists."""
        raise NotImplementedError('Port.check_image_diff')

    def check_pretty_patch(self, logging=True):
        """Checks whether we can use the PrettyPatch ruby script."""
        try:
            _ = self._executive.run_command(['ruby', '--version'])
        except OSError, e:
            if e.errno in [errno.ENOENT, errno.EACCES, errno.ECHILD]:
                if logging:
                    _log.error("Ruby is not installed; can't generate pretty patches.")
                    _log.error('')
                return False

        if not self._filesystem.exists(self._pretty_patch_path):
            if logging:
                _log.error("Unable to find %s; can't generate pretty patches." % self._pretty_patch_path)
                _log.error('')
            return False

        return True

    def check_wdiff(self, logging=True):
        if not self._path_to_wdiff():
            # Don't need to log here since this is the port choosing not to use wdiff.
            return False

        try:
            _ = self._executive.run_command([self._path_to_wdiff(), '--help'])
        except OSError:
            if logging:
                _log.error("wdiff is not installed.")
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

    def compare_text(self, expected_text, actual_text):
        """Return whether or not the two strings are *not* equal. This
        routine is used to diff text output.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        return expected_text != actual_text

    def compare_audio(self, expected_audio, actual_audio):
        # FIXME: If we give this method a better name it won't need this docstring (e.g. are_audio_results_equal()).
        """Return whether the two audio files are *not* equal."""
        return expected_audio != actual_audio

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        """Compare two images and return a tuple of an image diff, and a percentage difference (0-100).

        |tolerance| should be a percentage value (0.0 - 100.0).
        If it is omitted, the port default tolerance value is used.
        """
        raise NotImplementedError('Port.diff_image')


    def diff_text(self, expected_text, actual_text,
                  expected_filename, actual_filename):
        """Returns a string containing the diff of the two text strings
        in 'unified diff' format.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""

        # The filenames show up in the diff output, make sure they're
        # raw bytes and not unicode, so that they don't trigger join()
        # trying to decode the input.
        def to_raw_bytes(string_value):
            if isinstance(string_value, unicode):
                return string_value.encode('utf-8')
            return string_value
        expected_filename = to_raw_bytes(expected_filename)
        actual_filename = to_raw_bytes(actual_filename)
        diff = difflib.unified_diff(expected_text.splitlines(True),
                                    actual_text.splitlines(True),
                                    expected_filename,
                                    actual_filename)
        return ''.join(diff)

    def is_crash_reporter(self, process_name):
        return False

    def check_for_leaks(self, process_name, process_pid):
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
        """Returns the name of the actual binary that is performing the test,
        so that it can be referred to in log messages. In most cases this
        will be DumpRenderTree, but if a port uses a binary with a different
        name, it can be overridden here."""
        return "DumpRenderTree"

    def expected_baselines(self, test_name, suffix, all_baselines=False):
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
        baseline_filename = self._filesystem.splitext(test_name)[0] + '-expected' + suffix
        baseline_search_path = self.get_option('additional_platform_directory', []) + self.baseline_search_path()

        baselines = []
        for platform_dir in baseline_search_path:
            if self._filesystem.exists(self._filesystem.join(platform_dir, baseline_filename)):
                baselines.append((platform_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        # If it wasn't found in a platform directory, return the expected
        # result in the test directory, even if no such file actually exists.
        platform_dir = self.layout_tests_dir()
        if self._filesystem.exists(self._filesystem.join(platform_dir, baseline_filename)):
            baselines.append((platform_dir, baseline_filename))

        if baselines:
            return baselines

        return [(None, baseline_filename)]

    def expected_filename(self, test_name, suffix):
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
            search list of directories, e.g., 'chromium-win', or
            'chromium-cg-mac-leopard' (we follow the WebKit format)

        This routine is generic but is implemented here to live alongside
        the other baseline and filename manipulation routines.
        """
        # FIXME: The [0] here is very mysterious, as is the destructured return.
        platform_dir, baseline_filename = self.expected_baselines(test_name, suffix)[0]
        if platform_dir:
            return self._filesystem.join(platform_dir, baseline_filename)
        return self._filesystem.join(self.layout_tests_dir(), baseline_filename)

    def expected_checksum(self, test_name):
        """Returns the checksum of the image we expect the test to produce, or None if it is a text-only test."""
        png_path = self.expected_filename(test_name, '.png')

        if self._filesystem.exists(png_path):
            with self._filesystem.open_binary_file_for_reading(png_path) as filehandle:
                return read_checksum_from_png.read_checksum(filehandle)

        return None

    def expected_image(self, test_name):
        """Returns the image we expect the test to produce."""
        baseline_path = self.expected_filename(test_name, '.png')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_audio(self, test_name):
        baseline_path = self.expected_filename(test_name, '.wav')
        if not self._filesystem.exists(baseline_path):
            return None
        return self._filesystem.read_binary_file(baseline_path)

    def expected_text(self, test_name):
        """Returns the text output we expect the test to produce, or None
        if we don't expect there to be any text output.
        End-of-line characters are normalized to '\n'."""
        # FIXME: DRT output is actually utf-8, but since we don't decode the
        # output from DRT (instead treating it as a binary string), we read the
        # baselines as a binary string, too.
        baseline_path = self.expected_filename(test_name, '.txt')
        if not self._filesystem.exists(baseline_path):
            baseline_path = self.expected_filename(test_name, '.webarchive')
            if not self._filesystem.exists(baseline_path):
                return None
        text = self._filesystem.read_binary_file(baseline_path)
        return text.replace("\r\n", "\n")

    def _get_reftest_list(self, test_name):
        dirname = self._filesystem.join(self.layout_tests_dir(), self._filesystem.dirname(test_name))
        if dirname not in self._reftest_list:
            self._reftest_list[dirname] = Port._parse_reftest_list(self._filesystem, dirname)
        return self._reftest_list[dirname]

    @staticmethod
    def _parse_reftest_list(filesystem, test_dirpath):
        reftest_list_path = filesystem.join(test_dirpath, 'reftest.list')
        if not filesystem.isfile(reftest_list_path):
            return None
        reftest_list_file = filesystem.read_text_file(reftest_list_path)

        parsed_list = {}
        for line in reftest_list_file.split('\n'):
            line = re.sub('#.+$', '', line)
            split_line = line.split()
            if len(split_line) < 3:
                continue
            expectation_type, test_file, ref_file = split_line
            parsed_list.setdefault(filesystem.join(test_dirpath, test_file), []).append((expectation_type, filesystem.join(test_dirpath, ref_file)))
        return parsed_list

    def reference_files(self, test_name):
        """Return a list of expectation (== or !=) and filename pairs"""

        reftest_list = self._get_reftest_list(test_name)
        if not reftest_list:
            expected_filenames = [('==', self.expected_filename(test_name, '.html')), ('!=', self.expected_filename(test_name, '-mismatch.html'))]
            return [(expectation, filename) for expectation, filename in expected_filenames if self._filesystem.exists(filename)]

        return reftest_list.get(self._filesystem.join(self.layout_tests_dir(), test_name), [])

    def is_reftest(self, test_name):
        reftest_list = self._get_reftest_list(test_name)
        if not reftest_list:
            has_expected = self._filesystem.exists(self.expected_filename(test_name, '.html'))
            return has_expected or self._filesystem.exists(self.expected_filename(test_name, '-mismatch.html'))
        filename = self._filesystem.join(self.layout_tests_dir(), test_name)
        return filename in reftest_list

    def tests(self, paths):
        """Return the list of tests found."""
        # When collecting test cases, skip these directories
        skipped_directories = set(['.svn', '_svn', 'resources', 'script-tests', 'reference', 'reftest'])
        files = find_files.find(self._filesystem, self.layout_tests_dir(), paths, skipped_directories, Port._is_test_file)
        return set([self.relative_test_filename(f) for f in files])

    # When collecting test cases, we include any file with these extensions.
    _supported_file_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl',
                                      '.htm', '.php', '.svg', '.mht'])

    @staticmethod
    def is_reference_html_file(filesystem, dirname, filename):
        if filename.startswith('ref-') or filename.endswith('notref-'):
            return True
        filename_wihout_ext, unused = filesystem.splitext(filename)
        for suffix in ['-expected', '-expected-mismatch', '-ref', '-notref']:
            if filename_wihout_ext.endswith(suffix):
                return True
        return False

    @staticmethod
    def _has_supported_extension(filesystem, filename):
        """Return true if filename is one of the file extensions we want to run a test on."""
        extension = filesystem.splitext(filename)[1]
        return extension in Port._supported_file_extensions

    @staticmethod
    def _is_test_file(filesystem, dirname, filename):
        return Port._has_supported_extension(filesystem, filename) and not Port.is_reference_html_file(filesystem, dirname, filename)

    def test_dirs(self):
        """Returns the list of top-level test directories."""
        layout_tests_dir = self.layout_tests_dir()
        return filter(lambda x: self._filesystem.isdir(self._filesystem.join(layout_tests_dir, x)),
                      self._filesystem.listdir(layout_tests_dir))

    def test_isdir(self, test_name):
        """Return True if the test name refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        test_path = self.abspath_for_test(test_name)
        return self._filesystem.isdir(test_path)

    def test_exists(self, test_name):
        """Return True if the test name refers to an existing test or baseline."""
        # Used by test_expectations.py to determine if an entry refers to a
        # valid test and by printing.py to determine if baselines exist.
        test_path = self.abspath_for_test(test_name)
        return self._filesystem.exists(test_path)

    def split_test(self, test_name):
        """Splits a test name into the 'directory' part and the 'basename' part."""
        index = test_name.rfind(self.TEST_PATH_SEPARATOR)
        if index < 1:
            return ('', test_name)
        return (test_name[0:index], test_name[index:])

    def normalize_test_name(self, test_name):
        """Returns a normalized version of the test name or test directory."""
        if self.test_isdir(test_name) and not test_name.endswith('/'):
            return test_name + '/'
        return test_name

    def driver_cmd_line(self):
        """Prints the DRT command line that will be used."""
        driver = self.create_driver(0)
        return driver.cmd_line()

    def update_baseline(self, baseline_path, data):
        """Updates the baseline for a test.

        Args:
            baseline_path: the actual path to use for baseline, not the path to
              the test. This function is used to update either generic or
              platform-specific baselines, but we can't infer which here.
            data: contents of the baseline.
        """
        self._filesystem.write_binary_file(baseline_path, data)

    def layout_tests_dir(self):
        """Return the absolute path to the top of the LayoutTests directory."""
        return self.path_from_webkit_base('LayoutTests')

    def webkit_base(self):
        return self._filesystem.abspath(self.path_from_webkit_base('.'))

    def skipped_layout_tests(self):
        return []

    def skipped_tests(self):
        return []

    def skips_layout_test(self, test_name):
        """Figures out if the givent test is being skipped or not.

        Test categories are handled as well."""
        for test_or_category in self.skipped_layout_tests():
            if test_or_category == test_name:
                return True
            category = self._filesystem.join(self.layout_tests_dir(), test_or_category)
            if self._filesystem.isdir(category) and test_name.startswith(test_or_category):
                return True
        return False

    def maybe_make_directory(self, *comps):
        """Creates the specified directory if it doesn't already exist."""
        self._filesystem.maybe_make_directory(*comps)

    def name(self):
        """Returns a name that uniquely identifies this particular type of port
        (e.g., "mac-snowleopard" or "chromium-gpu-linux-x86_x64" and can be passed
        to factory.get() to instantiate the port."""
        return self._name

    def real_name(self):
        # FIXME: Seems this is only used for MockDRT and should be removed.
        """Returns the name of the port as passed to the --platform command line argument."""
        return self.name()

    def operating_system(self):
        # Subclasses should override this default implementation.
        return 'mac'

    def version(self):
        """Returns a string indicating the version of a given platform, e.g.
        'leopard' or 'xp'.

        This is used to help identify the exact port when parsing test
        expectations, determining search paths, and logging information."""
        return self._version

    def graphics_type(self):
        """Returns whether the port uses accelerated graphics ('gpu') or not ('cpu')."""
        return self._graphics_type

    def architecture(self):
        return self._architecture

    def get_option(self, name, default_value=None):
        # FIXME: Eventually we should not have to do a test for
        # hasattr(), and we should be able to just do
        # self.options.value. See additional FIXME in the constructor.
        if hasattr(self._options, name):
            return getattr(self._options, name)
        return default_value

    def set_option_default(self, name, default_value):
        # FIXME: Callers could also use optparse_parser.Values.ensure_value,
        # since this should always be a optparse_parser.Values object.
        if not hasattr(self._options, name) or getattr(self._options, name) is None:
            return setattr(self._options, name, default_value)

    def path_from_webkit_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        WebKit source tree and the list of path components in |*comps|."""
        return self._config.path_from_webkit_base(*comps)

    def path_to_test_expectations_file(self):
        """Update the test expectations to the passed-in string.

        This is used by the rebaselining tool. Raises NotImplementedError
        if the port does not use expectations files."""
        raise NotImplementedError('Port.path_to_test_expectations_file')

    def relative_test_filename(self, filename):
        """Returns a test_name a realtive unix-style path for a filename under the LayoutTests
        directory. Filenames outside the LayoutTests directory should raise
        an error."""
        # Ports that run on windows need to override this method to deal with
        # filenames with backslashes in them.
        assert filename.startswith(self.layout_tests_dir()), "%s did not start with %s" % (filename, self.layout_tests_dir())
        return filename[len(self.layout_tests_dir()) + 1:]

    def abspath_for_test(self, test_name):
        """Returns the full path to the file for a given test name. This is the
        inverse of relative_test_filename()."""
        return self._filesystem.normpath(self._filesystem.join(self.layout_tests_dir(), test_name))

    def results_directory(self):
        """Absolute path to the place to store the test results (uses --results-directory)."""
        if not self._results_directory:
            option_val = self.get_option('results_directory') or self.default_results_directory()
            self._results_directory = self._filesystem.abspath(option_val)
        return self._results_directory

    def default_results_directory(self):
        """Absolute path to the default place to store the test results."""
        raise NotImplementedError()

    def setup_test_run(self):
        """Perform port-specific work at the beginning of a test run."""
        pass

    # FIXME: os.environ access should be moved to onto a common/system class to be more easily mockable.
    def _value_or_default_from_environ(self, name, default=None):
        if name in os.environ:
            return os.environ[name]
        return default

    def _copy_value_from_environ_if_set(self, clean_env, name):
        if name in os.environ:
            clean_env[name] = os.environ[name]

    def setup_environ_for_server(self, server_name=None):
        # We intentionally copy only a subset of os.environ when
        # launching subprocesses to ensure consistent test results.
        clean_env = {}
        variables_to_copy = [
            # For Linux:
            'XAUTHORITY',
            'HOME',
            'LANG',
            'LD_LIBRARY_PATH',
            'DBUS_SESSION_BUS_ADDRESS',

            # Darwin:
            'DYLD_LIBRARY_PATH',
            'HOME',

            # CYGWIN:
            'HOMEDRIVE',
            'HOMEPATH',
            '_NT_SYMBOL_PATH',

            # Windows:
            'PATH',
        ]
        for variable in variables_to_copy:
            self._copy_value_from_environ_if_set(clean_env, variable)

        # For Linux:
        clean_env['DISPLAY'] = self._value_or_default_from_environ('DISPLAY', ':1')
        return clean_env

    def show_results_html_file(self, results_filename):
        """This routine should display the HTML file pointed at by
        results_filename in a users' browser."""
        return self.host.user.open_url(path.abspath_to_uri(results_filename))

    def create_driver(self, worker_number):
        """Return a newly created Driver subclass for starting/stopping the test driver."""
        return driver.DriverProxy(self, worker_number, self._driver_class(), pixel_tests=self.get_option('pixel_tests'))

    def start_helper(self):
        """If a port needs to reconfigure graphics settings or do other
        things to ensure a known test configuration, it should override this
        method."""
        pass

    def start_http_server(self):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a web server to be running."""
        assert not self._http_server, 'Already running an http server.'

        if self._uses_apache():
            server = apache_http_server.LayoutTestApacheHttpd(self, self.results_directory())
        else:
            server = http_server.Lighttpd(self, self.results_directory())

        server.start()
        self._http_server = server

    def start_websocket_server(self):
        """Start a web server. Raise an error if it can't start or is already running.

        Ports can stub this out if they don't need a websocket server to be running."""
        assert not self._websocket_server, 'Already running a websocket server.'

        server = websocket_server.PyWebSocket(self, self.results_directory())
        server.start()
        self._websocket_server = server

    def acquire_http_lock(self):
        self._http_lock = http_lock.HttpLock(None, filesystem=self._filesystem, executive=self._executive)
        self._http_lock.wait_for_httpd_lock()

    def stop_helper(self):
        """Shut down the test helper if it is running. Do nothing if
        it isn't, or it isn't available. If a port overrides start_helper()
        it must override this routine as well."""
        pass

    def stop_http_server(self):
        """Shut down the http server if it is running. Do nothing if it isn't."""
        if self._http_server:
            self._http_server.stop()
            self._http_server = None

    def stop_websocket_server(self):
        """Shut down the websocket server if it is running. Do nothing if it isn't."""
        if self._websocket_server:
            self._websocket_server.stop()
            self._websocket_server = None

    def release_http_lock(self):
        if self._http_lock:
            self._http_lock.cleanup_http_lock()

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
            self._test_configuration = TestConfiguration(self._version, self._architecture, self._options.configuration.lower(), self._graphics_type)
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
        return self._filesystem.exists(self.path_to_test_expectations_file())

    def test_expectations(self):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        return self._filesystem.read_text_file(self.path_to_test_expectations_file())

    def test_expectations_overrides(self):
        """Returns an optional set of overrides for the test_expectations.

        This is used by ports that have code in two repositories, and where
        it is possible that you might need "downstream" expectations that
        temporarily override the "upstream" expectations until the port can
        sync up the two repos."""
        return None

    def test_repository_paths(self):
        """Returns a list of (repository_name, repository_path) tuples
        of its depending code base.  By default it returns a list that only
        contains a ('webkit', <webkitRepossitoryPath>) tuple.
        """
        return [('webkit', self.layout_tests_dir())]


    _WDIFF_DEL = '##WDIFF_DEL##'
    _WDIFF_ADD = '##WDIFF_ADD##'
    _WDIFF_END = '##WDIFF_END##'

    def _format_wdiff_output_as_html(self, wdiff):
        wdiff = cgi.escape(wdiff)
        wdiff = wdiff.replace(self._WDIFF_DEL, "<span class=del>")
        wdiff = wdiff.replace(self._WDIFF_ADD, "<span class=add>")
        wdiff = wdiff.replace(self._WDIFF_END, "</span>")
        html = "<head><style>.del { background: #faa; } "
        html += ".add { background: #afa; }</style></head>"
        html += "<pre>%s</pre>" % wdiff
        return html

    def _wdiff_command(self, actual_filename, expected_filename):
        executable = self._path_to_wdiff()
        return [executable,
                "--start-delete=%s" % self._WDIFF_DEL,
                "--end-delete=%s" % self._WDIFF_END,
                "--start-insert=%s" % self._WDIFF_ADD,
                "--end-insert=%s" % self._WDIFF_END,
                actual_filename,
                expected_filename]

    @staticmethod
    def _handle_wdiff_error(script_error):
        # Exit 1 means the files differed, any other exit code is an error.
        if script_error.exit_code != 1:
            raise script_error

    def _run_wdiff(self, actual_filename, expected_filename):
        """Runs wdiff and may throw exceptions.
        This is mostly a hook for unit testing."""
        # Diffs are treated as binary as they may include multiple files
        # with conflicting encodings.  Thus we do not decode the output.
        command = self._wdiff_command(actual_filename, expected_filename)
        wdiff = self._executive.run_command(command, decode_output=False,
            error_handler=self._handle_wdiff_error)
        return self._format_wdiff_output_as_html(wdiff)

    def wdiff_text(self, actual_filename, expected_filename):
        """Returns a string of HTML indicating the word-level diff of the
        contents of the two filenames. Returns an empty string if word-level
        diffing isn't available."""
        if not self.wdiff_available():
            return ""
        try:
            # It's possible to raise a ScriptError we pass wdiff invalid paths.
            return self._run_wdiff(actual_filename, expected_filename)
        except OSError, e:
            if e.errno in [errno.ENOENT, errno.EACCES, errno.ECHILD]:
                # Silently ignore cases where wdiff is missing.
                self._wdiff_available = False
                return ""
            raise

    # This is a class variable so we can test error output easily.
    _pretty_patch_error_html = "Failed to run PrettyPatch, see error log."

    def pretty_patch_text(self, diff_path):
        if self._pretty_patch_available is None:
            self._pretty_patch_available = self.check_pretty_patch(logging=False)
        if not self._pretty_patch_available:
            return self._pretty_patch_error_html
        command = ("ruby", "-I", self._filesystem.dirname(self._pretty_patch_path),
                   self._pretty_patch_path, diff_path)
        try:
            # Diffs are treated as binary (we pass decode_output=False) as they
            # may contain multiple files of conflicting encodings.
            return self._executive.run_command(command, decode_output=False)
        except OSError, e:
            # If the system is missing ruby log the error and stop trying.
            self._pretty_patch_available = False
            _log.error("Failed to run PrettyPatch (%s): %s" % (command, e))
            return self._pretty_patch_error_html
        except ScriptError, e:
            # If ruby failed to run for some reason, log the command
            # output and stop trying.
            self._pretty_patch_available = False
            _log.error("Failed to run PrettyPatch (%s):\n%s" % (command, e.message_with_output()))
            return self._pretty_patch_error_html

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

    def _path_to_apache(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module."""
        raise NotImplementedError('Port.path_to_apache')

    def _path_to_apache_config_file(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module."""
        raise NotImplementedError('Port.path_to_apache_config_file')

    def _path_to_driver(self, configuration=None):
        """Returns the full path to the test driver (DumpRenderTree)."""
        raise NotImplementedError('Port._path_to_driver')

    def _path_to_webcore_library(self):
        """Returns the full path to a built copy of WebCore."""
        raise NotImplementedError('Port.path_to_webcore_library')

    def _path_to_helper(self):
        """Returns the full path to the layout_test_helper binary, which
        is used to help configure the system for the test run, or None
        if no helper is needed.

        This is likely only used by start/stop_helper()."""
        raise NotImplementedError('Port._path_to_helper')

    def _path_to_image_diff(self):
        """Returns the full path to the image_diff binary, or None if it is not available.

        This is likely used only by diff_image()"""
        raise NotImplementedError('Port.path_to_image_diff')

    def _path_to_lighttpd(self):
        """Returns the path to the LigHTTPd binary.

        This is needed only by ports that use the http_server.py module."""
        raise NotImplementedError('Port._path_to_lighttpd')

    def _path_to_lighttpd_modules(self):
        """Returns the path to the LigHTTPd modules directory.

        This is needed only by ports that use the http_server.py module."""
        raise NotImplementedError('Port._path_to_lighttpd_modules')

    def _path_to_lighttpd_php(self):
        """Returns the path to the LigHTTPd PHP executable.

        This is needed only by ports that use the http_server.py module."""
        raise NotImplementedError('Port._path_to_lighttpd_php')

    def _path_to_wdiff(self):
        """Returns the full path to the wdiff binary, or None if it is not available.

        This is likely used only by wdiff_text()"""
        raise NotImplementedError('Port._path_to_wdiff')

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
        raise NotImplementedError('Port._driver_class')
