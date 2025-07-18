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

mock_linter = None


class TestExporterTest(testing.PathTestCase):
    maxDiff = None
    BUGZILLA_URL = 'https://bugs.example.com'
    basepath = 'mock/repository/WebKitBuild/w3c-tests/web-platform-tests'

    def setUp(self):
        super().setUp()
        os.mkdir(os.path.join(self.path, '.git'))

    class MockBugzilla(object):
        def __init__(self):
            self.calls = []

        def fetch_bug_dictionary(self, id):
            self.calls.append('fetch bug ' + id)
            return {"title": "my bug title"}

        def post_comment_to_bug(self, id, comment, see_also=None):
            if see_also:
                self.calls.append("Append %s to see also list" % ", ".join(see_also))
            self.calls.append('post comment to bug ' + id + ' : ' + comment)
            return True

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

    class MockWPTLinter(object):
        def __init__(self, repository_directory, filesystem):
            self.calls = [repository_directory]
            # workaround to appease the style checker which thinks
            # exporter._linter is an instance of WPTLinter and
            # complains if we try to access the calls property which
            # only exists on MockWPTLinter
            global mock_linter
            mock_linter = self

        def lint(self):
            self.calls.append('lint')
            return 0

    def test_export(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)]):
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', '1@main', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()

            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        self.assertEqual(mock_linter.calls, [self.path, 'lint'])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n"
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
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

    def test_export_with_specific_branch(self):
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)]):
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-bn', 'wpt-export-branch', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()
            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        self.assertEqual(mock_linter.calls, [self.path, 'lint'])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n"
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
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
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)]):
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '--no-clean', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()
            issue = Tracker.from_string('{}/show_bug.cgi?id=1'.format(self.BUGZILLA_URL))
            self.assertEqual(issue.comments[-1].content, 'Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/1')
            self.assertEqual(issue.related_links, ['https://github.com/web-platform-tests/wpt/pull/1'])

        self.assertEqual(mock_linter.calls, [self.path, 'lint'])

        self.assertEqual(
            captured.stdout.getvalue(),
            "Created 'PR 1 | WebKit export of https://bugs.example.com/show_bug.cgi?id=1'!\n"
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        log = captured.root.log.getvalue().splitlines()
        self.assertEqual(
            [line for line in log if 'Mock process' not in line], [
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
        with OutputCapture(level=logging.INFO) as captured, bmocks.Bugzilla(self.BUGZILLA_URL.split('://')[1], issues=bmocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ), patch('webkitbugspy.Tracker._trackers', [bugzilla.Tracker(self.BUGZILLA_URL)]):
            host = TestExporterTest.MyMockHost()
            host.filesystem.maybe_make_directory(self.path)
            host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
            options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '--interactive', '-d', self.path])
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()

    def test_export_invalid_token(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host.web.responses.append({'status_code': 401})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ), self.assertRaises(Exception) as context:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()
            self.assertIn('OAuth token is not valid', str(context.exception))

    def test_export_wrong_token(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host.web.responses.append({'status_code': 200, 'body': '{"login": "DIFF_USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with OutputCapture(level=logging.INFO) as captured, mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ), self.assertRaises(Exception) as context:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            exporter.do_export()
            self.assertIn('OAuth token does not match the provided username', str(context.exception))

    def test_has_wpt_changes(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as repo:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            self.assertTrue(exporter.has_wpt_changes())

    def test_has_no_wpt_changes_for_no_diff(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = None
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.remote.GitHub(remote='github.com/web-platform-tests/wpt', labels={
            'webkit-export': dict(color='00000', description=''),
        }) as wpt_remote, mocks.local.Git(
            self.path,
            remote='https://{}'.format(wpt_remote.remote)
        ):
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
            self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_expected_file(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt

+change to expected

diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.serviceworker.html b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.serviceworker.html

+change to expected

diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.sharedworker.html b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values.any.sharedworker.html

+change to expected
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as repo:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_expected_mismatch_file(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected-mismatch.html b/LayoutTests/imported/w3c/web-platform-tests/css/css-counter-styles/counter-style-at-rule/empty-string-symbol-expected-mismatch.html

+change to expected-mismatch
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as repo:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_w3c_import_log(self):
        host = TestExporterTest.MyMockHost()
        host.filesystem.maybe_make_directory(self.path)
        host._mockSCM.mock_format_patch_result = b"""
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log

+change to w3c import
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1', '-c', '-n', 'USER', '-t', 'TOKEN', '-d', self.path])
        with mocks.local.Git(self.path) as repo:
            exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockBugzilla, TestExporterTest.MockWPTLinter, 1)
        self.assertFalse(exporter.has_wpt_changes())
