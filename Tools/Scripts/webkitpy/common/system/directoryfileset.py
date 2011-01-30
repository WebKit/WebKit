# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.common.system.fileset import FileSetFileHandle
from webkitpy.common.system.filesystem import FileSystem


class DirectoryFileSet(object):
    """The set of files under a local directory."""
    def __init__(self, path, filesystem=None):
        self._path = path
        self._filesystem = filesystem or FileSystem()
        if not self._path.endswith(self._filesystem.sep):
            self._path += self._filesystem.sep

    def _full_path(self, filename):
        assert self._is_under(self._path, filename)
        return self._filesystem.join(self._path, filename)

    def _drop_directory_prefix(self, path):
        return path[len(self._path):]

    def _files_in_directory(self):
        """Returns a list of all the files in the directory, including the path
           to the directory"""
        return self._filesystem.files_under(self._path)

    def _is_under(self, dir, filename):
        return bool(self._filesystem.relpath(self._filesystem.join(dir, filename), dir))

    def open(self, filename):
        return FileSetFileHandle(self, filename, self._filesystem)

    def namelist(self):
        return map(self._drop_directory_prefix, self._files_in_directory())

    def read(self, filename):
        return self._filesystem.read_text_file(self._full_path(filename))

    def extract(self, filename, path):
        """Extracts a file from this file set to the specified directory."""
        src = self._full_path(filename)
        dest = self._filesystem.join(path, filename)
        # As filename may have slashes in it, we must ensure that the same
        # directory hierarchy exists at the output path.
        self._filesystem.maybe_make_directory(self._filesystem.dirname(dest))
        self._filesystem.copyfile(src, dest)

    def delete(self, filename):
        filename = self._full_path(filename)
        self._filesystem.remove(filename)
