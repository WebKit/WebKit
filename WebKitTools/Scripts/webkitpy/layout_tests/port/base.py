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
import codecs
import difflib
import errno
import os
import shlex
import sys
import time

import apache_http_server
import http_server
import websocket_server

from webkitpy.common.system import logutils
from webkitpy.common.system.executive import Executive, ScriptError


_log = logutils.get_logger(__file__)


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
_wdiff_available = True
_pretty_patch_available = True

# FIXME: This class should merge with webkitpy.webkit_port at some point.
class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package.
    """

    @staticmethod
    def flag_from_configuration(configuration):
        flags_by_configuration = {
            "Debug": "--debug",
            "Release": "--release",
        }
        return flags_by_configuration[configuration]

    def __init__(self, port_name=None, options=None, executive=Executive()):
        self._name = port_name
        self._options = options
        self._helper = None
        self._http_server = None
        self._webkit_base_dir = None
        self._websocket_server = None
        self._executive = executive

    def default_child_processes(self):
        """Return the number of DumpRenderTree instances to use for this
        port."""
        return self._executive.cpu_count()

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

    def compare_text(self, expected_text, actual_text):
        """Return whether or not the two strings are *not* equal. This
        routine is used to diff text output.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        return expected_text != actual_text

    def diff_image(self, expected_filename, actual_filename,
                   diff_filename=None, tolerance=0):
        """Compare two image files and produce a delta image file.

        Return True if the two files are different, False if they are the same.
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
            (os.path.join of the two gives you the full path to the file,
                unless None was returned.)
        Return values will be in the format appropriate for the current
        platform (e.g., "\\" for path separators on Windows). If the results
        file is not found, then None will be returned for the directory,
        but the expected relative pathname will still be returned.

        This routine is generic but lives here since it is used in
        conjunction with the other baseline and filename routines that are
        platform specific.
        """
        testname = os.path.splitext(self.relative_test_filename(filename))[0]

        baseline_filename = testname + '-expected' + suffix

        baseline_search_path = self.baseline_search_path()

        baselines = []
        for platform_dir in baseline_search_path:
            if os.path.exists(os.path.join(platform_dir, baseline_filename)):
                baselines.append((platform_dir, baseline_filename))

            if not all_baselines and baselines:
                return baselines

        # If it wasn't found in a platform directory, return the expected
        # result in the test directory, even if no such file actually exists.
        platform_dir = self.layout_tests_dir()
        if os.path.exists(os.path.join(platform_dir, baseline_filename)):
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
            return os.path.join(platform_dir, baseline_filename)
        return os.path.join(self.layout_tests_dir(), baseline_filename)

    def filename_to_uri(self, filename):
        """Convert a test file to a URI."""
        LAYOUTTEST_HTTP_DIR = "http/tests/"
        LAYOUTTEST_WEBSOCKET_DIR = "websocket/tests/"

        relative_path = self.relative_test_filename(filename)
        port = None
        use_ssl = False

        if relative_path.startswith(LAYOUTTEST_HTTP_DIR):
            # http/tests/ run off port 8000 and ssl/ off 8443
            relative_path = relative_path[len(LAYOUTTEST_HTTP_DIR):]
            port = 8000
        elif relative_path.startswith(LAYOUTTEST_WEBSOCKET_DIR):
            # websocket/tests/ run off port 8880 and 9323
            # Note: the root is /, not websocket/tests/
            port = 8880

        # Make http/tests/local run as local files. This is to mimic the
        # logic in run-webkit-tests.
        #
        # TODO(dpranke): remove the media reference and the SSL reference?
        if (port and not relative_path.startswith("local/") and
            not relative_path.startswith("media/")):
            if relative_path.startswith("ssl/"):
                port += 443
                protocol = "https"
            else:
                protocol = "http"
            return "%s://127.0.0.1:%u/%s" % (protocol, port, relative_path)

        if sys.platform in ('cygwin', 'win32'):
            return "file:///" + self.get_absolute_path(filename)
        return "file://" + self.get_absolute_path(filename)

    def get_absolute_path(self, filename):
        """Return the absolute path in unix format for the given filename.

        This routine exists so that platforms that don't use unix filenames
        can convert accordingly."""
        return os.path.abspath(filename)

    def layout_tests_dir(self):
        """Return the absolute path to the top of the LayoutTests directory."""
        return self.path_from_webkit_base('LayoutTests')

    def skips_layout_test(self, test_name):
        """Figures out if the givent test is being skipped or not.

        Test categories are handled as well."""
        for test_or_category in self.skipped_layout_tests():
            if test_or_category == test_name:
                return True
            category = os.path.join(self.layout_tests_dir(), test_or_category)
            if os.path.isdir(category) and test_name.startswith(test_or_category):
                return True
        return False

    def maybe_make_directory(self, *path):
        """Creates the specified directory if it doesn't already exist."""
        try:
            os.makedirs(os.path.join(*path))
        except OSError, e:
            if e.errno != errno.EEXIST:
                raise

    def name(self):
        """Return the name of the port (e.g., 'mac', 'chromium-win-xp').

        Note that this is different from the test_platform_name(), which
        may be different (e.g., 'win-xp' instead of 'chromium-win-xp'."""
        return self._name

    # FIXME: This could be replaced by functions in webkitpy.common.checkout.scm.
    def path_from_webkit_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        WebKit source tree and the list of path components in |*comps|."""
        if not self._webkit_base_dir:
            abspath = os.path.abspath(__file__)
            self._webkit_base_dir = abspath[0:abspath.find('WebKitTools')]

        return os.path.join(self._webkit_base_dir, *comps)

    # FIXME: Callers should eventually move to scm.script_path.
    def script_path(self, script_name):
        return self.path_from_webkit_base("WebKitTools", "Scripts", script_name)

    def path_to_test_expectations_file(self):
        """Update the test expectations to the passed-in string.

        This is used by the rebaselining tool. Raises NotImplementedError
        if the port does not use expectations files."""
        raise NotImplementedError('Port.path_to_test_expectations_file')

    def relative_test_filename(self, filename):
        """Relative unix-style path for a filename under the LayoutTests
        directory. Filenames outside the LayoutTests directory should raise
        an error."""
        assert(filename.startswith(self.layout_tests_dir()))
        return filename[len(self.layout_tests_dir()) + 1:]

    def results_directory(self):
        """Absolute path to the place to store the test results."""
        raise NotImplementedError('Port.results_directory')

    def setup_test_run(self):
        """Perform port-specific work at the beginning of a test run."""
        pass

    def setup_environ_for_server(self):
        """Perform port-specific work at the beginning of a server launch.

        Returns:
           Operating-system's environment.
        """
        return os.environ.copy()

    def show_html_results_file(self, results_filename):
        """This routine should display the HTML file pointed at by
        results_filename in a users' browser."""
        raise NotImplementedError('Port.show_html_results_file')

    def create_driver(self, png_path, options):
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
        if self._options.use_apache:
            self._http_server = apache_http_server.LayoutTestApacheHttpd(self,
                self._options.results_directory)
        else:
            self._http_server = http_server.Lighttpd(self,
                self._options.results_directory)
        self._http_server.start()

    def start_websocket_server(self):
        """Start a websocket server if it is available. Do nothing if
        it isn't. This routine is allowed to (and may) fail if a server
        is already running."""
        self._websocket_server = websocket_server.PyWebSocket(self,
            self._options.results_directory)
        self._websocket_server.start()

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

    def test_expectations(self):
        """Returns the test expectations for this port.

        Basically this string should contain the equivalent of a
        test_expectations file. See test_expectations.py for more details."""
        raise NotImplementedError('Port.test_expectations')

    def test_expectations_overrides(self):
        """Returns an optional set of overrides for the test_expectations.

        This is used by ports that have code in two repositories, and where
        it is possible that you might need "downstream" expectations that
        temporarily override the "upstream" expectations until the port can
        sync up the two repos."""
        return None

    def test_base_platform_names(self):
        """Return a list of the 'base' platforms on your port. The base
        platforms represent different architectures, operating systems,
        or implementations (as opposed to different versions of a single
        platform). For example, 'mac' and 'win' might be different base
        platforms, wherease 'mac-tiger' and 'mac-leopard' might be
        different platforms. This routine is used by the rebaselining tool
        and the dashboards, and the strings correspond to the identifiers
        in your test expectations (*not* necessarily the platform names
        themselves)."""
        raise NotImplementedError('Port.base_test_platforms')

    def test_platform_name(self):
        """Returns the string that corresponds to the given platform name
        in the test expectations. This may be the same as name(), or it
        may be different. For example, chromium returns 'mac' for
        'chromium-mac'."""
        raise NotImplementedError('Port.test_platform_name')

    def test_platforms(self):
        """Returns the list of test platform identifiers as used in the
        test_expectations and on dashboards, the rebaselining tool, etc.

        Note that this is not necessarily the same as the list of ports,
        which must be globally unique (e.g., both 'chromium-mac' and 'mac'
        might return 'mac' as a test_platform name'."""
        raise NotImplementedError('Port.platforms')

    def test_platform_name_to_name(self, test_platform_name):
        """Returns the Port platform name that corresponds to the name as
        referenced in the expectations file. E.g., "mac" returns
        "chromium-mac" on the Chromium ports."""
        raise NotImplementedError('Port.test_platform_name_to_name')

    def version(self):
        """Returns a string indicating the version of a given platform, e.g.
        '-leopard' or '-xp'.

        This is used to help identify the exact port when parsing test
        expectations, determining search paths, and logging information."""
        raise NotImplementedError('Port.version')

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
        global _wdiff_available  # See explaination at top of file.
        if not _wdiff_available:
            return ""
        try:
            # It's possible to raise a ScriptError we pass wdiff invalid paths.
            return self._run_wdiff(actual_filename, expected_filename)
        except OSError, e:
            if e.errno in [errno.ENOENT, errno.EACCES, errno.ECHILD]:
                # Silently ignore cases where wdiff is missing.
                _wdiff_available = False
                return ""
            raise

    _pretty_patch_error_html = "Failed to run PrettyPatch, see error console."

    def pretty_patch_text(self, diff_path):
        # FIXME: Much of this function could move to prettypatch.rb
        global _pretty_patch_available
        if not _pretty_patch_available:
            return self._pretty_patch_error_html
        pretty_patch_path = self.path_from_webkit_base("BugsSite", "PrettyPatch")
        prettify_path = os.path.join(pretty_patch_path, "prettify.rb")
        command = ["ruby", "-I", pretty_patch_path, prettify_path, diff_path]
        try:
            # Diffs are treated as binary (we pass decode_output=False) as they
            # may contain multiple files of conflicting encodings.
            return self._executive.run_command(command, decode_output=False)
        except OSError, e:
            # If the system is missing ruby log the error and stop trying.
            _pretty_patch_available = False
            _log.error("Failed to run PrettyPatch (%s): %s" % (command, e))
            return self._pretty_patch_error_html
        except ScriptError, e:
            # If ruby failed to run for some reason, log the command output and stop trying.
            _pretty_patch_available = False
            _log.error("Failed to run PrettyPatch (%s):\n%s" % (command, e.message_with_output()))
            return self._pretty_patch_error_html

    def _webkit_build_directory(self, args):
        args = ["perl", self.script_path("webkit-build-directory")] + args
        return self._executive.run_command(args).rstrip()

    def _configuration_file_path(self):
        build_root = self._webkit_build_directory(["--top-level"])
        return os.path.join(build_root, "Configuration")

    # Easy override for unit tests
    def _open_configuration_file(self):
        configuration_path = self._configuration_file_path()
        return codecs.open(configuration_path, "r", "utf-8")

    def _read_configuration(self):
        try:
            with self._open_configuration_file() as file:
                return file.readline().rstrip()
        except IOError, e:
            return None

    # FIXME: This list may be incomplete as Apple has some sekret configs.
    _RECOGNIZED_CONFIGURATIONS = ("Debug", "Release")

    def default_configuration(self):
        # FIXME: Unify this with webkitdir.pm configuration reading code.
        configuration = self._read_configuration()
        if not configuration:
            configuration = "Release"
        if configuration not in self._RECOGNIZED_CONFIGURATIONS:
            _log.warn("Configuration \"%s\" found in %s is not a recognized value.\n" % (configuration, self._configuration_file_path()))
            _log.warn("Scripts may fail.  See 'set-webkit-configuration --help'.")
        return configuration

    #
    # PROTECTED ROUTINES
    #
    # The routines below should only be called by routines in this class
    # or any of its subclasses.
    #

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
        raise NotImplementedError('Port.path_to_driver')

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
        return os.path.join(self.layout_tests_dir(), 'platform',
                            platform)


class Driver:
    """Abstract interface for the DumpRenderTree interface."""

    def __init__(self, port, png_path, options):
        """Initialize a Driver to subsequently run tests.

        Typically this routine will spawn DumpRenderTree in a config
        ready for subsequent input.

        port - reference back to the port object.
        png_path - an absolute path for the driver to write any image
            data for a test (as a PNG). If no path is provided, that
            indicates that pixel test results will not be checked.
        options - any port-specific driver options."""
        raise NotImplementedError('Driver.__init__')

    def run_test(self, uri, timeout, checksum):
        """Run a single test and return the results.

        Note that it is okay if a test times out or crashes and leaves
        the driver in an indeterminate state. The upper layers of the program
        are responsible for cleaning up and ensuring things are okay.

        uri - a full URI for the given test
        timeout - number of milliseconds to wait before aborting this test.
        checksum - if present, the expected checksum for the image for this
            test

        Returns a tuple of the following:
            crash - a boolean indicating whether the driver crashed on the test
            timeout - a boolean indicating whehter the test timed out
            checksum - a string containing the checksum of the image, if
                present
            output - any text output
            error - any unexpected or additional (or error) text output

        Note that the image itself should be written to the path that was
        specified in the __init__() call."""
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
