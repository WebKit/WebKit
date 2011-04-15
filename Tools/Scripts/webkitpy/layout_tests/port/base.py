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
import shlex
import sys
import time

# Handle Python < 2.6 where multiprocessing isn't available.
try:
    import multiprocessing
except ImportError:
    multiprocessing = None

from webkitpy.common import system
from webkitpy.common.system import filesystem
from webkitpy.common.system import logutils
from webkitpy.common.system import path
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.user import User
from webkitpy.layout_tests import read_checksum_from_png
from webkitpy.layout_tests.port import apache_http_server
from webkitpy.layout_tests.port import config as port_config
from webkitpy.layout_tests.port import http_lock
from webkitpy.layout_tests.port import http_server
from webkitpy.layout_tests.port import test_files
from webkitpy.layout_tests.port import websocket_server

_log = logutils.get_logger(__file__)


class DummyOptions(object):
    """Fake implementation of optparse.Values. Cloned from
    webkitpy.tool.mocktool.MockOptions.

    """

    def __init__(self, **kwargs):
        # The caller can set option values using keyword arguments. We don't
        # set any values by default because we don't know how this
        # object will be used. Generally speaking unit tests should
        # subclass this or provider wrapper functions that set a common
        # set of options.
        for key, value in kwargs.items():
            self.__dict__[key] = value


# FIXME: This class should merge with webkitpy.webkit_port at some point.
class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package."""

    def __init__(self, port_name=None, options=None,
                 executive=None,
                 user=None,
                 filesystem=None,
                 config=None,
                 **kwargs):
        self._name = port_name

        # These are default values that should be overridden in a subclasses.
        # FIXME: These should really be passed in.
        self._operating_system = 'mac'
        self._version = ''
        self._architecture = 'x86'
        self._graphics_type = 'cpu'

        # FIXME: Ideally we'd have a package-wide way to get a
        # well-formed options object that had all of the necessary
        # options defined on it.
        self._options = options or DummyOptions()

        self._executive = executive or Executive()
        self._user = user or User()
        self._filesystem = filesystem or system.filesystem.FileSystem()
        self._config = config or port_config.Config(self._executive, self._filesystem)

        self._helper = None
        self._http_server = None
        self._webkit_base_dir = None
        self._websocket_server = None
        self._http_lock = None

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
        self._wdiff_available = True

        # FIXME: prettypatch.py knows this path, why is it copied here?
        self._pretty_patch_path = self.path_from_webkit_base("Websites",
            "bugs.webkit.org", "PrettyPatch", "prettify.rb")
        self._pretty_patch_available = None

        if not hasattr(self._options, 'configuration') or self._options.configuration is None:
            self._options.configuration = self.default_configuration()
        self._test_configuration = None
        self._multiprocessing_is_available = (multiprocessing is not None)
        self._results_directory = None

    def wdiff_available(self):
        return bool(self._wdiff_available)

    def pretty_patch_available(self):
        return bool(self._pretty_patch_available)

    def default_child_processes(self):
        """Return the number of DumpRenderTree instances to use for this
        port."""
        return self._executive.cpu_count()

    def default_worker_model(self):
        if self._multiprocessing_is_available:
            return 'processes'
        return 'threads'

    def baseline_path(self):
        """Return the absolute path to the directory to store new baselines
        in for this port."""
        raise NotImplementedError('Port.baseline_path')

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
        return True

    def check_image_diff(self, override_step=None, logging=True):
        """This routine is used to check whether image_diff binary exists."""
        raise NotImplementedError('Port.check_image_diff')

    def check_pretty_patch(self, logging=True):
        """Checks whether we can use the PrettyPatch ruby script."""
        # check if Ruby is installed
        try:
            result = self._executive.run_command(['ruby', '--version'])
        except OSError, e:
            if e.errno in [errno.ENOENT, errno.EACCES, errno.ECHILD]:
                if logging:
                    _log.error("Ruby is not installed; can't generate pretty patches.")
                    _log.error('')
                return False

        if not self.path_exists(self._pretty_patch_path):
            if logging:
                _log.error("Unable to find %s; can't generate pretty patches." % self._pretty_patch_path)
                _log.error('')
            return False

        return True

    def compare_text(self, expected_text, actual_text):
        """Return whether or not the two strings are *not* equal. This
        routine is used to diff text output.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        return expected_text != actual_text

    def compare_audio(self, expected_audio, actual_audio):
        """Return whether the two audio files are *not* equal."""
        return expected_audio != actual_audio

    def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None, tolerance=0):
        """Compare two images and produce a delta image file.

        Return True if the two images are different, False if they are the same.
        Also produce a delta image of the two images and write that into
        |diff_filename| if it is not None.

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
        def to_raw_bytes(str):
            if isinstance(str, unicode):
                return str.encode('utf-8')
            return str
        expected_filename = to_raw_bytes(expected_filename)
        actual_filename = to_raw_bytes(actual_filename)
        diff = difflib.unified_diff(expected_text.splitlines(True),
                                    actual_text.splitlines(True),
                                    expected_filename,
                                    actual_filename)
        return ''.join(diff)

    def driver_name(self):
        """Returns the name of the actual binary that is performing the test,
        so that it can be referred to in log messages. In most cases this
        will be DumpRenderTree, but if a port uses a binary with a different
        name, it can be overridden here."""
        return "DumpRenderTree"

    def expected_baselines(self, filename, suffix, all_baselines=False):
        """Given a test name, finds where the baseline results are located.

        Args:
        filename: absolute filename to test file
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
        testname = self._filesystem.splitext(self.relative_test_filename(filename))[0]

        baseline_filename = testname + '-expected' + suffix

        baseline_search_path = self.get_option('additional_platform_directory', []) + self.baseline_search_path()

        baselines = []
        for platform_dir in baseline_search_path:
            if self.path_exists(self._filesystem.join(platform_dir,
                                                      baseline_filename)):
                baselines.append((platform_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        # If it wasn't found in a platform directory, return the expected
        # result in the test directory, even if no such file actually exists.
        platform_dir = self.layout_tests_dir()
        if self.path_exists(self._filesystem.join(platform_dir,
                                                  baseline_filename)):
            baselines.append((platform_dir, baseline_filename))

        if baselines:
            return baselines

        return [(None, baseline_filename)]

    def expected_filename(self, filename, suffix):
        """Given a test name, returns an absolute path to its expected results.

        If no expected results are found in any of the searched directories,
        the directory in which the test itself is located will be returned.
        The return value is in the format appropriate for the platform
        (e.g., "\\" for path separators on windows).

        Args:
        filename: absolute filename to test file
        suffix: file suffix of the expected results, including dot; e.g. '.txt'
            or '.png'.  This should not be None, but may be an empty string.
        platform: the most-specific directory name to use to build the
            search list of directories, e.g., 'chromium-win', or
            'chromium-mac-leopard' (we follow the WebKit format)

        This routine is generic but is implemented here to live alongside
        the other baseline and filename manipulation routines.
        """
        platform_dir, baseline_filename = self.expected_baselines(
            filename, suffix)[0]
        if platform_dir:
            return self._filesystem.join(platform_dir, baseline_filename)
        return self._filesystem.join(self.layout_tests_dir(), baseline_filename)

    def expected_checksum(self, test):
        """Returns the checksum of the image we expect the test to produce, or None if it is a text-only test."""
        png_path = self.expected_filename(test, '.png')
        checksum_path = self._filesystem.splitext(png_path)[0] + '.checksum'

        if self.path_exists(checksum_path):
            return self._filesystem.read_binary_file(checksum_path)

        if self.path_exists(png_path):
            with self._filesystem.open_binary_file_for_reading(png_path) as filehandle:
                return read_checksum_from_png.read_checksum(filehandle)

        return None

    def expected_image(self, test):
        """Returns the image we expect the test to produce."""
        path = self.expected_filename(test, '.png')
        if not self.path_exists(path):
            return None
        return self._filesystem.read_binary_file(path)

    def expected_audio(self, test):
        path = self.expected_filename(test, '.wav')
        if not self.path_exists(path):
            return None
        return self._filesystem.read_binary_file(path)

    def expected_text(self, test):
        """Returns the text output we expect the test to produce, or None
        if we don't expect there to be any text output.
        End-of-line characters are normalized to '\n'."""
        # FIXME: DRT output is actually utf-8, but since we don't decode the
        # output from DRT (instead treating it as a binary string), we read the
        # baselines as a binary string, too.
        path = self.expected_filename(test, '.txt')
        if not self.path_exists(path):
            return None
        text = self._filesystem.read_binary_file(path)
        return text.replace("\r\n", "\n")

    def reftest_expected_filename(self, filename):
        """Return the filename of reference we expect the test matches."""
        return self.expected_filename(filename, '.html')

    def reftest_expected_mismatch_filename(self, filename):
        """Return the filename of reference we don't expect the test matches."""
        return self.expected_filename(filename, '-mismatch.html')

    def filename_to_uri(self, filename):
        """Convert a test file (which is an absolute path) to a URI."""
        LAYOUTTEST_HTTP_DIR = "http/tests/"
        LAYOUTTEST_WEBSOCKET_DIR = "http/tests/websocket/tests/"

        relative_path = self.relative_test_filename(filename)
        port = None
        use_ssl = False

        if (relative_path.startswith(LAYOUTTEST_WEBSOCKET_DIR)
            or relative_path.startswith(LAYOUTTEST_HTTP_DIR)):
            relative_path = relative_path[len(LAYOUTTEST_HTTP_DIR):]
            port = 8000

        # Make http/tests/local run as local files. This is to mimic the
        # logic in run-webkit-tests.
        #
        # TODO(dpranke): remove the SSL reference?
        if (port and not relative_path.startswith("local/")):
            if relative_path.startswith("ssl/"):
                port += 443
                protocol = "https"
            else:
                protocol = "http"
            return "%s://127.0.0.1:%u/%s" % (protocol, port, relative_path)

        return path.abspath_to_uri(self._filesystem.abspath(filename))

    def tests(self, paths):
        """Return the list of tests found (relative to layout_tests_dir()."""
        return test_files.find(self, paths)

    def test_dirs(self):
        """Returns the list of top-level test directories.

        Used by --clobber-old-results."""
        layout_tests_dir = self.layout_tests_dir()
        return filter(lambda x: self._filesystem.isdir(self._filesystem.join(layout_tests_dir, x)),
                      self._filesystem.listdir(layout_tests_dir))

    def path_isdir(self, path):
        """Return True if the path refers to a directory of tests."""
        # Used by test_expectations.py to apply rules to whole directories.
        return self._filesystem.isdir(path)

    def path_exists(self, path):
        """Return True if the path refers to an existing test or baseline."""
        # Used by test_expectations.py to determine if an entry refers to a
        # valid test and by printing.py to determine if baselines exist.
        return self._filesystem.exists(path)

    def driver_cmd_line(self):
        """Prints the DRT command line that will be used."""
        driver = self.create_driver(0)
        return driver.cmd_line()

    def update_baseline(self, path, data):
        """Updates the baseline for a test.

        Args:
            path: the actual path to use for baseline, not the path to
              the test. This function is used to update either generic or
              platform-specific baselines, but we can't infer which here.
            data: contents of the baseline.
        """
        self._filesystem.write_binary_file(path, data)

    def uri_to_test_name(self, uri):
        """Return the base layout test name for a given URI.

        This returns the test name for a given URI, e.g., if you passed in
        "file:///src/LayoutTests/fast/html/keygen.html" it would return
        "fast/html/keygen.html".

        """
        test = uri
        if uri.startswith("file:///"):
            prefix = path.abspath_to_uri(self.layout_tests_dir()) + "/"
            return test[len(prefix):]

        if uri.startswith("http://127.0.0.1:8880/"):
            # websocket tests
            return test.replace('http://127.0.0.1:8880/', '')

        if uri.startswith("http://"):
            # regular HTTP test
            return test.replace('http://127.0.0.1:8000/', 'http/tests/')

        if uri.startswith("https://"):
            return test.replace('https://127.0.0.1:8443/', 'http/tests/')

        raise NotImplementedError('unknown url type: %s' % uri)

    def layout_tests_dir(self):
        """Return the absolute path to the top of the LayoutTests directory."""
        return self.path_from_webkit_base('LayoutTests')

    def skips_layout_test(self, test_name):
        """Figures out if the givent test is being skipped or not.

        Test categories are handled as well."""
        for test_or_category in self.skipped_layout_tests():
            if test_or_category == test_name:
                return True
            category = self._filesystem.join(self.layout_tests_dir(),
                                             test_or_category)
            if (self._filesystem.isdir(category) and
                test_name.startswith(test_or_category)):
                return True
        return False

    def maybe_make_directory(self, *path):
        """Creates the specified directory if it doesn't already exist."""
        self._filesystem.maybe_make_directory(*path)

    def name(self):
        """Return the name of the port (e.g., 'mac', 'chromium-win-xp')."""
        return self._name

    def operating_system(self):
        return self._operating_system

    def version(self):
        """Returns a string indicating the version of a given platform, e.g.
        'leopard' or 'xp'.

        This is used to help identify the exact port when parsing test
        expectations, determining search paths, and logging information."""
        return self._version

    def graphics_type(self):
        """Returns whether the port uses accelerated graphics ('gpu') or not
        ('cpu')."""
        return self._graphics_type

    def architecture(self):
        return self._architecture

    def real_name(self):
        """Returns the actual name of the port, not the delegate's."""
        return self.name()

    def get_option(self, name, default_value=None):
        # FIXME: Eventually we should not have to do a test for
        # hasattr(), and we should be able to just do
        # self.options.value. See additional FIXME in the constructor.
        if hasattr(self._options, name):
            return getattr(self._options, name)
        return default_value

    def set_option_default(self, name, default_value):
        if not hasattr(self._options, name):
            return setattr(self._options, name, default_value)

    def path_from_webkit_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        WebKit source tree and the list of path components in |*comps|."""
        return self._config.path_from_webkit_base(*comps)

    def script_path(self, script_name):
        return self._config.script_path(script_name)

    def script_shell_command(self, script_name):
        return self._config.script_shell_command(script_name)

    def path_to_test_expectations_file(self):
        """Update the test expectations to the passed-in string.

        This is used by the rebaselining tool. Raises NotImplementedError
        if the port does not use expectations files."""
        raise NotImplementedError('Port.path_to_test_expectations_file')

    def relative_test_filename(self, filename):
        """Relative unix-style path for a filename under the LayoutTests
        directory. Filenames outside the LayoutTests directory should raise
        an error."""
        # FIXME: On Windows, does this return test_names with forward slashes,
        # or windows-style relative paths?
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

    def setup_environ_for_server(self):
        """Perform port-specific work at the beginning of a server launch.

        Returns:
           Operating-system's environment.
        """
        return os.environ.copy()

    def show_results_html_file(self, results_filename):
        """This routine should display the HTML file pointed at by
        results_filename in a users' browser."""
        return self._user.open_url(results_filename)

    def create_driver(self, worker_number):
        """Return a newly created base.Driver subclass for starting/stopping
        the test driver."""
        raise NotImplementedError('Port.create_driver')

    def start_helper(self):
        """If a port needs to reconfigure graphics settings or do other
        things to ensure a known test configuration, it should override this
        method."""
        pass

    def start_http_server(self):
        """Start a web server if it is available. Do nothing if
        it isn't. This routine is allowed to (and may) fail if a server
        is already running."""
        if self.get_option('use_apache'):
            self._http_server = apache_http_server.LayoutTestApacheHttpd(self,
                self.results_directory())
        else:
            self._http_server = http_server.Lighttpd(self, self.results_directory())
        self._http_server.start()

    def start_websocket_server(self):
        """Start a websocket server if it is available. Do nothing if
        it isn't. This routine is allowed to (and may) fail if a server
        is already running."""
        self._websocket_server = websocket_server.PyWebSocket(self, self.results_directory())
        self._websocket_server.start()

    def acquire_http_lock(self):
        self._http_lock = http_lock.HttpLock(None)
        self._http_lock.wait_for_httpd_lock()

    def stop_helper(self):
        """Shut down the test helper if it is running. Do nothing if
        it isn't, or it isn't available. If a port overrides start_helper()
        it must override this routine as well."""
        pass

    def stop_http_server(self):
        """Shut down the http server if it is running. Do nothing if
        it isn't, or it isn't available."""
        if self._http_server:
            self._http_server.stop()

    def stop_websocket_server(self):
        """Shut down the websocket server if it is running. Do nothing if
        it isn't, or it isn't available."""
        if self._websocket_server:
            self._websocket_server.stop()

    def release_http_lock(self):
        if self._http_lock:
            self._http_lock.cleanup_http_lock()

    #
    # TEST EXPECTATION-RELATED METHODS
    #

    def test_configuration(self):
        """Returns the current TestConfiguration for the port."""
        if not self._test_configuration:
            self._test_configuration = TestConfiguration(self)
        return self._test_configuration

    def all_test_configurations(self):
        return self.test_configuration().all_test_configurations()

    def all_baseline_variants(self):
        """Returns a list of platform names sufficient to cover all the baselines.

        The list should be sorted so that a later platform  will reuse
        an earlier platform's baselines if they are the same (e.g.,
        'snowleopard' should precede 'leopard')."""
        raise NotImplementedError

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
        if not self._wdiff_available:
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
            _log.error("Failed to run PrettyPatch (%s):\n%s" % (command,
                       e.message_with_output()))
            return self._pretty_patch_error_html

    def default_configuration(self):
        return self._config.default_configuration()

    #
    # PROTECTED ROUTINES
    #
    # The routines below should only be called by routines in this class
    # or any of its subclasses.
    #
    def _webkit_build_directory(self, args):
        return self._config.build_directory(args[0])

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
        """Returns the full path to the image_diff binary, or None if it
        is not available.

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
        """Returns the full path to the wdiff binary, or None if it is
        not available.

        This is likely used only by wdiff_text()"""
        raise NotImplementedError('Port._path_to_wdiff')

    def _shut_down_http_server(self, pid):
        """Forcefully and synchronously kills the web server.

        This routine should only be called from http_server.py or its
        subclasses."""
        raise NotImplementedError('Port._shut_down_http_server')

    def _webkit_baseline_path(self, platform):
        """Return the  full path to the top of the baseline tree for a
        given platform."""
        return self._filesystem.join(self.layout_tests_dir(), 'platform',
                                     platform)


class DriverInput(object):
    """Holds the input parameters for a driver."""

    def __init__(self, filename, timeout, image_hash):
        """Initializes a DriverInput object.

        Args:
          filename: Full path to the test.
          timeout: Timeout in msecs the driver should use while running the test
          image_hash: A image checksum which is used to avoid doing an image dump if
                     the checksums match.
        """
        self.filename = filename
        self.timeout = timeout
        self.image_hash = image_hash


class DriverOutput(object):
    """Groups information about a output from driver for easy passing of data."""

    def __init__(self, text, image, image_hash, audio,
                 crash=False, test_time=0, timeout=False, error=''):
        """Initializes a TestOutput object.

        Args:
          text: a text output
          image: an image output
          image_hash: a string containing the checksum of the image
          audio: contents of an audio stream, if any (in WAV format)
          crash: a boolean indicating whether the driver crashed on the test
          test_time: the time the test took to execute
          timeout: a boolean indicating whehter the test timed out
          error: any unexpected or additional (or error) text output
        """
        self.text = text
        self.image = image
        self.image_hash = image_hash
        self.audio = audio
        self.crash = crash
        self.test_time = test_time
        self.timeout = timeout
        self.error = error


class Driver:
    """Abstract interface for the DumpRenderTree interface."""

    def __init__(self, port, worker_number):
        """Initialize a Driver to subsequently run tests.

        Typically this routine will spawn DumpRenderTree in a config
        ready for subsequent input.

        port - reference back to the port object.
        worker_number - identifier for a particular worker/driver instance
        """
        raise NotImplementedError('Driver.__init__')

    def run_test(self, driver_input):
        """Run a single test and return the results.

        Note that it is okay if a test times out or crashes and leaves
        the driver in an indeterminate state. The upper layers of the program
        are responsible for cleaning up and ensuring things are okay.

        Args:
          driver_input: a DriverInput object

        Returns a DriverOutput object.
          Note that DriverOutput.image will be '' (empty string) if a test crashes.
        """
        raise NotImplementedError('Driver.run_test')

    # FIXME: This is static so we can test it w/o creating a Base instance.
    @classmethod
    def _command_wrapper(cls, wrapper_option):
        # Hook for injecting valgrind or other runtime instrumentation,
        # used by e.g. tools/valgrind/valgrind_tests.py.
        wrapper = []
        browser_wrapper = os.environ.get("BROWSER_WRAPPER", None)
        if browser_wrapper:
            # FIXME: There seems to be no reason to use BROWSER_WRAPPER over --wrapper.
            # Remove this code any time after the date listed below.
            _log.error("BROWSER_WRAPPER is deprecated, please use --wrapper instead.")
            _log.error("BROWSER_WRAPPER will be removed any time after June 1st 2010 and your scripts will break.")
            wrapper += [browser_wrapper]

        if wrapper_option:
            wrapper += shlex.split(wrapper_option)
        return wrapper

    def poll(self):
        """Returns None if the Driver is still running. Returns the returncode
        if it has exited."""
        raise NotImplementedError('Driver.poll')

    def stop(self):
        raise NotImplementedError('Driver.stop')


class TestConfiguration(object):
    def __init__(self, port=None, os=None, version=None, architecture=None,
                 build_type=None, graphics_type=None):
        self.os = os or port.operating_system()
        self.version = version or port.version()
        self.architecture = architecture or port.architecture()
        self.build_type = build_type or port._options.configuration.lower()
        self.graphics_type = graphics_type or port.graphics_type()

    def items(self):
        return self.__dict__.items()

    def keys(self):
        return self.__dict__.keys()

    def __str__(self):
        return ("<%(os)s, %(version)s, %(architecture)s, %(build_type)s, %(graphics_type)s>" %
                self.__dict__)

    def __repr__(self):
        return "TestConfig(os='%(os)s', version='%(version)s', architecture='%(architecture)s', build_type='%(build_type)s', graphics_type='%(graphics_type)s')" % self.__dict__

    def values(self):
        """Returns the configuration values of this instance as a tuple."""
        return self.__dict__.values()

    def all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for system in self.all_systems():
            for build_type in self.all_build_types():
                for graphics_type in self.all_graphics_types():
                    test_configurations.append(TestConfiguration(
                        os=system[0],
                        version=system[1],
                        architecture=system[2],
                        build_type=build_type,
                        graphics_type=graphics_type))
        return test_configurations

    def all_systems(self):
        return (('mac', 'leopard', 'x86'),
                ('mac', 'snowleopard', 'x86'),
                ('win', 'xp', 'x86'),
                ('win', 'vista', 'x86'),
                ('win', 'win7', 'x86'),
                ('linux', 'hardy', 'x86'),
                ('linux', 'hardy', 'x86_64'))

    def all_build_types(self):
        return ('debug', 'release')

    def all_graphics_types(self):
        return ('cpu', 'gpu')
