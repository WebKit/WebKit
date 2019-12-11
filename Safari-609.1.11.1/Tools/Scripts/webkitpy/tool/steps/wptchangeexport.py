# Copyright (c) 2018, Bocoup LLC. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from webkitpy.common.system.executive import ScriptError
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.w3c.test_exporter import WebPlatformTestExporter, parse_args


class WPTChangeExport(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.non_interactive,
            Options.git_commit,
        ]

    def run(self, state):
        if self._options.non_interactive:
            return

        bug_id = state.get("bug_id")
        if not bug_id:
            return

        args = ["--bug", str(bug_id), "--create-pr"]

        if self._options.git_commit:
            args.append("--git-commit")
            args.append(self._options.git_commit)

        test_exporter = WebPlatformTestExporter(self._tool, parse_args(args))
        message = 'Would you like to export the web-platform-tests changes to a WPT GitHub repository?'
        if test_exporter.has_wpt_changes() and self._tool.user.confirm(message):
            test_exporter.do_export()
