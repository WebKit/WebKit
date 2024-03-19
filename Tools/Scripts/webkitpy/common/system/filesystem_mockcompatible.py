# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

try:
    from collections.abc import MutableMapping
except ImportError:
    from collections import MutableMapping

from pyfakefs.fake_filesystem_unittest import Patcher

from webkitcorepy import string_utils

from . import filesystem


class MockCompatibleFileSystem(filesystem.FileSystem):
    def __init__(self, files=None, dirs=None, cwd="/", reset=False):
        try:
            assert Patcher.PATCHER is not None
        except AttributeError:
            pass

        super(MockCompatibleFileSystem, self).__init__()
        self.current_tmpno = 0
        self.last_tmpdir = None
        self._written_files = set()

        if reset:
            self.reset()

        self.maybe_make_directory(cwd)
        self.chdir(cwd)

        if dirs is not None:
            for d in dirs:
                self.maybe_make_directory(d)

        if files is not None:
            for name, contents in files.items():
                d = self.dirname(name)
                if d:
                    self.maybe_make_directory(d)
                if contents is None:
                    continue
                self.write_binary_file(name, string_utils.encode(contents))

    def reset(self):
        for path in self.listdir("/"):
            path = self.join("/", path)
            if self.isdir(path):
                self.rmtree(path)
                # We pass ignore_errors=True to rmtree, so check we didn't error.
                if self.isdir(path):
                    raise Exception("Failed to rmtree")
            else:
                self.remove(path)

    def write_binary_file(self, path, contents):
        self._written_files.add(path)
        d = self.dirname(path)
        if d:
            self.maybe_make_directory(d)
        super(MockCompatibleFileSystem, self).write_binary_file(path, contents)

    def write_text_file(self, path, contents):
        self._written_files.add(path)
        d = self.dirname(path)
        if d:
            self.maybe_make_directory(d)
        super(MockCompatibleFileSystem, self).write_text_file(path, contents)

    def move(self, source, destination):
        d = self.dirname(destination)
        if d:
            self.maybe_make_directory(d)
        super(MockCompatibleFileSystem, self).move(source, destination)

    def expanduser(self, path):
        if path[0] != "~":
            return path
        parts = path.split(self.sep, 1)
        home_directory = self.sep + "Users" + self.sep + "mock"
        if len(parts) == 1:
            return home_directory
        return home_directory + self.sep + parts[1]

    def path_to_module(self, module_name):
        return "/mock-checkout/Tools/Scripts/" + module_name.replace(".", "/") + ".py"

    def mkdtemp(self, **kwargs):
        def _mktemp(suffix="", prefix="tmp", dir=None, **kwargs):
            if dir is None:
                dir = self.sep + "__im_tmp"
            curno = self.current_tmpno
            self.current_tmpno += 1
            self.last_tmpdir = self.join(dir, "%s_%u_%s" % (prefix, curno, suffix))
            self.maybe_make_directory(self.last_tmpdir)
            return self.last_tmpdir

        old = filesystem.tempfile.mkdtemp
        filesystem.tempfile.mkdtemp = _mktemp
        try:
            return super(MockCompatibleFileSystem, self).mkdtemp(**kwargs)
        finally:
            filesystem.tempfile.mkdtemp = old

    def open_text_file_for_writing(self, path, *args, **kwargs):
        self._written_files.add(path)
        return super(MockCompatibleFileSystem, self).open_text_file_for_writing(
            path, *args, **kwargs
        )

    def clear_written_files(self):
        self._written_files = set()

    @property
    def files(self):
        return FilesMapping(self)

    @property
    def written_files(self):
        files = self.files
        return {k: files.get(k) for k in self._written_files}

    @property
    def _slow_but_correct_join(self):
        return self.join

    @property
    def _slow_but_correct_normpath(self):
        return self.normpath


class FilesMapping(MutableMapping):
    def __init__(self, fs):
        self.fs = fs

    def __getitem__(self, name):
        if not self.fs.isfile(name):
            return None
        return self.fs.read_binary_file(name)

    def __setitem__(self, name, value):
        d = self.fs.dirname(name)
        if d:
            self.fs.maybe_make_directory(d)
        self.fs.write_binary_file(name, string_utils.encode(value))

    def __delitem__(self, name):
        self.fs.remove(name)

    def __iter__(self):
        return iter(self.fs.files_under("/"))

    def __len__(self):
        return len(self.fs.files_under("/"))
