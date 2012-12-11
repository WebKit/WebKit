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

import logging

from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.system.executive import ScriptError

_log = logging.getLogger(__name__)

class RunTests(AbstractStep):
    # FIXME: This knowledge really belongs in the commit-queue.
    NON_INTERACTIVE_FAILURE_LIMIT_COUNT = 30

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

        if not self._options.non_interactive:
            # FIXME: We should teach the commit-queue and the EWS how to run these tests.

            python_unittests_command = self._tool.port().run_python_unittests_command()
            if python_unittests_command:
                _log.info("Running Python unit tests")
                self._tool.executive.run_and_throw_if_fail(python_unittests_command, cwd=self._tool.scm().checkout_root)

            perl_unittests_command = self._tool.port().run_perl_unittests_command()
            if perl_unittests_command:
                _log.info("Running Perl unit tests")
                self._tool.executive.run_and_throw_if_fail(perl_unittests_command, cwd=self._tool.scm().checkout_root)

            javascriptcore_tests_command = self._tool.port().run_javascriptcore_tests_command()
            if javascriptcore_tests_command:
                _log.info("Running JavaScriptCore tests")
                self._tool.executive.run_and_throw_if_fail(javascriptcore_tests_command, quiet=True, cwd=self._tool.scm().checkout_root)

        webkit_unit_tests_command = self._tool.port().run_webkit_unit_tests_command()
        if webkit_unit_tests_command:
            _log.info("Running WebKit unit tests")
            args = webkit_unit_tests_command
            if self._options.non_interactive:
                args.append("--gtest_output=xml:%s/webkit_unit_tests_output.xml" % self._tool.port().results_directory)
            try:
                self._tool.executive.run_and_throw_if_fail(args, cwd=self._tool.scm().checkout_root)
            except ScriptError, e:
                _log.info("Error running webkit_unit_tests: %s" % e.message_with_output())

        _log.info("Running run-webkit-tests")
        args = self._tool.port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.extend([
                "--no-new-test-results",
                "--no-show-results",
                "--skip-failing-tests",
                "--exit-after-n-failures=%s" % self.NON_INTERACTIVE_FAILURE_LIMIT_COUNT,
                "--results-directory=%s" % self._tool.port().results_directory,
                "--quiet",
            ])

        if self._options.quiet:
            args.append("--quiet")
        self._tool.executive.run_and_throw_if_fail(args, cwd=self._tool.scm().checkout_root)
