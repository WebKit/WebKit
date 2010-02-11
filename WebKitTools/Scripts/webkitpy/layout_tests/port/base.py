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

import cgi
import difflib
import errno
import os
import subprocess
import sys

import apache_http_server
import http_server
import websocket_server

# Python bug workaround.  See Port.wdiff_text() for an explanation.
_wdiff_available = True


# FIXME: This class should merge with webkitpy.webkit_port at some point.
class Port(object):
    """Abstract class for Port-specific hooks for the layout_test package.
    """

    def __init__(self, port_name=None, options=None):
        self._name = port_name
        self._options = options
        self._helper = None
        self._http_server = None
        self._webkit_base_dir = None
        self._websocket_server = None

    def baseline_path(self):
        """Return the absolute path to the directory to store new baselines
        in for this port."""
        raise NotImplementedError('Port.baseline_path')

    def baseline_search_path(self):
        """Return a list of absolute paths to directories to search under for
        baselines. The directories are searched in order."""
        raise NotImplementedError('Port.baseline_search_path')

    def check_sys_deps(self):
        """If the port needs to do some runtime checks to ensure that the
        tests can be run successfully, they should be done here.

        Returns whether the system is properly configured."""
        raise NotImplementedError('Port.check_sys_deps')

    def compare_text(self, actual_text, expected_text):
        """Return whether or not the two strings are *not* equal. This
        routine is used to diff text output.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        return actual_text != expected_text

    def diff_image(self, actual_filename, expected_filename, diff_filename):
        """Compare two image files and produce a delta image file.

        Return 1 if the two files are different, 0 if they are the same.
        Also produce a delta image of the two images and write that into
        |diff_filename|.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        executable = self._path_to_image_diff()
        cmd = [executable, '--diff', actual_filename, expected_filename,
               diff_filename]
        result = 1
        try:
            result = subprocess.call(cmd)
        except OSError, e:
            if e.errno == errno.ENOENT or e.errno == errno.EACCES:
                _compare_available = False
            else:
                raise e
        except ValueError:
            # work around a race condition in Python 2.4's implementation
            # of subprocess.Popen. See http://bugs.python.org/issue1199282 .
            pass
        return result

    def diff_text(self, actual_text, expected_text,
                  actual_filename, expected_filename):
        """Returns a string containing the diff of the two text strings
        in 'unified diff' format.

        While this is a generic routine, we include it in the Port
        interface so that it can be overriden for testing purposes."""
        diff = difflib.unified_diff(expected_text.splitlines(True),
                                    actual_text.splitlines(True),
                                    expected_filename,
                                    actual_filename)
        return ''.join(diff)

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

    def num_cores(self):
        """Return the number of cores/cpus available on this machine.

        This routine is used to determine the default amount of parallelism
        used by run-chromium-webkit-tests."""
        raise NotImplementedError('Port.num_cores')

    def path_from_webkit_base(self, *comps):
        """Returns the full path to path made by joining the top of the
        WebKit source tree and the list of path components in |*comps|."""
        if not self._webkit_base_dir:
            abspath = os.path.abspath(__file__)
            self._webkit_base_dir = abspath[0:abspath.find('WebKitTools')]
        return os.path.join(self._webkit_base_dir, *comps)

    def remove_directory(self, *path):
        """Recursively removes a directory, even if it's marked read-only.

        Remove the directory located at *path, if it exists.

        shutil.rmtree() doesn't work on Windows if any of the files
        or directories are read-only, which svn repositories and
        some .svn files are.  We need to be able to force the files
        to be writable (i.e., deletable) as we traverse the tree.

        Even with all this, Windows still sometimes fails to delete a file,
        citing a permission error (maybe something to do with antivirus
        scans or disk indexing).  The best suggestion any of the user
        forums had was to wait a bit and try again, so we do that too.
        It's hand-waving, but sometimes it works. :/
        """
        file_path = os.path.join(*path)
        if not os.path.exists(file_path):
            return

        win32 = False
        if sys.platform == 'win32':
            win32 = True
            # Some people don't have the APIs installed. In that case we'll do
            # without.
            try:
                win32api = __import__('win32api')
                win32con = __import__('win32con')
            except ImportError:
                win32 = False

            def remove_with_retry(rmfunc, path):
                os.chmod(path, stat.S_IWRITE)
                if win32:
                    win32api.SetFileAttributes(path,
                                              win32con.FILE_ATTRIBUTE_NORMAL)
                try:
                    return rmfunc(path)
                except EnvironmentError, e:
                    if e.errno != errno.EACCES:
                        raise
                    print 'Failed to delete %s: trying again' % repr(path)
                    time.sleep(0.1)
                    return rmfunc(path)
        else:

            def remove_with_retry(rmfunc, path):
                if os.path.islink(path):
                    return os.remove(path)
                else:
                    return rmfunc(path)

        for root, dirs, files in os.walk(file_path, topdown=False):
            # For POSIX:  making the directory writable guarantees
            # removability. Windows will ignore the non-read-only
            # bits in the chmod value.
            os.chmod(root, 0770)
            for name in files:
                remove_with_retry(os.remove, os.path.join(root, name))
            for name in dirs:
                remove_with_retry(os.rmdir, os.path.join(root, name))

        remove_with_retry(os.rmdir, file_path)

    def test_platform_name(self):
        return self._name

    def relative_test_filename(self, filename):
        """Relative unix-style path for a filename under the LayoutTests
        directory. Filenames outside the LayoutTests directory should raise
        an error."""
        return filename[len(self.layout_tests_dir()) + 1:]

    def results_directory(self):
        """Absolute path to the place to store the test results."""
        raise NotImplemented('Port.results_directory')

    def setup_test_run(self):
        """This routine can be overridden to perform any port-specific
        work that shouuld be done at the beginning of a test run."""
        pass

    def show_html_results_file(self, results_filename):
        """This routine should display the HTML file pointed at by
        results_filename in a users' browser."""
        raise NotImplementedError('Port.show_html_results_file')

    def start_driver(self, png_path, options):
        """Starts a new test Driver and returns a handle to the object."""
        raise NotImplementedError('Port.start_driver')

    def start_helper(self):
        """Start a layout test helper if needed on this port. The test helper
        is used to reconfigure graphics settings and do other things that
        may be necessary to ensure a known test configuration."""
        raise NotImplementedError('Port.start_helper')

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
        it isn't, or it isn't available."""
        raise NotImplementedError('Port.stop_helper')

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

    def version(self):
        """Returns a string indicating the version of a given platform, e.g.
        '-leopard' or '-xp'.

        This is used to help identify the exact port when parsing test
        expectations, determining search paths, and logging information."""
        raise NotImplementedError('Port.version')

    def wdiff_text(self, actual_filename, expected_filename):
        """Returns a string of HTML indicating the word-level diff of the
        contents of the two filenames. Returns an empty string if word-level
        diffing isn't available."""
        executable = self._path_to_wdiff()
        cmd = [executable,
               '--start-delete=##WDIFF_DEL##',
               '--end-delete=##WDIFF_END##',
               '--start-insert=##WDIFF_ADD##',
               '--end-insert=##WDIFF_END##',
               expected_filename,
               actual_filename]
        global _wdiff_available
        result = ''
        try:
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
            #
            # It also has a threading bug, so we don't output wdiff if
            # the Popen raises a ValueError.
            # http://bugs.python.org/issue1236
            if _wdiff_available:
                try:
                    wdiff = subprocess.Popen(cmd,
                        stdout=subprocess.PIPE).communicate()[0]
                except ValueError, e:
                    # Working around a race in Python 2.4's implementation
                    # of Popen().
                    wdiff = ''
                wdiff = cgi.escape(wdiff)
                wdiff = wdiff.replace('##WDIFF_DEL##', '<span class=del>')
                wdiff = wdiff.replace('##WDIFF_ADD##', '<span class=add>')
                wdiff = wdiff.replace('##WDIFF_END##', '</span>')
                result = '<head><style>.del { background: #faa; } '
                result += '.add { background: #afa; }</style></head>'
                result += '<pre>' + wdiff + '</pre>'
        except OSError, e:
            if (e.errno == errno.ENOENT or e.errno == errno.EACCES or
                e.errno == errno.ECHILD):
                _wdiff_available = False
            else:
                raise e
        return result

    #
    # PROTECTED ROUTINES
    #
    # The routines below should only be called by routines in this class
    # or any of its subclasses.
    #

    def _kill_process(self, pid):
        """Forcefully kill a process.

        This routine should not be used or needed generically, but can be
        used in helper files like http_server.py."""
        raise NotImplementedError('Port.kill_process')

    def _path_to_apache(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module."""
        raise NotImplementedError('Port.path_to_apache')

    def _path_to_apache_config_file(self):
        """Returns the full path to the apache binary.

        This is needed only by ports that use the apache_http_server module."""
        raise NotImplementedError('Port.path_to_apache_config_file')

    def _path_to_driver(self):
        """Returns the full path to the test driver (DumpRenderTree)."""
        raise NotImplementedError('Port.path_to_driver')

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

    def poll(self):
        """Returns None if the Driver is still running. Returns the returncode
        if it has exited."""
        raise NotImplementedError('Driver.poll')

    def returncode(self):
        """Returns the system-specific returncode if the Driver has stopped or
        exited."""
        raise NotImplementedError('Driver.returncode')

    def stop(self):
        raise NotImplementedError('Driver.stop')
