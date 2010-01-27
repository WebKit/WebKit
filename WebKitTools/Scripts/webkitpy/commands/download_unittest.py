# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

from webkitpy.commands.commandtest import CommandsTest
from webkitpy.commands.download import *
from webkitpy.mock import Mock

class DownloadCommandsTest(CommandsTest):
    def _default_options(self):
        options = Mock()
        options.force_clean = False
        options.clean = True
        options.check_builders = True
        options.quiet = False
        options.non_interactive = False
        options.update = True
        options.build = True
        options.test = True
        options.close_bug = True
        options.complete_rollout = False
        return options

    def test_build(self):
        expected_stderr = "Updating working directory\nBuilding WebKit\n"
        self.assert_execute_outputs(Build(), [], options=self._default_options(), expected_stderr=expected_stderr)

    def test_build_and_test(self):
        expected_stderr = "Updating working directory\nBuilding WebKit\nRunning Python unit tests\nRunning Perl unit tests\nRunning JavaScriptCore tests\nRunning run-webkit-tests\n"
        self.assert_execute_outputs(BuildAndTest(), [], options=self._default_options(), expected_stderr=expected_stderr)

    def test_apply_attachment(self):
        options = self._default_options()
        options.update = True
        options.local_commit = True
        expected_stderr = "Updating working directory\nProcessing 1 patch from 1 bug.\nProcessing patch 197 from bug 42.\n"
        self.assert_execute_outputs(ApplyAttachment(), [197], options=options, expected_stderr=expected_stderr)

    def test_apply_patches(self):
        options = self._default_options()
        options.update = True
        options.local_commit = True
        expected_stderr = "Updating working directory\n2 reviewed patches found on bug 42.\nProcessing 2 patches from 1 bug.\nProcessing patch 197 from bug 42.\nProcessing patch 128 from bug 42.\n"
        self.assert_execute_outputs(ApplyFromBug(), [42], options=options, expected_stderr=expected_stderr)

    def test_land_diff(self):
        expected_stderr = "Building WebKit\nRunning Python unit tests\nRunning Perl unit tests\nRunning JavaScriptCore tests\nRunning run-webkit-tests\nUpdating bug 42\n"
        self.assert_execute_outputs(Land(), [42], options=self._default_options(), expected_stderr=expected_stderr)

    def test_check_style(self):
        expected_stderr = "Processing 1 patch from 1 bug.\nUpdating working directory\nProcessing patch 197 from bug 42.\nRunning check-webkit-style\n"
        self.assert_execute_outputs(CheckStyle(), [197], options=self._default_options(), expected_stderr=expected_stderr)

    def test_build_attachment(self):
        expected_stderr = "Processing 1 patch from 1 bug.\nUpdating working directory\nProcessing patch 197 from bug 42.\nBuilding WebKit\n"
        self.assert_execute_outputs(BuildAttachment(), [197], options=self._default_options(), expected_stderr=expected_stderr)

    def test_land_attachment(self):
        # FIXME: This expected result is imperfect, notice how it's seeing the same patch as still there after it thought it would have cleared the flags.
        expected_stderr = """Processing 1 patch from 1 bug.
Updating working directory
Processing patch 197 from bug 42.
Building WebKit
Running Python unit tests
Running Perl unit tests
Running JavaScriptCore tests
Running run-webkit-tests
Not closing bug 42 as attachment 197 has review=+.  Assuming there are more patches to land from this bug.
"""
        self.assert_execute_outputs(LandAttachment(), [197], options=self._default_options(), expected_stderr=expected_stderr)

    def test_land_patches(self):
        # FIXME: This expected result is imperfect, notice how it's seeing the same patch as still there after it thought it would have cleared the flags.
        expected_stderr = """2 reviewed patches found on bug 42.
Processing 2 patches from 1 bug.
Updating working directory
Processing patch 197 from bug 42.
Building WebKit
Running Python unit tests
Running Perl unit tests
Running JavaScriptCore tests
Running run-webkit-tests
Not closing bug 42 as attachment 197 has review=+.  Assuming there are more patches to land from this bug.
Updating working directory
Processing patch 128 from bug 42.
Building WebKit
Running Python unit tests
Running Perl unit tests
Running JavaScriptCore tests
Running run-webkit-tests
Not closing bug 42 as attachment 197 has review=+.  Assuming there are more patches to land from this bug.
"""
        self.assert_execute_outputs(LandFromBug(), [42], options=self._default_options(), expected_stderr=expected_stderr)

    def test_rollout(self):
        expected_stderr = "Updating working directory\nRunning prepare-ChangeLog\n\nNOTE: Rollout support is experimental.\nPlease verify the rollout diff and use \"webkit-patch land 12345\" to commit the rollout.\n"
        self.assert_execute_outputs(Rollout(), [852, "Reason"], options=self._default_options(), expected_stderr=expected_stderr)

    def test_complete_rollout(self):
        options = self._default_options()
        options.complete_rollout = True
        expected_stderr = "Will re-open bug 12345 after rollout.\nUpdating working directory\nRunning prepare-ChangeLog\nBuilding WebKit\n"
        self.assert_execute_outputs(Rollout(), [852, "Reason"], options=options, expected_stderr=expected_stderr)
