# Copyright (C) 2017 Sony Interactive Entertainment
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Sony Interactive Entertainment nor the names
# of its contributors may be used to endorse or promote products derived
# from this software without specific prior written permission.
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

import logging
import re
import sys

from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options

_log = logging.getLogger(__name__)


class SortXcodeProjectFiles(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.sort_xcode_project,
        ]

    def run(self, state):
        if self._options.sort_xcode_project:
            changed_pbxproj = [file for file in self._changed_files(state) if file.endswith(".pbxproj")]
            if (len(changed_pbxproj) > 0):
                args = self._tool.deprecated_port().run_sort_xcode_project_file_command()
                args = args + changed_pbxproj
                try:
                    output = self._tool.executive.run_and_throw_if_fail(args, self._options.quiet, cwd=self._tool.scm().checkout_root)
                    self.did_modify_checkout(state)
                except ScriptError as e:
                    _log.error("Unable to sort modified xcode projects.")
                    sys.exit(1)
