#!/usr/bin/env python
# Copyright (C) 2011 Google Inc. All rights reserved.
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

"""This module is used to find files used by run-webkit-tests and
perftestrunner. It exposes one public function - find() - which takes
an optional list of paths, optional set of skipped directories and optional
filter callback.

If a list is passed in, the returned list of files is constrained to those
found under the paths passed in. i.e. calling find(["LayoutTests/fast"])
will only return files under that directory.

If a set of skipped directories is passed in, the function will filter out
the files lying in these directories i.e. find(["LayoutTests"], set(["fast"]))
will return everything except files in fast subfolder.

If a callback is passed in, it will be called for the each file and the file
will be included into the result if the callback returns True.
The callback has to take three arguments: filesystem, dirname and filename."""

import time

from webkitpy.common.system import logutils


_log = logutils.get_logger(__file__)


def find(filesystem, base_dir, paths=None, skipped_directories=None, file_filter=None):
    """Finds the set of tests under a given list of sub-paths.

    Args:
      paths: a list of path expressions relative to base_dir
          to search. Glob patterns are ok, as are path expressions with
          forward slashes on Windows. If paths is empty, we look at
          everything under the base_dir.
    """

    global _skipped_directories
    paths = paths or ['*']
    skipped_directories = skipped_directories or set(['.svn', '_svn'])
    return _normalized_find(filesystem, _normalize(filesystem, base_dir, paths), skipped_directories, file_filter)


def _normalize(filesystem, base_dir, paths):
    return [filesystem.normpath(filesystem.join(base_dir, path)) for path in paths]


def _normalized_find(filesystem, paths, skipped_directories, file_filter):
    """Finds the set of tests under the list of paths.

    Args:
      paths: a list of absolute path expressions to search.
          Glob patterns are ok.
    """
    gather_start_time = time.time()
    paths_to_walk = set()

    for path in paths:
        # If there's an * in the name, assume it's a glob pattern.
        if path.find('*') > -1:
            filenames = filesystem.glob(path)
            paths_to_walk.update(filenames)
        else:
            paths_to_walk.add(path)

    # FIXME: I'm not sure there's much point in this being a set. A list would probably be faster.
    all_files = set()
    for path in paths_to_walk:
        files = filesystem.files_under(path, skipped_directories, file_filter)
        all_files.update(set(files))

    gather_time = time.time() - gather_start_time
    _log.debug("Test gathering took %f seconds" % gather_time)

    return all_files
