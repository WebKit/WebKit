# Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

from __future__ import annotations

import os
import shutil
from typing import Iterator

from webkitcorepy.subprocess_utils import run
from webkitcorepy.decorators import hybridmethod


class Editor(object):
    @classmethod
    def sublime(cls) -> Editor:
        path = shutil.which('subl') or '/Applications/Sublime Text.app/Contents/SharedSupport/bin/subl'
        return cls(
            name='Sublime',
            path=path,
            command=[path, '-n'],
            wait=['-w'],
        )

    @classmethod
    def bbedit(cls) -> Editor:
        path = shutil.which('bbedit') or '/Applications/BBEdit.app/Contents/Helpers/bbedit_tool'
        return cls(
            name='BBEdit',
            path=path,
            command=[path, '--view-top'],
            wait=['--wait', '--resume'],
        )

    @classmethod
    def textmate(cls) -> Editor:
        return cls(
            name='TextMate',
            path=shutil.which('mate') or '/Applications/TextMate.app/Contents/Resources/mate',
            wait=['-w'],
        )

    @classmethod
    def xcode(cls) -> Editor:
        return cls(
            name='Xcode',
            path=shutil.which('xed') or '/Applications/Xcode.app/Contents/Developer/usr/bin/xed',
            wait=['-w'],
        )

    @classmethod
    def textedit(cls) -> Editor:
        return cls(
            name='TextEdit',
            path='/System/Applications/TextEdit.app/Contents/MacOS/TextEdit',
            command=['open', '-a', 'TextEdit'],
            wait=['--wait-apps'],
        )

    @classmethod
    def vscode(cls) -> Editor:
        path = shutil.which('code') or '/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code'
        return cls(
            name='VSCode',
            path=path,
            command=[path, '-n'],
            wait=['-w'],
        )

    @classmethod
    def vi(cls) -> Editor:
        path = shutil.which('vi')
        return cls(
            name='vi',
            path=path,
            command=[path],
        )

    @classmethod
    def default(cls) -> Editor:
        path = shutil.which('open')
        return cls(
            name='open',
            path=path,
            command=[path, '-t'],
            wait=['--wait-apps'],
        )

    @classmethod
    def preferred(cls) -> Editor:
        for program in cls.programs():
            if program:
                return program
        return cls.default()

    @classmethod
    def by_name(cls, name: str) -> Editor | None:
        for program in cls.programs():
            if program.name.lower().startswith(name.lower()):
                return program
        return None

    @classmethod
    def programs(cls, exists: bool = True) -> Iterator[Editor]:
        for program in [
            Editor.sublime(),
            Editor.textmate(),
            Editor.bbedit(),
            Editor.vscode(),
            Editor.xcode(),
            Editor.textedit(),
            Editor.vi(),
            Editor.default(),
        ]:
            if not exists or program:
                yield program

    def __init__(self, name: str, path: str | None, command: list[str | None] | None = None, wait: list[str] | None = None) -> None:
        self.name = name
        self.path = path
        self.command: list[str | None] = command or [self.path]
        self.wait: list[str | None] = self.command + list(wait or [])

    @hybridmethod
    def open(context: Editor | type[Editor], file: str, block: bool = False) -> bool:
        if isinstance(context, type):
            context = context.preferred()

        if not context.path or not os.path.isfile(context.path):
            return False
        return not run((context.wait if block else context.command) + [file], capture_output=True).returncode

    def __repr__(self) -> str:
        return self.name

    def __bool__(self) -> bool:
        return bool(self.path and os.path.isfile(self.path))
