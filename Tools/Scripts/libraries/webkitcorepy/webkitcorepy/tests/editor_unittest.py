# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

import unittest

from unittest import mock

from webkitcorepy import Editor


class EditorTest(unittest.TestCase):
    def test_command_without_path(self) -> None:
        editor = Editor(name='editor', path=None)
        self.assertEqual(editor.command, [])
        self.assertEqual(editor.wait, [])

    def test_command_is_copied_from_argument(self) -> None:
        command = ['/usr/bin/editor', '-n']
        editor = Editor(name='editor', path='/usr/bin/editor', command=command, wait=['-w'])
        self.assertEqual(editor.command, ['/usr/bin/editor', '-n'])
        self.assertEqual(editor.wait, ['/usr/bin/editor', '-n', '-w'])

        editor.command.append('-extra')
        self.assertEqual(command, ['/usr/bin/editor', '-n'])

    def test_open_without_path(self) -> None:
        self.assertFalse(Editor(name='editor', path=None).open('file.txt'))

    def test_open_without_path_blocking(self) -> None:
        self.assertFalse(Editor(name='editor', path=None).open('file.txt', block=True))

    def test_vi_without_vi_installed(self) -> None:
        with mock.patch('shutil.which', return_value=None):
            self.assertEqual(Editor.vi().command, [])

    def test_default_without_open_installed(self) -> None:
        with mock.patch('shutil.which', return_value=None):
            self.assertEqual(Editor.default().command, [])
