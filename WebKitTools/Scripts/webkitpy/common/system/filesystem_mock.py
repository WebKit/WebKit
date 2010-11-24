# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import errno
import os
import path


class MockFileSystem(object):
    def __init__(self, files={}):
        """Initializes a "mock" filesystem that can be used to completely
        stub out a filesystem.

        Args:
            files: a dict of filenames -> file contents. A file contents
                value of None is used to indicate that the file should
                not exist.
        """
        self.files = files

    def exists(self, path):
        if path in self.files:
            return self.files[path] is not None
        return False

    def join(self, *comps):
        # os.path.join ignores trailing slashes on components (i.e.
        # join('foo/', 'bar') and join('foo', 'bar') produce the same result),
        # we emulate that behavior.
        trimmed_comps = []
        for comp in comps:
            if len(comp) and comp[-1] == '/':
                trimmed_comps.append(comp[0:-1])
            else:
                trimmed_comps.append(comp)
        return '/'.join(trimmed_comps)

    def maybe_make_directory(self, *path):
        # FIXME: Implement such that subsequent calls to isdir() work?
        pass

    def read_text_file(self, path):
        return self.read_binary_file(path)

    def read_binary_file(self, path):
        if path in self.files:
            if self.files[path] is None:
                raise IOError(errno.ENOENT, path, os.strerror(errno.ENOENT))
            return self.files[path]

    def write_text_file(self, path, contents):
        return self.write_binary_file(path, contents)

    def write_binary_file(self, path, contents):
        self.files[path] = contents
