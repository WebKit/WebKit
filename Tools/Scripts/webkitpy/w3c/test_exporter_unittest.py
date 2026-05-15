# Copyright (c) 2017, Apple Inc. All rights reserved.
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

import os
import logging
from unittest.mock import patch

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.test_exporter import WebPlatformTestExporter, parse_args
from webkitbugspy import mocks as bmocks, Tracker, bugzilla
from webkitcorepy import mocks as wkmocks, testing, OutputCapture
from webkitscmpy import mocks as mocks


class TestExporterTest(testing.PathTestCase):
    maxDiff = None
    BUGZILLA_URL = 'https://bugs.example.com'
    basepath = 'mock/repository/wpt'

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    class MockGit(object):
        mock_format_patch_result = b'my patch containing some diffs'

        def __init__(self, repository_directory, patch_directories, executive, filesystem):
            self.calls = [repository_directory]

        def create_patch(self, commit, arguments, commit_message=False, find_branch=False):
            self.calls.append('create_patch ' + commit + ' ' + str(arguments))
            return self.mock_format_patch_result

    class MyMockHost(MockHost):
        def __init__(self):
            MockHost.__init__(self)
            self.executive = MockExecutive2(exception=OSError())
            self.filesystem = MockFileSystem()
            self._mockSCM = TestExporterTest.MockGit(None, None, None, None)

        def scm(self):
            return self._mockSCM

    def test_export(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        mock_linter_class.assert_called_once_with(self.path)
        mock_linter_class.return_value.lint.assert_called_once_with()

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Creating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/1'],
        )

    def test_export_no_git_commit(self):
        with mocks.local.Git(self.path), patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ):
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            options = parse_args(['test_exporter.py', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            with self.assertRaises(ValueError) as context:
                WebPlatformTestExporter(host, options)
            self.assertIn('--git-commit', str(context.exception))

    def test_export_no_bugzilla_issue_in_commit(self):
        with mocks.local.Git(self.path) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ):
            git_mock.head.message = 'Commit with no bug reference\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            with self.assertRaises(ValueError) as context:
                WebPlatformTestExporter(host, options)
            self.assertIn('Unable to find associated bug', str(context.exception))

    def test_export_local_branch_exists(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            # Pre-create the local branch before export
            git_mock.checkout('wpt-export-for-webkit-1', create=True)
            git_mock.checkout(git_mock.default_branch)

            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Creating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/1'],
        )

    def test_export_remote_branch_exists_no_pr(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            # Pre-populate the remote branch (simulating a previous push) but no PR
            git_mock.remotes['username/wpt-export-for-webkit-1'] = git_mock.commits[git_mock.default_branch][:]

            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Creating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/1'],
        )

    def test_export_updates_existing_open_pr(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            # Remote branch must exist for the push to be rejected as non-fast-forward
            git_mock.remotes['username/wpt-export-for-webkit-1'] = git_mock.commits[git_mock.default_branch][:]
            wpt_remote.pull_requests.append({
                'number': 42,
                'state': 'open',
                'title': 'Old title',
                'body': 'Old body',
                'user': {'login': 'USER'},
                'head': {'ref': 'wpt-export-for-webkit-1', 'sha': 'abc123', 'label': 'USER:wpt-export-for-webkit-1'},
                'base': {'ref': 'master', 'label': 'web-platform-tests:master', 'user': {'login': 'web-platform-tests'}},
                'draft': False,
                '_links': {'issue': {'href': 'https://api.github.com/repos/web-platform-tests/wpt/issues/42'}},
                'reviews': [],
            })
            wpt_remote.issues[42] = dict(
                creator=wpt_remote.users.create(username='USER'),
                timestamp=0,
                assignee=None,
                comments=[],
                title='Old title',
                opened=True,
                description='Old body',
            )

            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/42')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/42'])

            # No new PR should have been created
            self.assertEqual(len(wpt_remote.pull_requests), 1)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 42 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Updating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/42'],
        )

    def test_export_local_and_remote_exist_with_open_pr(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            # Pre-create the local branch
            git_mock.checkout('wpt-export-for-webkit-1', create=True)
            git_mock.checkout(git_mock.default_branch)

            # Remote branch must exist for the push to be rejected as non-fast-forward
            git_mock.remotes['username/wpt-export-for-webkit-1'] = git_mock.commits[git_mock.default_branch][:]
            wpt_remote.pull_requests.append({
                'number': 42,
                'state': 'open',
                'title': 'Old title',
                'body': 'Old body',
                'user': {'login': 'USER'},
                'head': {'ref': 'wpt-export-for-webkit-1', 'sha': 'abc123', 'label': 'USER:wpt-export-for-webkit-1'},
                'base': {'ref': 'master', 'label': 'web-platform-tests:master', 'user': {'login': 'web-platform-tests'}},
                'draft': False,
                '_links': {'issue': {'href': 'https://api.github.com/repos/web-platform-tests/wpt/issues/42'}},
                'reviews': [],
            })
            wpt_remote.issues[42] = dict(
                creator=wpt_remote.users.create(username='USER'),
                timestamp=0,
                assignee=None,
                comments=[],
                title='Old title',
                opened=True,
                description='Old body',
            )

            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/42')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/42'])

            # No new PR should have been created
            self.assertEqual(len(wpt_remote.pull_requests), 1)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Updated 'PR 42 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Updating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/42'],
        )

    def test_export_creates_new_pr_when_existing_is_closed(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            # Pre-populate a closed PR for the same branch
            wpt_remote.pull_requests.append({
                'number': 42,
                'state': 'closed',
                'title': 'Old title',
                'body': 'Old body',
                'user': {'login': 'USER'},
                'head': {'ref': 'wpt-export-for-webkit-1', 'sha': 'abc123', 'label': 'USER:wpt-export-for-webkit-1'},
                'base': {'ref': 'master', 'label': 'web-platform-tests:master', 'user': {'login': 'web-platform-tests'}},
                'draft': False,
                '_links': {'issue': {'href': 'https://api.github.com/repos/web-platform-tests/wpt/issues/42'}},
                'reviews': [],
            })
            wpt_remote.issues[42] = dict(
                creator=wpt_remote.users.create(username='USER'),
                timestamp=0,
                assignee=None,
                comments=[],
                title='Old title',
                opened=False,
                description='Old body',
            )

            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')

            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/43')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/43'])

            # A new PR should have been created (total: original closed + new open)
            self.assertEqual(len(wpt_remote.pull_requests), 2)

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 43 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Creating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/43'],
        )

    def test_export_with_specific_branch(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-bn', 'wpt-export-branch', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()
            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        mock_linter_class.assert_called_once_with(self.path)
        mock_linter_class.return_value.lint.assert_called_once_with()

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-branch',
                '',
                "Creating pull-request for 'username:wpt-export-branch'...",
                'Removing local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/1'],
        )

    def test_export_no_clean(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ) as mock_linter_class:
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.filesystem.write_binary_file(f'{self.path}/resources/testharness.js', '')
            host.filesystem.write_binary_file(f'{self.path}/wpt', '')
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '--no-clean', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()
            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        mock_linter_class.assert_called_once_with(self.path)
        mock_linter_class.return_value.lint.assert_called_once_with()

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n",
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
                f'Using the WPT repository found at `{self.path}`',
                'Fetching web-platform-tests repository',
                'Cleaning web-platform-tests master branch',
                'Applying patch to web-platform-tests branch wpt-export-for-webkit-1',
                'Pushing branch wpt-export-for-webkit-1 to username...',
                'Branch available at https://github.com/username/wpt/tree/wpt-export-for-webkit-1',
                '',
                "Creating pull-request for 'username:wpt-export-for-webkit-1'...",
                'Keeping local branch wpt-export-for-webkit-1',
                'WPT Pull Request: https://github.com/web-platform-tests/wpt/pull/1'],
        )

    def test_export_interactive_mode(self):
        with OutputCapture(level=logging.INFO), bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '--interactive', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()

    def test_export_invalid_token(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host.web.responses.append({'status_code': 401})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with OutputCapture(level=logging.INFO), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), self.assertRaises(Exception) as context, patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()
            self.assertIn('OAuth token is not valid', str(context.exception))

    def test_export_wrong_token(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host.web.responses.append({'status_code': 200, 'body': '{"login": "DIFF_USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with OutputCapture(level=logging.INFO), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), self.assertRaises(Exception) as context, patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
            exporter.do_export()
            self.assertIn('OAuth token does not match the provided username', str(context.exception))

    def test_has_wpt_changes(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
            self.assertTrue(exporter.has_wpt_changes())

    def test_has_no_wpt_changes_for_no_diff(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = None
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
            self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_expected_file(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected.xht b/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected.xht

+change to expected

diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt

+change to expected

diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.serviceworker.html b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.serviceworker.html

+change to any.serviceworker

diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.sharedworker.html b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.sharedworker.html

+change to any.sharedworker
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_expected_mismatch_file(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected-mismatch.html b/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected-mismatch.html

+change to expected-mismatch
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_w3c_import_log(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log

+change to w3c import
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as git_mock, patch(
            'webkitpy.common.webkit_finder.WebKitFinder.webkit_base', return_value=self.path,
        ), patch(
            'webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)],
        ), patch(
            'webkitpy.w3c.test_exporter.WPTLinter', autospec=True, spec_set=True,
        ):
            git_mock.head.message = f'Test\n{self.BUGZILLA_URL}/show_bug.cgi?id=1\n'
            exporter = WebPlatformTestExporter(host, options)
        self.assertFalse(exporter.has_wpt_changes())
