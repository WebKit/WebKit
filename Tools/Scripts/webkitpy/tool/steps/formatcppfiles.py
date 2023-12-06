# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import logging
import sys
import os

from webkitcorepy import string_utils
from webkitpy.common.checkout.diff_parser import DiffParser
from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.systemhost import SystemHost
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options

_log = logging.getLogger(__name__)

_formatted_suffixes = set(['.cpp', '.cc', '.c', '.h', '.hh', '.mm'])


def _is_formatted(file_name):
    suffix = os.path.splitext(file_name)[1]
    return suffix in _formatted_suffixes


def _ranges(seq):
    if not seq:
        return
    start, end = seq[0], seq[0]
    count = start
    for item in seq:
        if not count == item:
            yield start, end
            start, end = item, item
            count = item
        end = item
        count += 1
    yield start, end


class FormatCppFiles(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.format_cpp_files,
        ]

    def __init__(self, tool, options, host=None, scm=None):
        self._tool = tool
        self._options = options
        self._host = host or SystemHost.get_default()

    def run(self, state):
        if not self._options.format_cpp_files:
            return
        if not any(_is_formatted(file) for file in self._changed_files(state)):
            return
        diff = self.cached_lookup(state, 'diff')
        patch_string = string_utils.decode(diff, target_type=str)
        patch_files = DiffParser(patch_string.splitlines()).files
        for path, diff_file in patch_files.items():
            if not _is_formatted(path):
                continue
            line_numbers = diff_file.added_or_modified_line_numbers()
            if not line_numbers:
                continue
            if not self._host.platform.is_mac():
                clang_format_command = ['clang-format']
            else:
                clang_format_command = ['xcrun', 'clang-format']
            args = clang_format_command + ['-i', '--sort-includes', '-Wno-clang-format-violations'] + ['--lines={}:{}'.format(r[0], r[1]) for r in _ranges(line_numbers)] + [path]
            try:
                self._tool.executive.run_and_throw_if_fail(args, self._options.quiet, cwd=self._tool.scm().checkout_root)
            except ScriptError:
                _log.error('Unable to format C++/C files.')
                sys.exit(1)
