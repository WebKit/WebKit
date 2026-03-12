# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import tempfile

from webkitcorepy import Editor

from .diff import DiffBase


class EditorDiff(DiffBase):
    editor = None

    def __init__(self, **kwargs):
        super(EditorDiff, self).__init__(**kwargs)

        if self.block is None:
            self.block = False

        self._file_handle = None
        self.file = os.path.join(tempfile.gettempdir(), 'patch.diff')

    def add_line(self, line):
        line = super(EditorDiff, self).add_line(line)
        if not self._file_handle:
            raise ValueError('EditorDiff context has not been initialized')
        self._file_handle.write(line)
        return line

    def __enter__(self):
        if self.editor is None:
            raise ValueError('Undefined editor')
        if not self.editor:
            raise ValueError('{} cannot be found'.format(self.editor.name))

        self._file_handle = open(self.file, 'w')
        return self

    def __exit__(self, *args, **kwargs):
        if not self._file_handle:
            return
        self._file_handle.close()
        self._file_handle = None
        self.editor.open(self.file, block=self.block)


class SublimeDiff(EditorDiff):
    editor = Editor.sublime()
    name = editor.name.lower()


class VSDiff(EditorDiff):
    editor = Editor.vscode()
    name = editor.name.lower()
