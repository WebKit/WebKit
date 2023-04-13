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
from webkitpy.common.memoized import memoized


"""Enforces rules for Base.xcconfig files."""


class BaseXcconfigChecker(object):
    categories = set([
        'basexcconfig/missing-wk_default-prefix',
        'basexcconfig/missing-inherited',
        'basexcconfig/overrides-common-base',
    ])

    default_vars = {
        'GCC_OPTIMIZATION_LEVEL': 1,
    }

    def __init__(self, file_path, handle_style_error):
        self._file_path = file_path
        self._handle_style_error = handle_style_error

    @memoized
    def read_common_base_xcconfig_variables(self):
        inherited_vars = {}
        override_vars = {}

        common_base_xcconfig_path = os.path.join(
            os.path.dirname(__file__), '../../../../..', 'Configurations/CommonBase.xcconfig')
        for line in open(common_base_xcconfig_path):
            # Find lines containing assignments.
            lhs, operator, rhs = map(str.strip, line.partition('='))

            # Skip non-assignment lines, comments, and WK_ variables which are allowed to be overridden.
            if operator != '=' or lhs.startswith('//') or lhs.startswith('WK_'):
                continue

            # Discard any setting condition.
            name = lhs.partition('[')[0]

            if '$(inherited)' in rhs:
                inherited_vars[name] = 1
            elif not self.default_vars.get(name):
                override_vars[name] = 1

        return inherited_vars, override_vars

    def check(self, lines):
        (inherited_vars, override_vars) = self.read_common_base_xcconfig_variables()

        for line_number, line in enumerate(lines, start=1):
            # Find lines containing assignments.
            lhs, operator, rhs = map(str.strip, line.partition('='))

            # Skip non-assignment lines, comments, and WK_ variables which are allowed to be overridden.
            if operator != '=' or lhs.startswith('//') or lhs.startswith('WK_'):
                continue

            # Discard any setting condition.
            name = lhs.partition('[')[0]

            if self.default_vars.get(name):
                self._handle_style_error(
                    line_number, 'basexcconfig/missing-wk_default-prefix', 5,
                    '{name} must be declared as WK_DEFAULT_{name} (see CommonBase.xcconfig)'.format(name=name))
            elif inherited_vars.get(name):
                inherited_name = '$(WK_COMMON_{name})'.format(name=name)
                if '$(inherited)' not in rhs and inherited_name not in rhs:
                    self._handle_style_error(
                        line_number, 'basexcconfig/missing-inherited', 5,
                        '{name} must include \'$(inherited)\' or \'$(WK_COMMON_{name})\' (see CommonBase.xcconfig)'.format(name=name))
            elif override_vars.get(name):
                self._handle_style_error(
                    line_number, 'basexcconfig/overrides-common-base', 4,
                    '{name} overrides the same variable in CommonBase.xcconfig'.format(name=name))
