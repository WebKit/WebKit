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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
Regression tests for the prepare-commit-msg git hook.

Prevents removal of radar.Tracker.RES from breaking get_bugs_string().
See: https://bugs.webkit.org/show_bug.cgi?id=312300
     https://commits.webkit.org/311158@main (follow-up fix)
"""

import importlib.machinery
import importlib.util
import os
import shutil
import subprocess
import sys
import unittest

from unittest.mock import MagicMock, patch

from webkitbugspy import mocks as bugspy_mocks
from webkitcorepy import testing


_TEST_DIR = os.path.dirname(os.path.realpath(__file__))
_SCRIPTS_SUFFIX = os.sep + os.path.join('Tools', 'Scripts') + os.sep
_SCRIPTS_IDX = _TEST_DIR.rfind(_SCRIPTS_SUFFIX)
_SCRIPTS_DIR = _TEST_DIR[:_SCRIPTS_IDX] + os.sep + os.path.join('Tools', 'Scripts') if _SCRIPTS_IDX >= 0 else None
_HOOKS_DIR = os.path.join(_SCRIPTS_DIR, 'hooks') if _SCRIPTS_DIR else None
_HOOK_TEMPLATE = os.path.join(_HOOKS_DIR, 'prepare-commit-msg') if _HOOKS_DIR else None


def _render_hook_template():
    """Render the prepare-commit-msg Jinja2 template into valid Python.

    Uses the same template variables as InstallHooks.main()
    (see webkitscmpy/program/install_hooks.py).
    """
    from jinja2 import Template

    with open(_HOOK_TEMPLATE, 'r') as f:
        return Template(f.read()).render(
            location='Tools/Scripts/hooks/prepare-commit-msg',
            perl=repr(shutil.which('perl') or '/usr/bin/perl'),
            python=os.path.basename(sys.executable),
            prefer_radar=True,
            default_branch='main',
            source_remotes=['origin'],
            trailers_to_strip=[],
        )


@unittest.skipUnless(_HOOK_TEMPLATE and os.path.isfile(_HOOK_TEMPLATE), 'prepare-commit-msg hook template not found')
class TestGetBugsString(testing.PathTestCase):
    """Regression tests for prepare-commit-msg get_bugs_string().

    Loads the real hook template via jinja2 + importlib.util and tests
    get_bugs_string() to ensure radar.Tracker.RES is present and
    correctly matches radar URL formats.

    See: https://bugs.webkit.org/show_bug.cgi?id=312300
    """
    basepath = 'mock/prepare-commit-msg'

    def setUp(self):
        super(TestGetBugsString, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        # Ensure webkitpy is importable (hook imports DiffParser at module level)
        if _SCRIPTS_DIR not in sys.path:
            sys.path.insert(0, _SCRIPTS_DIR)

    def _load_hook_module(self):
        """Load the rendered hook as a Python module via importlib.util.

        Mocks subprocess.run for the module-level git rev-parse call.

        FIXME: Replace the subprocess.run patch with mocks.local.Git(cwd=None)
        once mocks.local.Git supports a cwd parameter. Currently all 83 route
        definitions in webkitscmpy/mocks/local/git.py use cwd=self.path, so
        they don't match subprocess calls made without an explicit cwd. To fix:
        1. Add a cwd=_UNSET parameter to Git.__init__()
        2. Store self._route_cwd = self.path if cwd is _UNSET else cwd
        3. Replace all 83 cwd=self.path with cwd=self._route_cwd in routes
        """
        rendered_source = _render_hook_template()

        tmp_path = os.path.join(self.path, 'prepare-commit-msg.py')
        with open(tmp_path, 'w') as f:
            f.write(rendered_source)

        spec = importlib.util.spec_from_file_location(
            'prepare_commit_msg', tmp_path,
            loader=importlib.machinery.SourceFileLoader('prepare_commit_msg', tmp_path))
        module = importlib.util.module_from_spec(spec)

        original_run = subprocess.run

        def _mock_run(*args, **kwargs):
            cmd = args[0] if args else kwargs.get('args', [])
            if cmd[:3] == ['git', 'rev-parse', '--show-toplevel']:
                mock_result = MagicMock()
                mock_result.stdout = self.path
                return mock_result
            return original_run(*args, **kwargs)

        with patch('subprocess.run', side_effect=_mock_run):
            spec.loader.exec_module(module)

        return module

    def test_radar_url_detected(self):
        """get_bugs_string() detects radar URLs via radar.Tracker.RES."""
        hook = self._load_hook_module()
        with patch.dict(os.environ, {'COMMIT_MESSAGE_BUG': 'rdar://problem/123456'}), \
                bugspy_mocks.Radar():
            result = hook.get_bugs_string()
        self.assertEqual(result, 'rdar://problem/123456')

    def test_non_radar_url_prompts_for_radar(self):
        """Non-radar bug URL with radarclient available prompts for radar."""
        hook = self._load_hook_module()
        bug_url = 'https://bugs.webkit.org/show_bug.cgi?id=12345'
        with patch.dict(os.environ, {'COMMIT_MESSAGE_BUG': bug_url}), \
                bugspy_mocks.Radar():
            result = hook.get_bugs_string()
        self.assertEqual(result, bug_url + '\nInclude a Radar link (OOPS!).')

    def test_no_bug_url(self):
        """No COMMIT_MESSAGE_BUG with radarclient available shows both prompts."""
        hook = self._load_hook_module()
        env = os.environ.copy()
        env.pop('COMMIT_MESSAGE_BUG', None)
        with patch.dict(os.environ, env, clear=True), \
                bugspy_mocks.Radar():
            result = hook.get_bugs_string()
        self.assertEqual(result, 'Need the bug URL (OOPS!).\nInclude a Radar link (OOPS!).')
