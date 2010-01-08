# Copyright (C) 2010 Google Inc. All rights reserved.
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
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

from webkitpy.steps.abstractstep import AbstractStep
from webkitpy.steps.options import Options
from webkitpy.webkit_logging import log

class RunTests(AbstractStep):
    @classmethod
    def options(cls):
        return [
            Options.build,
            Options.test,
            Options.non_interactive,
            Options.quiet,
            Options.port,
        ]

    def run(self, state):
        if not self._options.build:
            return
        if not self._options.test:
            return

        # Run the scripting unit tests first because they're quickest.
        log("Running Python unit tests")
        self._tool.executive.run_and_throw_if_fail(self.port().run_python_unittests_command())
        log("Running Perl unit tests")
        self._tool.executive.run_and_throw_if_fail(self.port().run_perl_unittests_command())
        log("Running JavaScriptCore tests")
        self._tool.executive.run_and_throw_if_fail(self.port().run_javascriptcore_tests_command(), quiet=True)

        log("Running run-webkit-tests")
        args = self.port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.append("--no-launch-safari")
            args.append("--exit-after-n-failures=1")
        if self._options.quiet:
            args.append("--quiet")
        self._tool.executive.run_and_throw_if_fail(args)

