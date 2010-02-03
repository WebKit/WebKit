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

"""This is the Mac implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import os
import platform
import signal
import subprocess

import path_utils


def platform_name():
    """Returns the name of the platform we're currently running on."""
    # At the moment all chromium mac results are version-independent. At some
    # point we may need to return 'chromium-mac' + PlatformVersion()
    return 'chromium-mac'


def platform_version():
    """Returns the version string for the platform, e.g. '-vista' or
    '-snowleopard'. If the platform does not distinguish between
    minor versions, it returns ''."""
    os_version_string = platform.mac_ver()[0]  # e.g. "10.5.6"
    if not os_version_string:
        return '-leopard'

    release_version = int(os_version_string.split('.')[1])

    # we don't support 'tiger' or earlier releases
    if release_version == 5:
        return '-leopard'
    elif release_version == 6:
        return '-snowleopard'

    return ''


def get_num_cores():
    """Returns the number of cores on the machine. For hyperthreaded machines,
    this will be double the number of actual processors."""
    return int(os.popen2("sysctl -n hw.ncpu")[1].read())


def baseline_path(platform=None):
    """Returns the path relative to the top of the source tree for the
    baselines for the specified platform version. If |platform| is None,
    then the version currently in use is used."""
    if platform is None:
        platform = platform_name()
    return path_utils.path_from_base('webkit', 'data', 'layout_tests',
                                     'platform', platform, 'LayoutTests')

# TODO: We should add leopard and snowleopard to the list of paths to check
# once we start running the tests from snowleopard.


def baseline_search_path(platform=None):
    """Returns the list of directories to search for baselines/results, in
    order of preference. Paths are relative to the top of the source tree."""
    return [baseline_path(platform),
            path_utils.webkit_baseline_path('mac' + platform_version()),
            path_utils.webkit_baseline_path('mac')]


def wdiff_path():
    """Path to the WDiff executable, which we assume is already installed and
    in the user's $PATH."""
    return 'wdiff'


def image_diff_path(target):
    """Path to the image_diff executable

    Args:
      target: build type - 'Debug','Release',etc."""
    return path_utils.path_from_base('xcodebuild', target, 'image_diff')


def layout_test_helper_path(target):
    """Path to the layout_test_helper executable, if needed, empty otherwise

    Args:
      target: build type - 'Debug','Release',etc."""
    return path_utils.path_from_base('xcodebuild', target,
                                     'layout_test_helper')


def test_shell_path(target):
    """Path to the test_shell executable.

    Args:
      target: build type - 'Debug','Release',etc."""
    # TODO(pinkerton): make |target| happy with case-sensitive file systems.
    return path_utils.path_from_base('xcodebuild', target, 'TestShell.app',
                                     'Contents', 'MacOS', 'TestShell')


def apache_executable_path():
    """Returns the executable path to start Apache"""
    return os.path.join("/usr", "sbin", "httpd")


def apache_config_file_path():
    """Returns the path to Apache config file"""
    return path_utils.path_from_base("third_party", "WebKit", "LayoutTests",
        "http", "conf", "apache2-httpd.conf")


def lighttpd_executable_path():
    """Returns the executable path to start LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'mac',
                                     'bin', 'lighttpd')


def lighttpd_module_path():
    """Returns the library module path for LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'mac', 'lib')


def lighttpd_php_path():
    """Returns the PHP executable path for LigHTTPd"""
    return path_utils.path_from_base('third_party', 'lighttpd', 'mac', 'bin',
                                     'php-cgi')


def shut_down_http_server(server_pid):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

      Args:
        server_pid: The process ID of the running server.
    """
    # server_pid is not set when "http_server.py stop" is run manually.
    if server_pid is None:
        # TODO(mmoss) This isn't ideal, since it could conflict with lighttpd
        # processes not started by http_server.py, but good enough for now.
        kill_all_process('lighttpd')
        kill_all_process('httpd')
    else:
        try:
            os.kill(server_pid, signal.SIGTERM)
            # TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
        except OSError:
            # Sometimes we get a bad PID (e.g. from a stale httpd.pid file),
            # so if kill fails on the given PID, just try to 'killall' web
            # servers.
            shut_down_http_server(None)


def kill_process(pid):
    """Forcefully kill the process.

    Args:
      pid: The id of the process to be killed.
    """
    os.kill(pid, signal.SIGKILL)


def kill_all_process(process_name):
    # On Mac OS X 10.6, killall has a new constraint: -SIGNALNAME or
    # -SIGNALNUMBER must come first.  Example problem:
    #   $ killall -u $USER -TERM lighttpd
    #   killall: illegal option -- T
    # Use of the earlier -TERM placement is just fine on 10.5.
    null = open(os.devnull)
    subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'),
                     process_name], stderr=null)
    null.close()


def kill_all_test_shells():
    """Kills all instances of the test_shell binary currently running."""
    kill_all_process('TestShell')
