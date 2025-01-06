# Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

import logging
import os
import sys

from webkitcorepy import OutputCapture, Version, run, testing
from webkitcorepy.mocks import Terminal as MockTerminal

from webkitscmpy import local, mocks, program


class TestInstallHooks(testing.PathTestCase):
    basepath = 'mock/repository'

    @classmethod
    def write_hook(cls, path, version=None, security_levels=False):
        with open(path, 'w') as f:
            f.write('#!/usr/bin/env {{ python }}\n')
            if version:
                f.write("VERSION = '{}'\n".format(version))
            if security_levels:
                f.write('SECURITY_LEVELS = {{ security_levels }}\n')
            f.write("print('Hello, world!\\n')\n")

    def write_config(self, **kwargs):
        if not kwargs:
            return
        with open(os.path.join(self.path, '.git', 'config'), 'a') as f:
            for remote, args in kwargs.items():
                f.write('[webkitscmpy "remotes.{}"]\n'.format(remote))
                for key, value in args.items():
                    f.write('    {} = {}\n'.format(key, value))

    def setUp(self):
        super(TestInstallHooks, self).setUp()
        os.mkdir(os.path.join(self.path, '.git'))
        os.mkdir(os.path.join(self.path, 'hooks'))
        os.mkdir(os.path.join(self.path, '.git', 'hooks'))
        os.mkdir(os.path.join(self.path, '.svn'))

    def test_regex(self):
        self.assertEqual(
            program.InstallHooks.REMOTE_RE.match('https://github.com/WebKit/WebKit').groups(),
            ('https://', None, 'github.com', '/', 'WebKit/WebKit', None),
        )
        self.assertEqual(
            program.InstallHooks.REMOTE_RE.match('ssh://github.com/WebKit/WebKit').groups(),
            ('ssh://', None, 'github.com', '/', 'WebKit/WebKit', None),
        )
        self.assertEqual(
            program.InstallHooks.REMOTE_RE.match('git@github.com:WebKit/WebKit.git').groups(),
            (None, 'git@', 'github.com', ':', 'WebKit/WebKit', '.git'),
        )
        self.assertEqual(
            program.InstallHooks.REMOTE_RE.match('ssh://git@github.com/WebKit/WebKit').groups(),
            ('ssh://', 'git@', 'github.com', '/', 'WebKit/WebKit', None),
        )

    def test_security_levels(self):
        with OutputCapture(level=logging.INFO), mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            self.write_config(
                origin={
                    'url': 'git@example.org:project/project',
                    'security-level': 0,
                }, security={
                    'url': 'git@example.org:project/project-security',
                    'security-level': 1,
                },
            )

            self.assertDictEqual({
                'example.org:project/project': 0,
                'example.org:project/project-security': 1,
            }, program.InstallHooks._security_levels(local.Git(self.path)))

    def test_version_re(self):
        self.assertEqual(program.InstallHooks.VERSION_RE.match("VERSION = '1.2.3'").group('number'), '1.2.3')
        self.assertEqual(program.InstallHooks.VERSION_RE.match("VERSION='1.2.3'").group('number'), '1.2.3')
        self.assertEqual(program.InstallHooks.VERSION_RE.match('VERSION = "1.2.3"').group('number'), '1.2.3')

        self.assertEqual(program.InstallHooks.VERSION_RE.match('import os'), None)

    def test_install_hook(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-commit'))
            self.assertEqual(0, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Successfully installed 1 of 1 repository hooks\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            "Configuring and copying hook 'pre-commit' for this repository\n",
        )

        resolved_hook = os.path.join(self.path, '.git', 'hooks', 'pre-commit')
        self.assertTrue(os.path.isfile(resolved_hook))
        with open(resolved_hook, 'r') as f:
            self.assertEqual(
                f.read(),
                "#!/usr/bin/env {}\nprint('Hello, world!\\n')\n".format(os.path.basename(sys.executable)),
            )

    def test_install_hook_alternate_hook_path_change(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('Change'), mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            run(
                [local.Git.executable(), 'config', 'core.hookspath', os.path.join(self.path, 'alternate-hooks-path')],
                capture_output=True, cwd=self.path,
            )
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-commit'))
            self.assertEqual(0, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))
            self.assertEqual(
                local.Git(self.path).config().get('core.hookspath'),
                os.path.join(self.path, '.git/hooks'),
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            "git believes hooks for this repository are in '{}', but they should be in '{}' ([Change]/Use/Exit): \n"
            'Successfully installed 1 of 1 repository hooks\n'.format(
                os.path.join(self.path, 'alternate-hooks-path'),
                os.path.join(self.path, '.git/hooks'),
            )
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            "Configuring and copying hook 'pre-commit' for this repository\n",
        )

        resolved_hook = os.path.join(self.path, '.git', 'hooks', 'pre-commit')
        self.assertTrue(os.path.isfile(resolved_hook))
        with open(resolved_hook, 'r') as f:
            self.assertEqual(
                f.read(),
                "#!/usr/bin/env {}\nprint('Hello, world!\\n')\n".format(os.path.basename(sys.executable)),
            )

    def test_install_hook_alternate_hook_path_use(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('Use'), mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            run(
                [local.Git.executable(), 'config', 'core.hookspath', os.path.join(self.path, 'alternate-hooks-path')],
                capture_output=True, cwd=self.path,
            )
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-commit'))
            self.assertEqual(0, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))
            self.assertEqual(
                local.Git(self.path).config().get('core.hookspath'),
                os.path.join(self.path, 'alternate-hooks-path'),
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            "git believes hooks for this repository are in '{}', but they should be in '{}' ([Change]/Use/Exit): \n"
            'Successfully installed 1 of 1 repository hooks\n'.format(
                os.path.join(self.path, 'alternate-hooks-path'),
                os.path.join(self.path, '.git/hooks'),
            )
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            "Configuring and copying hook 'pre-commit' for this repository\n",
        )

        resolved_hook = os.path.join(self.path, 'alternate-hooks-path', 'pre-commit')
        self.assertTrue(os.path.isfile(resolved_hook))
        with open(resolved_hook, 'r') as f:
            self.assertEqual(
                f.read(),
                "#!/usr/bin/env {}\nprint('Hello, world!\\n')\n".format(os.path.basename(sys.executable)),
            )

    def test_install_hook_alternate_hook_path_exit(self):
        with OutputCapture(level=logging.INFO) as captured, MockTerminal.input('Exit'), mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            run(
                [local.Git.executable(), 'config', 'core.hookspath', os.path.join(self.path, 'alternate-hooks-path')],
                capture_output=True, cwd=self.path,
            )
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-commit'))
            self.assertEqual(1, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))

        self.assertEqual(captured.stdout.getvalue(), "git believes hooks for this repository are in '{}', but they should be in '{}' ([Change]/Use/Exit): \n".format(
            os.path.join(self.path, 'alternate-hooks-path'),
            os.path.join(self.path, '.git/hooks'),
        ))
        self.assertEqual(captured.stderr.getvalue(), 'No hooks installed because user canceled hook installation\n')
        self.assertEqual(captured.root.log.getvalue(), '')

    def test_security_level_in_hook(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-push'), security_levels=True)
            self.write_config(
                origin={
                    'url': 'git@example.org:project/project',
                    'security-level': 0,
                }, security={
                    'url': 'git@example.org:project/project-security',
                    'security-level': 1,
                },
            )
            self.assertEqual(0, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Successfully installed 1 of 1 repository hooks\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            "Configuring and copying hook 'pre-push' for this repository\n",
        )

        resolved_hook = os.path.join(self.path, '.git', 'hooks', 'pre-push')
        self.assertTrue(os.path.isfile(resolved_hook))
        with open(resolved_hook, 'r') as f:
            self.assertEqual(
                f.read(), '''#!/usr/bin/env {}
SECURITY_LEVELS = {{'example.org:project/project': 0,
 'example.org:project/project-security': 1}}
print('Hello, world!\\n')
'''.format(os.path.basename(sys.executable)),
            )

    def test_security_level_addition(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-push'), security_levels=True)
            self.write_config(
                origin={
                    'url': 'git@example.org:project/project',
                    'security-level': 0,
                }, security={
                    'url': 'git@example.org:project/project-security',
                    'security-level': 1,
                },
            )
            self.assertEqual(0, program.main(
                args=('install-hooks', '-v', '--level', 'example.org:organization/project=2'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Successfully installed 1 of 1 repository hooks\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(
            captured.root.log.getvalue(),
            "Configuring and copying hook 'pre-push' for this repository\n",
        )

        resolved_hook = os.path.join(self.path, '.git', 'hooks', 'pre-push')
        self.assertTrue(os.path.isfile(resolved_hook))
        with open(resolved_hook, 'r') as f:
            self.assertEqual(
                f.read(), '''#!/usr/bin/env {}
SECURITY_LEVELS = {{'example.org:project/project': 0,
 'example.org:project/project-security': 1,
 'example.org:organization/project': 2}}
print('Hello, world!\\n')
'''.format(os.path.basename(sys.executable)),
            )

    def test_security_level_override(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path, remote='git@example.org:project/project'), mocks.local.Svn():
            self.write_hook(os.path.join(self.path, 'hooks', 'pre-push'), security_levels=True)
            self.write_config(
                origin={
                    'url': 'git@example.org:project/project',
                    'security-level': 0,
                }, security={
                    'url': 'git@example.org:project/project-security',
                    'security-level': 1,
                },
            )
            self.assertEqual(1, program.main(
                args=('install-hooks', '-v', '--level', 'example.org:project/project=2'),
                path=self.path,
                hooks=os.path.join(self.path, 'hooks'),
            ))

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), "'example.org:project/project' already has a security level of '0'\n")
        self.assertEqual(captured.root.log.getvalue(), '')

        resolved_hook = os.path.join(self.path, '.git', 'hooks', 'pre-push')
        self.assertFalse(os.path.isfile(resolved_hook))

    def test_svn(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn(self.path):
            self.assertEqual(1, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'Can only install hooks in a native git repository\n')

    def test_none(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(), mocks.local.Svn():
            self.assertEqual(1, program.main(
                args=('install-hooks', '-v'),
                path=self.path,
            ))
        self.assertEqual(captured.stderr.getvalue(), 'Can only install hooks in a native git repository\n')

    def test_no_file(self):
        hook = os.path.join(self.path, 'invalid-hook')
        self.assertEqual(program.InstallHooks.version_for(hook), None)

    def test_no_version(self):
        hook = os.path.join(self.path, 'hook')
        self.write_hook(hook)
        self.assertEqual(program.InstallHooks.version_for(hook), None)

    def test_version(self):
        hook = os.path.join(self.path, 'hook')
        self.write_hook(hook, '1.0')
        self.assertEqual(program.InstallHooks.version_for(hook), Version(1, 0))

    def test_equal_version(self):
        template = os.path.join(self.path, 'hook')
        hook = os.path.join(self.path, '.git', 'hooks', 'hook')
        self.write_hook(template, '1.0')
        self.write_hook(hook, '1.0')
        with mocks.local.Git(self.path):
            self.assertFalse(program.InstallHooks.hook_needs_update(local.Git(self.path), template))

    def test_no_hook(self):
        template = os.path.join(self.path, 'hook')
        self.write_hook(template)
        with mocks.local.Git(self.path):
            self.assertTrue(program.InstallHooks.hook_needs_update(local.Git(self.path), template))

    def test_old_hook(self):
        template = os.path.join(self.path, 'hook')
        hook = os.path.join(self.path, '.git', 'hooks', 'hook')
        self.write_hook(template, '1.1')
        self.write_hook(hook, '1.0')
        with mocks.local.Git(self.path):
            self.assertTrue(program.InstallHooks.hook_needs_update(local.Git(self.path), template))

    def test_no_template(self):
        template = os.path.join(self.path, 'invalid-hook')
        hook = os.path.join(self.path, '.git', 'hooks', 'invalid-hook')
        self.write_hook(hook, '1.0')
        with mocks.local.Git(self.path):
            self.assertFalse(program.InstallHooks.hook_needs_update(local.Git(self.path), template))
