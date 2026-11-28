# Copyright (C) 2026 Gerard Recinto. All rights reserved.
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

import os
import platform
import runpy
import shutil
import sys
import tempfile
import types
import unittest
from unittest import mock


class AutoInstallDirectoryTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.main_repository = os.path.join(self.directory.name, 'main')
        os.makedirs(os.path.join(self.main_repository, '.git'))
        os.makedirs(os.path.join(self.main_repository, 'Tools', 'Scripts', 'libraries', 'autoinstalled'))

    def load_directory(self, repository, root=False):
        scripts = os.path.join(repository, 'Tools', 'Scripts')
        os.makedirs(os.path.join(scripts, 'webkitpy'), exist_ok=True)
        os.makedirs(os.path.join(scripts, 'libraries'), exist_ok=True)
        init_file = os.path.join(scripts, 'webkitpy', '__init__.py')
        shutil.copyfile(os.path.join(os.path.dirname(__file__), '__init__.py'), init_file)

        # Run the bootstrap without installing packages or changing the test runner's imports.
        core = mock.Mock()
        modules = dict(webkitcorepy=core, webkitscmpy=types.ModuleType('webkitscmpy'), urllib3=None)
        with mock.patch.dict(sys.modules, modules), mock.patch.dict(os.environ), mock.patch.object(sys, 'path', list(sys.path)):
            with mock.patch('subprocess.check_output', return_value=os.path.join(self.main_repository, '.git').encode()):
                with mock.patch('os.getuid', return_value=0 if root else os.stat(scripts).st_uid, create=True):
                    runpy.run_path(init_file)
        core.AutoInstall.set_directory.assert_called_once()
        return core.AutoInstall.set_directory.call_args[0][0]

    def expected_directory(self, repository):
        return os.path.join(repository, 'Tools', 'Scripts', 'libraries', 'autoinstalled',
                            'python-{}-{}'.format(sys.version_info[0], platform.machine()))

    def test_main_repository(self):
        self.assertEqual(self.load_directory(self.main_repository), self.expected_directory(self.main_repository))

    def test_worktrees_do_not_share_installed_packages(self):
        first = os.path.join(self.directory.name, 'first-worktree')
        second = os.path.join(self.directory.name, 'second-worktree')
        self.assertEqual(self.load_directory(first), self.expected_directory(first))
        self.assertEqual(self.load_directory(second), self.expected_directory(second))

    @unittest.skipUnless(sys.platform == 'darwin', 'macOS fallback directory')
    def test_root_uses_user_library(self):
        self.assertEqual(self.load_directory(self.main_repository, root=True), os.path.join(
            os.path.expanduser('~/Library/webkitpy'), 'autoinstalled',
            'python-{}-{}'.format(sys.version_info[0], platform.machine())))
