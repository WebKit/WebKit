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

import re
import shutil
import subprocess
import sys

from webkitcorepy import NullContext, Terminal

from .diff import DiffBase


class TerminalDiff(DiffBase):
    name = 'terminal'

    def __init__(self, **kwargs):
        super(TerminalDiff, self).__init__(**kwargs)

        if self.block is None:
            self.block = True

        self._is_conflicting = False
        self._pager = None
        self._out = None

    def __enter__(self):
        if self.block and Terminal.isatty(sys.stdout) and shutil.which('less'):
            self._pager = subprocess.Popen(
                ['less', '-R'],
                stdin=subprocess.PIPE,
                encoding='utf-8',
                errors='replace',
            )
            self._out = self._pager.stdin
            Terminal._atty_overrides[self._out.fileno()] = True
        else:
            self._out = sys.stdout
        return self

    def __exit__(self, *args, **kwargs):
        if self._pager:
            Terminal._atty_overrides.pop(self._pager.stdin.fileno(), None)
            self._pager.stdin.close()
            self._pager.wait()
            self._pager = None
        self._out = None

    def add_line(self, line):
        line = super(TerminalDiff, self).add_line(line)
        if not Terminal.isatty(sys.stdout):
            print(line.rstrip())
            return

        add_sub_match = self.ADD_SUB_RE.match(line)
        if add_sub_match:
            parsed_line, _ = line.split(' | ', 1)
            self._out.write('{} | {} '.format(parsed_line, add_sub_match.group(1)))
            if add_sub_match.group(2):
                with Terminal.Style(color=Terminal.Text.green).apply(self._out):
                    self._out.write(add_sub_match.group(2))
            if add_sub_match.group(3):
                with Terminal.Style(color=Terminal.Text.red).apply(self._out):
                    self._out.write(add_sub_match.group(3))
            self._out.write('\n')
            return line

        style = None
        if line.startswith('+<<<'):
            style = Terminal.Style(color=Terminal.Text.magenta)
            self._is_conflicting = True
        elif line.startswith('+===') or line.startswith('+>>>'):
            style = Terminal.Style(color=Terminal.Text.magenta)
            self._is_conflicting = False
        elif line.startswith('diff') or line.startswith('index') or line.startswith('new file') or line.startswith('From') or line.startswith('Date'):
            style = Terminal.Style(color=Terminal.Text.blue)
            self._is_conflicting = False
        elif line.startswith('@@') or line.startswith('---') or line.startswith('+++'):
            style = Terminal.Style(color=Terminal.Text.cyan)
            self._is_conflicting = False
        elif line.startswith('+'):
            style = Terminal.Style(color=Terminal.Text.green)
        elif line.startswith('-') or self._is_conflicting:
            style = Terminal.Style(color=Terminal.Text.red)

        with style.apply(self._out) if style else NullContext():
            self._out.write(line.rstrip())
        self._out.write('\n')
        return line
