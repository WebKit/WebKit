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

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.system.deprecated_logging import log

class RunTests(AbstractStep):
    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.test,
            Options.non_interactive,
            Options.quiet,
        ]

    def run(self, state):
        if not self._options.test:
            return

        # Run the scripting unit tests first because they're quickest.
        log("Running Python unit tests")
        self._tool.executive.run_and_throw_if_fail(self._tool.port().run_python_unittests_command())
        log("Running Perl unit tests")
        self._tool.executive.run_and_throw_if_fail(self._tool.port().run_perl_unittests_command())

        javascriptcore_tests_command = self._tool.port().run_javascriptcore_tests_command()
        if javascriptcore_tests_command:
            log("Running JavaScriptCore tests")
            self._tool.executive.run_and_throw_if_fail(javascriptcore_tests_command, quiet=True)

        log("Running run-webkit-tests")
        args = self._tool.port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.append("--no-new-test-results")
            args.append("--no-launch-safari")
            args.append("--exit-after-n-failures=1")
            args.append("--wait-for-httpd")
            # FIXME: Hack to work around https://bugs.webkit.org/show_bug.cgi?id=38912
            # when running the commit-queue on a mac leopard machine since compositing
            # does not work reliably on Leopard due to various graphics driver/system bugs.
            if self._tool.port().name() == "Mac" and self._tool.port().is_leopard():
                tests_to_ignore = []
                tests_to_ignore.append("compositing")

                # media tests are also broken on mac leopard due to
                # a separate CoreVideo bug which causes random crashes/hangs
                # https://bugs.webkit.org/show_bug.cgi?id=38912
                tests_to_ignore.append("media")

                args.extend(["--ignore-tests", ",".join(tests_to_ignore)])

        if self._options.quiet:
            args.append("--quiet")
        self._tool.executive.run_and_throw_if_fail(args)

