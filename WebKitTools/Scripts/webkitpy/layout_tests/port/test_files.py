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

"""This module is used to find all of the layout test files used by
run-webkit-tests. It exposes one public function - find() -
which takes an optional list of paths. If a list is passed in, the returned
list of test files is constrained to those found under the paths passed in,
i.e. calling find(["LayoutTests/fast"]) will only return files
under that directory."""

import glob
import os
import time

from webkitpy.common.system import logutils


_log = logutils.get_logger(__file__)


# When collecting test cases, we include any file with these extensions.
_supported_file_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.xhtmlmp', '.pl',
                                  '.php', '.svg'])
# When collecting test cases, skip these directories
_skipped_directories = set(['.svn', '_svn', 'resources', 'script-tests'])


def find(port, paths):
    """Finds the set of tests under port.layout_tests_dir().

    Args:
      paths: a list of command line paths relative to the layout_tests_dir()
          to limit the search to. glob patterns are ok.
    """
    gather_start_time = time.time()
    paths_to_walk = set()
    # if paths is empty, provide a pre-defined list.
    if paths:
        _log.debug("Gathering tests from: %s relative to %s" % (paths, port.layout_tests_dir()))
        for path in paths:
            # If there's an * in the name, assume it's a glob pattern.
            path = os.path.join(port.layout_tests_dir(), path)
            if path.find('*') > -1:
                filenames = glob.glob(path)
                paths_to_walk.update(filenames)
            else:
                paths_to_walk.add(path)
    else:
        _log.debug("Gathering tests from: %s" % port.layout_tests_dir())
        paths_to_walk.add(port.layout_tests_dir())

    # Now walk all the paths passed in on the command line and get filenames
    test_files = set()
    for path in paths_to_walk:
        if os.path.isfile(path) and _is_test_file(path):
            test_files.add(os.path.normpath(path))
            continue

        for root, dirs, files in os.walk(path):
            # Don't walk skipped directories or their sub-directories.
            if os.path.basename(root) in _skipped_directories:
                del dirs[:]
                continue
            # This copy and for-in is slightly inefficient, but
            # the extra walk avoidance consistently shaves .5 seconds
            # off of total walk() time on my MacBook Pro.
            for directory in dirs[:]:
                if directory in _skipped_directories:
                    dirs.remove(directory)

            for filename in files:
                if _is_test_file(filename):
                    filename = os.path.join(root, filename)
                    filename = os.path.normpath(filename)
                    test_files.add(filename)

    gather_time = time.time() - gather_start_time
    _log.debug("Test gathering took %f seconds" % gather_time)

    return test_files


def _has_supported_extension(filename):
    """Return true if filename is one of the file extensions we want to run a
    test on."""
    extension = os.path.splitext(filename)[1]
    return extension in _supported_file_extensions


def _is_reference_html_file(filename):
    """Return true if the filename points to a reference HTML file."""
    if (filename.endswith('-expected.html') or
        filename.endswith('-expected-mismatch.html')):
        _log.warn("Reftests are not supported - ignoring %s" % filename)
        return True
    return False


def _is_test_file(filename):
    """Return true if the filename points to a test file."""
    return (_has_supported_extension(filename) and
            not _is_reference_html_file(filename))
