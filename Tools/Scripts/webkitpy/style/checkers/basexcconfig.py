# Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

import os
import re
from webkitpy.common.memoized import memoized


"""Enforces rules for Base.xcconfig files."""


class BaseXcconfigChecker(object):
    categories = set([
        'basexcconfig/missing-wk_default-prefix',
        'basexcconfig/missing-inherited',
        'basexcconfig/duplicate-commonbase-definitions',
        'basexcconfig/overrides-common-base',
    ])

    default_vars = {
        'GCC_OPTIMIZATION_LEVEL': 1,
    }

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    def appends_to_inherited_value(self, rhs):
        # Only a standalone $(inherited) appends to the inherited value. A nested reference,
        # such as $(WK_MACOSX_DEPLOYMENT_TARGET:default=$(inherited)), is a fallback instead.
        return any(word.rstrip(';') == '$(inherited)' for word in rhs.split())

    def logical_lines(self, physical_lines):
        # Splice '\' line continuations into a single logical line, as xcconfig does, so that a
        # multi-line setting whose continuation contains '=' (such as a preprocessor macro value)
        # is not misread as a separate assignment. The yielded line number is that of the logical
        # line's first physical line.
        start_line_number = None
        buffer = ''
        for line_number, physical_line in enumerate(physical_lines, start=1):
            text = physical_line.rstrip('\r\n')
            if start_line_number is None:
                start_line_number = line_number
            if text.endswith('\\'):
                buffer += text[:-1] + ' '
                continue
            yield start_line_number, buffer + text
            start_line_number = None
            buffer = ''
        if buffer:
            yield start_line_number, buffer

    def assignments(self, physical_lines):
        # Yield (line_number, name, rhs) for each assignment line, after splicing '\' line
        # continuations. Comments and non-assignment lines are skipped; 'name' has any setting
        # condition ([config=...]) stripped. WK_ policy is left to each caller, since they differ.
        for line_number, line in self.logical_lines(physical_lines):
            lhs, operator, rhs = map(str.strip, line.partition('='))
            if operator != '=' or lhs.startswith('//'):
                continue
            yield line_number, lhs.partition('[')[0], rhs

    def read_xcconfig_variables(self, physical_lines, inherited_vars, override_vars, wk_common_vars):
        for _, name, rhs in self.assignments(physical_lines):
            # Record WK_COMMON_<NAME> variables, which projects may reference in place of
            # $(inherited) for the setting <NAME>.
            if name.startswith('WK_COMMON_'):
                wk_common_vars[name[len('WK_COMMON_'):]] = 1

            # WK_ variables are allowed to be overridden and are not themselves checked.
            if name.startswith('WK_'):
                continue

            if self.appends_to_inherited_value(rhs):
                inherited_vars[name] = 1
            elif override_vars is not None and not self.default_vars.get(name):
                override_vars[name] = 1

    def included_xcconfig_paths(self, including_path, physical_lines):
        included_paths = []
        for line in physical_lines:
            # Match #include and #include? of a path relative to the including file. Paths
            # resolved by Xcode, such as <DEVELOPER_DIR>/..., cannot be read from here.
            match = re.match(r'#include\??\s+"([^"<]+)"', line.strip())
            if not match:
                continue
            included_path = os.path.normpath(os.path.join(os.path.dirname(including_path), match.group(1)))
            if os.path.exists(included_path):
                included_paths.append(included_path)
        return included_paths

    @memoized
    def read_common_base_xcconfig_variables(self):
        inherited_vars = {}
        override_vars = {}
        wk_common_vars = {}

        common_base_xcconfig_path = os.path.join(
            os.path.dirname(__file__), '../../../../..', 'Configurations/CommonBase.xcconfig')
        with open(common_base_xcconfig_path) as common_base_file:
            common_base_lines = common_base_file.readlines()
        self.read_xcconfig_variables(common_base_lines, inherited_vars, override_vars, wk_common_vars)

        # The files CommonBase.xcconfig includes are part of the common base as well, so the
        # settings they append to must not be clobbered either.
        for included_path in self.included_xcconfig_paths(common_base_xcconfig_path, common_base_lines):
            with open(included_path) as included_file:
                self.read_xcconfig_variables(included_file.readlines(), inherited_vars, None, wk_common_vars)

        return inherited_vars, override_vars, wk_common_vars

    def check(self, lines, line_numbers=None):
        (inherited_vars, override_vars, wk_common_vars) = self.read_common_base_xcconfig_variables()

        for line_number, name, rhs in self.assignments(lines):
            # WK_ variables are allowed to be overridden.
            if name.startswith('WK_'):
                continue

            if self.default_vars.get(name):
                self._handle_style_error(
                    line_number, 'basexcconfig/missing-wk_default-prefix', 5,
                    '{name} must be declared as WK_DEFAULT_{name} (see CommonBase.xcconfig)'.format(name=name))
            elif inherited_vars.get(name):
                inherited_name = '$(WK_COMMON_{name})'.format(name=name)
                references_common = inherited_name in rhs
                if self.appends_to_inherited_value(rhs):
                    if references_common:
                        # $(inherited) already includes the value CommonBase.xcconfig contributes,
                        # so also listing $(WK_COMMON_<name>) duplicates it.
                        self._handle_style_error(
                            line_number, 'basexcconfig/duplicate-commonbase-definitions', 5,
                            '{name} must not list both \'$(inherited)\' and \'$(WK_COMMON_{name})\' (see CommonBase.xcconfig)'.format(name=name))
                elif references_common:
                    if not wk_common_vars.get(name):
                        # There is no WK_COMMON_ variable for this setting, so hard-coding one does
                        # not preserve the inherited value; $(inherited) is the only correct form.
                        self._handle_style_error(
                            line_number, 'basexcconfig/missing-inherited', 5,
                            '{name} must use \'$(inherited)\' instead of \'$(WK_COMMON_{name})\' (see CommonBase.xcconfig)'.format(name=name))
                elif wk_common_vars.get(name):
                    self._handle_style_error(
                        line_number, 'basexcconfig/missing-inherited', 5,
                        '{name} must include \'$(inherited)\' or \'$(WK_COMMON_{name})\' (see CommonBase.xcconfig)'.format(name=name))
                else:
                    self._handle_style_error(
                        line_number, 'basexcconfig/missing-inherited', 5,
                        '{name} must include \'$(inherited)\' (see CommonBase.xcconfig)'.format(name=name))
            elif override_vars.get(name):
                self._handle_style_error(
                    line_number, 'basexcconfig/overrides-common-base', 4,
                    '{name} overrides the same variable in CommonBase.xcconfig'.format(name=name))
