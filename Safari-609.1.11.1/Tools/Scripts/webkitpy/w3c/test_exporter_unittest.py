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

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.w3c.test_exporter import WebPlatformTestExporter, parse_args
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub

mock_linter = None

class TestExporterTest(unittest.TestCase):
    maxDiff = None

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
        mock_format_patch_result = 'my patch containing some diffs'

        @classmethod
        def clone(cls, url, directory, executive=None):
            return True

        def __init__(self, repository_directory, patch_directories, executive, filesystem):
            self.calls = [repository_directory]

        def fetch(self):
            self.calls.append('fetch')

        def checkout(self, branch):
            self.calls.append('checkout ' + branch)

        def reset_hard(self, commit):
            self.calls.append('reset hard ' + commit)

        def push(self, options):
            self.calls.append('push ' + ' '.join(options))

        def format_patch(self, options):
            self.calls.append('format patch ' + ' '.join(options))
            return 'formatted patch with changes done to LayoutTests/imported/w3c/web-platform-tests/test1.html'

        def delete_branch(self, branch_name):
            self.calls.append('delete branch ' + branch_name)

        def checkout_new_branch(self, branch_name):
            self.calls.append('checkout new branch ' + branch_name)

        def apply_mail_patch(self, options):
            # filtering options[0] as it changes for every run
            self.calls.append('apply_mail_patch patch.temp ' + ' '.join(options[1:]))

        def commit(self, options):
            self.calls.append('commit ' + ' '.join(options))

        def remote(self, options):
            self.calls.append('remote ' + ' '.join(options))
            return "my_remote_url"

        def local_config(self, name):
            return 'value'

        def branch_ref_exists(self, name):
            return False

        def create_patch(self, commit, arguments):
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
        host = TestExporterTest.MyMockHost()
        host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        exporter.do_export()
        self.assertEquals(exporter._github.calls, ['create_pr', 'add_label "webkit-export"'])
        self.assertTrue('WebKit export' in exporter._github.pull_requests_created[0][1])
        self.assertEquals(exporter._git.calls, [
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests',
            'fetch',
            'checkout master',
            'reset hard origin/master',
            'checkout new branch wpt-export-for-webkit-1234',
            'apply_mail_patch patch.temp ',
            'commit -a -m WebKit export of https://bugs.webkit.org/show_bug.cgi?id=1234',
            'remote ',
            'remote add USER https://USER@github.com/USER/wpt.git',
            'remote get-url USER',
            'push USER wpt-export-for-webkit-1234:wpt-export-for-webkit-1234 -f',
            'checkout master',
            'delete branch wpt-export-for-webkit-1234',
            'checkout master',
            'reset hard origin/master'])
        self.assertEquals(exporter._bugzilla.calls, [
            'fetch bug 1234',
            'Append https://github.com/web-platform-tests/wpt/pull/5678 to see also list',
            'post comment to bug 1234 : Submitted web-platform-tests pull request: https://github.com/web-platform-tests/wpt/pull/5678'])
        self.assertEquals(mock_linter.calls, ['/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests', 'lint'])

    def test_export_with_specific_branch(self):
        host = TestExporterTest.MyMockHost()
        host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN', '-bn', 'wpt-export-branch'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        exporter.do_export()
        self.assertEquals(exporter._git.calls, [
            '/mock-checkout/WebKitBuild/w3c-tests/web-platform-tests',
            'fetch',
            'checkout master',
            'reset hard origin/master',
            'checkout new branch wpt-export-for-webkit-1234',
            'apply_mail_patch patch.temp ',
            'commit -a -m WebKit export of https://bugs.webkit.org/show_bug.cgi?id=1234',
            'remote ',
            'remote add USER https://USER@github.com/USER/wpt.git',
            'remote get-url USER',
            'push USER wpt-export-for-webkit-1234:wpt-export-branch -f',
            'checkout master',
            'delete branch wpt-export-for-webkit-1234',
            'checkout master',
            'reset hard origin/master'])

    def test_export_interactive_mode(self):
        host = TestExporterTest.MyMockHost()
        host.web.responses.append({'status_code': 200, 'body': '{"login": "USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN', '--interactive'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        exporter.do_export()

    def test_export_invalid_token(self):
        host = TestExporterTest.MyMockHost()
        host.web.responses.append({'status_code': 401})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        with self.assertRaises(Exception) as context:
            exporter.do_export()
        self.assertIn('OAuth token is not valid', str(context.exception))

    def test_export_wrong_token(self):
        host = TestExporterTest.MyMockHost()
        host.web.responses.append({'status_code': 200, 'body': '{"login": "DIFF_USER"}'})
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        with self.assertRaises(Exception) as context:
            exporter.do_export()
        self.assertIn('OAuth token does not match the provided username', str(context.exception))

    def test_has_wpt_changes(self):
        host = TestExporterTest.MyMockHost()
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        self.assertTrue(exporter.has_wpt_changes())

    def test_has_no_wpt_changes_for_no_diff(self):
        host = TestExporterTest.MyMockHost()
        host._mockSCM.mock_format_patch_result = None
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_expected_file(self):
        host = TestExporterTest.MyMockHost()
        host._mockSCM.mock_format_patch_result = """
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/header-values-expected.txt

+change to expected
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        self.assertFalse(exporter.has_wpt_changes())

    def test_ignore_changes_to_w3c_import_log(self):
        host = TestExporterTest.MyMockHost()
        host._mockSCM.mock_format_patch_result = """
Subversion Revision: 231920
diff --git a/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log b/LayoutTests/imported/w3c/web-platform-tests/fetch/api/headers/w3c-import.log

+change to w3c import
"""
        options = parse_args(['test_exporter.py', '-g', 'HEAD', '-b', '1234', '-c', '-n', 'USER', '-t', 'TOKEN'])
        exporter = WebPlatformTestExporter(host, options, TestExporterTest.MockGit, TestExporterTest.MockBugzilla, MockWPTGitHub, TestExporterTest.MockWPTLinter)
        self.assertFalse(exporter.has_wpt_changes())
