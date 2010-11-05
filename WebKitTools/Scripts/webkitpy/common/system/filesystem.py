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

"""Wrapper object for the file system / source tree."""

from __future__ import with_statement

import codecs
import errno
import os
import tempfile


class FileSystem(object):
    """FileSystem interface for webkitpy.

    Unless otherwise noted, all paths are allowed to be either absolute
    or relative."""

    def exists(self, path):
        """Return whether the path exists in the filesystem."""
        return os.path.exists(path)

    def isdir(self, path):
        """Return whether the path refers to a directory."""
        return os.path.isdir(path)

    def join(self, *comps):
        """Return the path formed by joining the components."""
        return os.path.join(*comps)

    def listdir(self, path):
        """Return the contents of the directory pointed to by path."""
        return os.listdir(path)

    def mkdtemp(self, **kwargs):
        """Create and return a uniquely named directory.

        This is like tempfile.mkdtemp, but if used in a with statement
        the directory will self-delete at the end of the block (if the
        directory is empty; non-empty directories raise errors). The
        directory can be safely deleted inside the block as well, if so
        desired."""
        class TemporaryDirectory(object):
            def __init__(self, **kwargs):
                self._kwargs = kwargs
                self._directory_path = None

            def __enter__(self):
                self._directory_path = tempfile.mkdtemp(**self._kwargs)
                return self._directory_path

            def __exit__(self, type, value, traceback):
                # Only self-delete if necessary.

                # FIXME: Should we delete non-empty directories?
                if os.path.exists(self._directory_path):
                    os.rmdir(self._directory_path)

        return TemporaryDirectory(**kwargs)

    def maybe_make_directory(self, *path):
        """Create the specified directory if it doesn't already exist."""
        try:
            os.makedirs(os.path.join(*path))
        except OSError, e:
            if e.errno != errno.EEXIST:
                raise

    def read_binary_file(self, path):
        """Return the contents of the file at the given path as a byte string."""
        with file(path, 'rb') as f:
            return f.read()

    def read_text_file(self, path):
        """Return the contents of the file at the given path as a Unicode string.

        The file is read assuming it is a UTF-8 encoded file with no BOM."""
        with codecs.open(path, 'r', 'utf8') as f:
            return f.read()

    def write_binary_file(self, path, contents):
        """Write the contents to the file at the given location."""
        with file(path, 'wb') as f:
            f.write(contents)

    def write_text_file(self, path, contents):
        """Write the contents to the file at the given location.

        The file is written encoded as UTF-8 with no BOM."""
        with codecs.open(path, 'w', 'utf8') as f:
            f.write(contents)
