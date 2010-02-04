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
#     * Neither the name of Google Inc. nor the names of its
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

"""This package contains utility methods for manipulating paths and
filenames for test results and baselines. It also contains wrappers
of a few routines in port/ so that the port package can
be considered a 'protected' package - i.e., this file should be
the only file that ever includes port. This leads to
us including a few things that don't really have anything to do
 with paths, unfortunately."""

import errno
import os
import stat
import sys

import port
import chromium_win
import chromium_mac
import chromium_linux

# Cache some values so we don't have to recalculate them. _basedir is
# used by PathFromBase() and caches the full (native) path to the top
# of the source tree (/src). _baseline_search_path is used by
# ExpectedBaselines() and caches the list of native paths to search
# for baseline results.
_basedir = None
_baseline_search_path = None


class PathNotFound(Exception):
    pass


def layout_tests_dir():
    """Returns the fully-qualified path to the directory containing the input
    data for the specified layout test."""
    return path_from_base('third_party', 'WebKit', 'LayoutTests')


def chromium_baseline_path(platform=None):
    """Returns the full path to the directory containing expected
    baseline results from chromium ports. If |platform| is None, the
    currently executing platform is used.

    Note: although directly referencing individual port/* files is
    usually discouraged, we allow it here so that the rebaselining tool can
    pull baselines for platforms other than the host platform."""

    # Normalize the platform string.
    platform = platform_name(platform)
    if platform.startswith('chromium-mac'):
        return chromium_mac.baseline_path(platform)
    elif platform.startswith('chromium-win'):
        return chromium_win.baseline_path(platform)
    elif platform.startswith('chromium-linux'):
        return chromium_linux.baseline_path(platform)

    return port.baseline_path()


def webkit_baseline_path(platform):
    """Returns the full path to the directory containing expected
    baseline results from WebKit ports."""
    return path_from_base('third_party', 'WebKit', 'LayoutTests',
                          'platform', platform)


def baseline_search_path(platform=None):
    """Returns the list of directories to search for baselines/results for a
    given platform, in order of preference. Paths are relative to the top of
    the source tree. If parameter platform is None, returns the list for the
    current platform that the script is running on.

    Note: although directly referencing individual port/* files is
    usually discouraged, we allow it here so that the rebaselining tool can
    pull baselines for platforms other than the host platform."""

    # Normalize the platform name.
    platform = platform_name(platform)
    if platform.startswith('chromium-mac'):
        return chromium_mac.baseline_search_path(platform)
    elif platform.startswith('chromium-win'):
        return chromium_win.baseline_search_path(platform)
    elif platform.startswith('chromium-linux'):
        return chromium_linux.baseline_search_path(platform)
    return port.baseline_search_path()


def expected_baselines(filename, suffix, platform=None, all_baselines=False):
    """Given a test name, finds where the baseline results are located.

    Args:
       filename: absolute filename to test file
       suffix: file suffix of the expected results, including dot; e.g. '.txt'
           or '.png'.  This should not be None, but may be an empty string.
       platform: layout test platform: 'win', 'linux' or 'mac'. Defaults to
                 the current platform.
       all_baselines: If True, return an ordered list of all baseline paths
                      for the given platform. If False, return only the first
                      one.
    Returns
       a list of ( platform_dir, results_filename ), where
         platform_dir - abs path to the top of the results tree (or test tree)
         results_filename - relative path from top of tree to the results file
           (os.path.join of the two gives you the full path to the file,
            unless None was returned.)
      Return values will be in the format appropriate for the current platform
      (e.g., "\\" for path separators on Windows). If the results file is not
      found, then None will be returned for the directory, but the expected
      relative pathname will still be returned.
    """
    global _baseline_search_path
    global _search_path_platform
    testname = os.path.splitext(relative_test_filename(filename))[0]

    baseline_filename = testname + '-expected' + suffix

    if (_baseline_search_path is None) or (_search_path_platform != platform):
        _baseline_search_path = baseline_search_path(platform)
        _search_path_platform = platform

    baselines = []
    for platform_dir in _baseline_search_path:
        if os.path.exists(os.path.join(platform_dir, baseline_filename)):
            baselines.append((platform_dir, baseline_filename))

        if not all_baselines and baselines:
            return baselines

    # If it wasn't found in a platform directory, return the expected result
    # in the test directory, even if no such file actually exists.
    platform_dir = layout_tests_dir()
    if os.path.exists(os.path.join(platform_dir, baseline_filename)):
        baselines.append((platform_dir, baseline_filename))

    if baselines:
        return baselines

    return [(None, baseline_filename)]


def expected_filename(filename, suffix):
    """Given a test name, returns an absolute path to its expected results.

    If no expected results are found in any of the searched directories, the
    directory in which the test itself is located will be returned. The return
    value is in the format appropriate for the platform (e.g., "\\" for
    path separators on windows).

    Args:
       filename: absolute filename to test file
       suffix: file suffix of the expected results, including dot; e.g. '.txt'
           or '.png'.  This should not be None, but may be an empty string.
       platform: the most-specific directory name to use to build the
           search list of directories, e.g., 'chromium-win', or
           'chromium-mac-leopard' (we follow the WebKit format)
    """
    platform_dir, baseline_filename = expected_baselines(filename, suffix)[0]
    if platform_dir:
        return os.path.join(platform_dir, baseline_filename)
    return os.path.join(layout_tests_dir(), baseline_filename)


def relative_test_filename(filename):
    """Provide the filename of the test relative to the layout tests
    directory as a unix style path (a/b/c)."""
    return _win_path_to_unix(filename[len(layout_tests_dir()) + 1:])


def _win_path_to_unix(path):
    """Convert a windows path to use unix-style path separators (a/b/c)."""
    return path.replace('\\', '/')

#
# Routines that are arguably platform-specific but have been made
# generic for now
#


def filename_to_uri(full_path):
    """Convert a test file to a URI."""
    LAYOUTTEST_HTTP_DIR = "http/tests/"
    LAYOUTTEST_WEBSOCKET_DIR = "websocket/tests/"

    relative_path = _win_path_to_unix(relative_test_filename(full_path))
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
    # TODO(jianli): Consider extending this to "media/".
    if port and not relative_path.startswith("local/"):
        if relative_path.startswith("ssl/"):
            port += 443
            protocol = "https"
        else:
            protocol = "http"
        return "%s://127.0.0.1:%u/%s" % (protocol, port, relative_path)

    if sys.platform in ('cygwin', 'win32'):
        return "file:///" + get_absolute_path(full_path)
    return "file://" + get_absolute_path(full_path)


def get_absolute_path(path):
    """Returns an absolute UNIX path."""
    return _win_path_to_unix(os.path.abspath(path))


def maybe_make_directory(*path):
    """Creates the specified directory if it doesn't already exist."""
    try:
        os.makedirs(os.path.join(*path))
    except OSError, e:
        if e.errno != errno.EEXIST:
            raise


def path_from_base(*comps):
    """Returns an absolute filename from a set of components specified
    relative to the top of the source tree. If the path does not exist,
    the exception PathNotFound is raised."""
    global _basedir
    if _basedir == None:
        # We compute the top of the source tree by finding the absolute
        # path of this source file, and then climbing up three directories
        # as given in subpath. If we move this file, subpath needs to be
        # updated.
        path = os.path.abspath(__file__)
        subpath = os.path.join('third_party', 'WebKit')
        _basedir = path[:path.index(subpath)]
    path = os.path.join(_basedir, *comps)
    if not os.path.exists(path):
        raise PathNotFound('could not find %s' % (path))
    return path


def remove_directory(*path):
    """Recursively removes a directory, even if it's marked read-only.

    Remove the directory located at *path, if it exists.

    shutil.rmtree() doesn't work on Windows if any of the files or directories
    are read-only, which svn repositories and some .svn files are.  We need to
    be able to force the files to be writable (i.e., deletable) as we traverse
    the tree.

    Even with all this, Windows still sometimes fails to delete a file, citing
    a permission error (maybe something to do with antivirus scans or disk
    indexing).  The best suggestion any of the user forums had was to wait a
    bit and try again, so we do that too.  It's hand-waving, but sometimes it
    works. :/
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
        # For POSIX:  making the directory writable guarantees removability.
        # Windows will ignore the non-read-only bits in the chmod value.
        os.chmod(root, 0770)
        for name in files:
            remove_with_retry(os.remove, os.path.join(root, name))
        for name in dirs:
            remove_with_retry(os.rmdir, os.path.join(root, name))

    remove_with_retry(os.rmdir, file_path)

#
# Wrappers around port/
#


def platform_name(platform=None):
    """Returns the appropriate chromium platform name for |platform|. If
       |platform| is None, returns the name of the chromium platform on the
       currently running system. If |platform| is of the form 'chromium-*',
       it is returned unchanged, otherwise 'chromium-' is prepended."""
    if platform == None:
        return port.platform_name()
    if not platform.startswith('chromium-'):
        platform = "chromium-" + platform
    return platform


def platform_version():
    return port.platform_version()


def lighttpd_executable_path():
    return port.lighttpd_executable_path()


def lighttpd_module_path():
    return port.lighttpd_module_path()


def lighttpd_php_path():
    return port.lighttpd_php_path()


def wdiff_path():
    return port.wdiff_path()


def test_shell_path(target):
    return port.test_shell_path(target)


def image_diff_path(target):
    return port.image_diff_path(target)


def layout_test_helper_path(target):
    return port.layout_test_helper_path(target)


def fuzzy_match_path():
    return port.fuzzy_match_path()


def shut_down_http_server(server_pid):
    return port.shut_down_http_server(server_pid)


def kill_all_test_shells():
    port.kill_all_test_shells()
