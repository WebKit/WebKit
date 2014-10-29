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
import os
import platform
import sys
from webkitpy.tool.steps.abstractstep import AbstractStep
from webkitpy.tool.steps.options import Options
from webkitpy.common.system.executive import ScriptError, Executive

_log = logging.getLogger(__name__)

class RunTests(AbstractStep):
    # FIXME: This knowledge really belongs in the commit-queue.
    NON_INTERACTIVE_FAILURE_LIMIT_COUNT = 30

    @classmethod
    def options(cls):
        return AbstractStep.options() + [
            Options.build_style,
            Options.test,
            Options.non_interactive,
            Options.quiet,
        ]

    def run(self, state):
        if not self._options.test:
            return

        python_unittests_command = self._tool.deprecated_port().run_python_unittests_command()
        if python_unittests_command:
            _log.info("Running Python unit tests")
            if self._options.non_interactive:
                filesystem = self._tool.filesystem
                python_unittest_results_directory = self._tool.port_factory.get().python_unittest_results_directory()
                filesystem.maybe_make_directory(python_unittest_results_directory)

                python_unittests_command.append('--json')
                output = self._tool.executive.run_command(python_unittests_command, cwd=self._tool.scm().checkout_root, error_handler=Executive.ignore_error, return_stderr=False)
                filesystem.write_text_file(filesystem.join(python_unittest_results_directory, "results.json"), output)
            else:
                self._tool.executive.run_and_throw_if_fail(python_unittests_command, cwd=self._tool.scm().checkout_root)

        if not self._options.non_interactive:
            # FIXME: We should teach the commit-queue and the EWS how to run these tests.

            perl_unittests_command = self._tool.deprecated_port().run_perl_unittests_command()
            if perl_unittests_command:
                _log.info("Running Perl unit tests")
                self._tool.executive.run_and_throw_if_fail(perl_unittests_command, cwd=self._tool.scm().checkout_root)

            javascriptcore_tests_command = self._tool.deprecated_port().run_javascriptcore_tests_command()
            if javascriptcore_tests_command:
                _log.info("Running JavaScriptCore tests")
                self._tool.executive.run_and_throw_if_fail(javascriptcore_tests_command, quiet=True, cwd=self._tool.scm().checkout_root)

        bindings_tests_command = self._tool.deprecated_port().run_bindings_tests_command()
        if bindings_tests_command:
            _log.info("Running bindings generation tests")
            args = bindings_tests_command
            try:
                self._tool.executive.run_and_throw_if_fail(args, cwd=self._tool.scm().checkout_root)
            except ScriptError, e:
                _log.info("Error running run-bindings-tests: %s" % e.message_with_output())

        webkit_unit_tests_command = self._tool.deprecated_port().run_webkit_unit_tests_command()
        if webkit_unit_tests_command:
            _log.info("Running WebKit unit tests")
            args = webkit_unit_tests_command
            try:
                self._tool.executive.run_and_throw_if_fail(args, cwd=self._tool.scm().checkout_root)
            except ScriptError, e:
                _log.info("Error running webkit_unit_tests: %s" % e.message_with_output())


        _log.info("Running run-webkit-tests")
        args = self._tool.deprecated_port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.extend([
                "--no-new-test-results",
                "--no-show-results",
                "--exit-after-n-failures=%s" % self.NON_INTERACTIVE_FAILURE_LIMIT_COUNT,
            ])

            # old-run-webkit-tests does not support --skip-failing-tests
            # Using --quiet one Windows fails when we try to use /dev/null, disabling for now until we find a fix
            if sys.platform != "cygwin":
                args.append("--quiet")
                args.append("--skip-failing-tests")
            else:
                args.append("--no-build");

        if self._options.quiet:
            args.append("--quiet")

        self._tool.executive.run_and_throw_if_fail(args, cwd=self._tool.scm().checkout_root)
        
