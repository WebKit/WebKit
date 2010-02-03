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

"""This is the Linux implementation of the layout_package.platform_utils
   package. This file should only be imported by that package."""

import os
import signal
import subprocess
import sys
import logging

import path_utils
import platform_utils_win


def platform_name():
    """Returns the name of the platform we're currently running on."""
    return 'chromium-linux' + platform_version()


def platform_version():
    """Returns the version string for the platform, e.g. '-vista' or
    '-snowleopard'. If the platform does not distinguish between
    minor versions, it returns ''."""
    return ''


def get_num_cores():
    """Returns the number of cores on the machine. For hyperthreaded machines,
    this will be double the number of actual processors."""
    num_cores = os.sysconf("SC_NPROCESSORS_ONLN")
    if isinstance(num_cores, int) and num_cores > 0:
        return num_cores
    return 1


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
    return [baseline_path(platform),
            platform_utils_win.baseline_path('chromium-win'),
            path_utils.webkit_baseline_path('win'),
            path_utils.webkit_baseline_path('mac')]


def apache_executable_path():
    """Returns the executable path to start Apache"""
    path = os.path.join("/usr", "sbin", "apache2")
    if os.path.exists(path):
        return path
    print "Unable to fine Apache executable %s" % path
    _missing_apache()


def apache_config_file_path():
    """Returns the path to Apache config file"""
    return path_utils.path_from_base("third_party", "WebKit", "LayoutTests",
        "http", "conf", "apache2-debian-httpd.conf")


def lighttpd_executable_path():
    """Returns the executable path to start LigHTTPd"""
    binpath = "/usr/sbin/lighttpd"
    if os.path.exists(binpath):
        return binpath
    print "Unable to find LigHTTPd executable %s" % binpath
    _missing_lighttpd()


def lighttpd_module_path():
    """Returns the library module path for LigHTTPd"""
    modpath = "/usr/lib/lighttpd"
    if os.path.exists(modpath):
        return modpath
    print "Unable to find LigHTTPd modules %s" % modpath
    _missing_lighttpd()


def lighttpd_php_path():
    """Returns the PHP executable path for LigHTTPd"""
    binpath = "/usr/bin/php-cgi"
    if os.path.exists(binpath):
        return binpath
    print "Unable to find PHP CGI executable %s" % binpath
    _missing_lighttpd()


def wdiff_path():
    """Path to the WDiff executable, which we assume is already installed and
    in the user's $PATH."""
    return 'wdiff'


def image_diff_path(target):
    """Path to the image_diff binary.

    Args:
      target: Build target mode (debug or release)"""
    return _path_from_build_results(target, 'image_diff')


def layout_test_helper_path(target):
    """Path to the layout_test helper binary, if needed, empty otherwise"""
    return ''


def test_shell_path(target):
    """Return the platform-specific binary path for our TestShell.

    Args:
      target: Build target mode (debug or release) """
    if target in ('Debug', 'Release'):
        try:
            debug_path = _path_from_build_results('Debug', 'test_shell')
            release_path = _path_from_build_results('Release', 'test_shell')

            debug_mtime = os.stat(debug_path).st_mtime
            release_mtime = os.stat(release_path).st_mtime

            if debug_mtime > release_mtime and target == 'Release' or \
               release_mtime > debug_mtime and target == 'Debug':
                logging.info('\x1b[31mWarning: you are not running the most '
                             'recent test_shell binary. You need to pass '
                             '--debug or not to select between Debug and '
                             'Release.\x1b[0m')
        # This will fail if we don't have both a debug and release binary.
        # That's fine because, in this case, we must already be running the
        # most up-to-date one.
        except path_utils.PathNotFound:
            pass

    return _path_from_build_results(target, 'test_shell')


def fuzzy_match_path():
    """Return the path to the fuzzy matcher binary."""
    return path_utils.path_from_base('third_party', 'fuzzymatch', 'fuzzymatch')


def shut_down_http_server(server_pid):
    """Shut down the lighttpd web server. Blocks until it's fully shut down.

    Args:
      server_pid: The process ID of the running server.
    """
    # server_pid is not set when "http_server.py stop" is run manually.
    if server_pid is None:
        # This isn't ideal, since it could conflict with web server processes
        # not started by http_server.py, but good enough for now.
        kill_all_process('lighttpd')
        kill_all_process('apache2')
    else:
        try:
            os.kill(server_pid, signal.SIGTERM)
            #TODO(mmoss) Maybe throw in a SIGKILL just to be sure?
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
    null = open(os.devnull)
    subprocess.call(['killall', '-TERM', '-u', os.getenv('USER'),
                    process_name], stderr=null)
    null.close()


def kill_all_test_shells():
    """Kills all instances of the test_shell binary currently running."""
    kill_all_process('test_shell')

#
# Private helper functions
#


def _missing_lighttpd():
    print 'Please install using: "sudo apt-get install lighttpd php5-cgi"'
    print 'For complete Linux build requirements, please see:'
    print 'http://code.google.com/p/chromium/wiki/LinuxBuildInstructions'
    sys.exit(1)


def _missing_apache():
    print ('Please install using: "sudo apt-get install apache2 '
        'libapache2-mod-php5"')
    print 'For complete Linux build requirements, please see:'
    print 'http://code.google.com/p/chromium/wiki/LinuxBuildInstructions'
    sys.exit(1)


def _path_from_build_results(*pathies):
    # FIXME(dkegel): use latest or warn if more than one found?
    for dir in ["sconsbuild", "out", "xcodebuild"]:
        try:
            return path_utils.path_from_base(dir, *pathies)
        except:
            pass
    raise path_utils.PathNotFound("Unable to find %s in build tree" %
        (os.path.join(*pathies)))
