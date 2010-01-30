#!/usr/bin/env python
# Copyright (C) 2010 The Chromium Authors. All rights reserved.
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
#     * Neither the Chromium name nor the names of its
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

"""This is the Linux implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import os
import path_utils
import subprocess
import sys


def platform_name():
    """Returns the name of the platform we're currently running on."""
    # We're not ready for version-specific results yet. When we uncomment
    # this, we also need to add it to the BaselineSearchPath()
    return 'chromium-win' + platform_version()


def platform_version():
    """Returns the version string for the platform, e.g. '-vista' or
    '-snowleopard'. If the platform does not distinguish between
    minor versions, it returns ''."""
    winver = sys.getwindowsversion()
    if winver[0] == 6 and (winver[1] == 1):
        return '-7'
    if winver[0] == 6 and (winver[1] == 0):
        return '-vista'
    if winver[0] == 5 and (winver[1] == 1 or winver[1] == 2):
        return '-xp'
    return ''


def get_num_cores():
    """Returns the number of cores on the machine. For hyperthreaded machines,
    this will be double the number of actual processors."""
    return int(os.environ.get('NUMBER_OF_PROCESSORS', 1))


def baseline_path(platform=None):
    """Returns the path relative to the top of the source tree for the
    baselines for the specified platform version. If |platform| is None,
    then the version currently in use is used."""
    if platform is None:
        platform = platform_name()
    return path_utils.path_from_base('webkit', 'data', 'layout_tests',
                                     'platform', platform, 'LayoutTests')


def baseline_search_path(platform=None):
    """Returns the list of directories to search for baselines/results, in
    order of preference. Paths are relative to the top of the source tree."""
    dirs = []
    if platform is None:
        platform = platform_name()

    if platform == 'chromium-win-xp':
        dirs.append(baseline_path(platform))
    if platform in ('chromium-win-xp', 'chromium-win-vista'):
        dirs.append(baseline_path('chromium-win-vista'))
    dirs.append(baseline_path('chromium-win'))
    dirs.append(path_utils.webkit_baseline_path('win'))
    dirs.append(path_utils.webkit_baseline_path('mac'))
    return dirs


def wdiff_path():
    """Path to the WDiff executable, whose binary is checked in on Win"""
    return path_utils.path_from_base('third_party', 'cygwin', 'bin',
                                     'wdiff.exe')


def image_diff_path(target):
    """Return the platform-specific binary path for the image compare util.
         We use this if we can't find the binary in the default location
         in path_utils.

    Args:
      target: Build target mode (debug or release)
    """
    return _find_binary(target, 'image_diff.exe')


def layout_test_helper_path(target):
    """Return the platform-specific binary path for the layout test helper.
    We use this if we can't find the binary in the default location
    in path_utils.

    Args:
      target: Build target mode (debug or release)
    """
    return _find_binary(target, 'layout_test_helper.exe')


def test_shell_path(target):
    """Return the platform-specific binary path for our TestShell.
       We use this if we can't find the binary in the default location
       in path_utils.

    Args:
      target: Build target mode (debug or release)
    """
    return _find_binary(target, 'test_shell.exe')


def apache_executable_path():
    """Returns the executable path to start Apache"""
    path = path_utils.path_from_base('third_party', 'cygwin', "usr", "sbin")
    # Don't return httpd.exe since we want to use this from cygwin.
    return os.path.join(path, "httpd")


def apache_config_file_path():
    """Returns the path to Apache config file"""
    return path_utils.path_from_base("third_party", "WebKit", "LayoutTests",
        "http", "conf", "cygwin-httpd.conf")


def lighttpd_executable_path():
    """Returns the executable path to start LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'win',
                                     'LightTPD.exe')


def lighttpd_module_path():
    """Returns the library module path for LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'win', 'lib')


def lighttpd_php_path():
    """Returns the PHP executable path for LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'win', 'php5',
                                     'php-cgi.exe')


def shut_down_http_server(server_pid):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

    Args:
      server_pid: The process ID of the running server.
          Unused in this implementation of the method.
    """
    subprocess.Popen(('taskkill.exe', '/f', '/im', 'LightTPD.exe'),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()
    subprocess.Popen(('taskkill.exe', '/f', '/im', 'httpd.exe'),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()


def kill_process(pid):
    """Forcefully kill the process.

    Args:
      pid: The id of the process to be killed.
    """
    subprocess.call(('taskkill.exe', '/f', '/pid', str(pid)),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE)


def kill_all_test_shells(self):
    """Kills all instances of the test_shell binary currently running."""
    subprocess.Popen(('taskkill.exe', '/f', '/im', 'test_shell.exe'),
                     stdout=subprocess.PIPE,
                     stderr=subprocess.PIPE).wait()

#
# Private helper functions.
#


def _find_binary(target, binary):
    """On Windows, we look for binaries that we compile in potentially
    two places: src/webkit/$target (preferably, which we get if we
    built using webkit_glue.gyp), or src/chrome/$target (if compiled some
    other way)."""
    try:
        return path_utils.path_from_base('webkit', target, binary)
    except path_utils.PathNotFound:
        try:
            return path_utils.path_from_base('chrome', target, binary)
        except path_utils.PathNotFound:
            return path_utils.path_from_base('build', target, binary)
