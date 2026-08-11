# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""Integration test for WPTLinter."""

import os
import tempfile
import unittest

from webkitpy.common.system.systemhost import SystemHost
from webkitpy.port.factory import PortFactory
from webkitpy.w3c.wpt_linter import WPTLinter


class WPTLintIntegrationTest(unittest.TestCase):
    def integration_test_lint_runs_without_venv(self):
        port = PortFactory(SystemHost.get_default()).get()
        wpt_dir = port.host.filesystem.join(port.layout_tests_dir(), 'imported', 'w3c', 'web-platform-tests')
        venv_path = os.path.join(wpt_dir, '_venv3')

        if os.path.exists(venv_path):
            holding_path = tempfile.mkdtemp(prefix='_venv3-integrationtest-', dir=wpt_dir)
            os.rename(venv_path, holding_path)
            self.addCleanup(os.rename, holding_path, venv_path)

        for error in WPTLinter(wpt_dir).lint(paths=['README.md']):
            self.assertIsInstance(error, dict)

        self.assertFalse(os.path.exists(venv_path))

    def integration_test_lint_reports_multiple_errors(self):
        port = PortFactory(SystemHost.get_default()).get()
        wpt_dir = port.host.filesystem.join(port.layout_tests_dir(), 'imported', 'w3c', 'web-platform-tests')

        f = tempfile.NamedTemporaryFile(dir=wpt_dir, suffix='.txt', delete=False)
        self.addCleanup(os.remove, f.name)
        f.write(b'\thas a leading tab\n')
        f.write(b'has trailing whitespace \n')
        f.write(b'has a trailing CR\r\n')
        f.close()

        errors = list(WPTLinter(wpt_dir).lint(paths=[f.name]))

        # wpt reports paths relative to its own `os.getcwd()`, which
        # canonicalizes symlinks; f.name doesn't, so we must resolve wpt_dir
        # (but not f.name) through realpath to construct a matching
        # expectation, the same way wpt's `os.path.relpath` ends up doing.
        expected_path = os.path.relpath(f.name, os.path.realpath(wpt_dir))
        self.assertEqual([
            {'path': expected_path, 'lineno': 1, 'rule': 'INDENT TABS',
             'message': 'Test-file line starts with one or more tab characters'},
            {'path': expected_path, 'lineno': 2, 'rule': 'TRAILING WHITESPACE',
             'message': 'Whitespace at EOL'},
            {'path': expected_path, 'lineno': 3, 'rule': 'CR AT EOL',
             'message': 'Test-file line ends with CR (U+000D) character'},
        ], errors)
