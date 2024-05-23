# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging
import shutil
import sys

from webkitbugspy import github
from webkitcorepy import OutputCapture, testing, run
from webkitcorepy.mocks import Terminal as MockTerminal
from webkitscmpy import local, remote, mocks, program
from webkitscmpy.remote import BitBucket, GitHub


class TestReview(testing.PathTestCase):
    basepath = 'mock/repository'

    from webkitscmpy.test.pull_request_unittest import TestNetworkPullRequestBitBucket, TestNetworkPullRequestGitHub

    @classmethod
    def editor_callback(cls, will_print=True):
        def callback(path, will_print=will_print):
            if will_print:
                with open(path, 'r') as input_file:
                    for line in input_file.readlines():
                        sys.stdout.write(line)

        return callback

    def test_bitbucket(self):
        pr_id, remote_repo = program.Review.args_for_url('{}/pull-requests/1234/overview'.format(self.TestNetworkPullRequestBitBucket.remote))
        self.assertEqual(pr_id, '1234')
        self.assertEqual(self.TestNetworkPullRequestBitBucket.remote, remote_repo.url)

    def test_bitbucket_diff(self):
        pr_id, remote_repo = program.Review.args_for_url('{}/pull-requests/1234/diff#Tools%2FScripts%2Fexample.py'.format(self.TestNetworkPullRequestBitBucket.remote))
        self.assertEqual(pr_id, '1234')
        self.assertEqual(self.TestNetworkPullRequestBitBucket.remote, remote_repo.url)

    def test_github(self):
        pr_id, remote_repo = program.Review.args_for_url('{}/pull/1234'.format(self.TestNetworkPullRequestGitHub.remote))
        self.assertEqual(pr_id, '1234')
        self.assertEqual(self.TestNetworkPullRequestGitHub.remote, remote_repo.url)

    def test_github_files(self):
        pr_id, remote_repo = program.Review.args_for_url('{}/pull/1234/files#diff-abcdefabcdef'.format(self.TestNetworkPullRequestGitHub.remote))
        self.assertEqual(pr_id, '1234')
        self.assertEqual(self.TestNetworkPullRequestGitHub.remote, remote_repo.url)

    def test_invalid_pr_url(self):
        self.assertEqual(
            (None, None),
            program.Review.args_for_url('https://commits.webkit.org'),
        )

    def test_pr_argument(self):
        self.assertEqual('1234', program.Review.PR_RE.match('1234').group('number'))
        self.assertEqual('1234', program.Review.PR_RE.match('pr1234').group('number'))
        self.assertEqual('1234', program.Review.PR_RE.match('pr-1234').group('number'))
        self.assertEqual('1234', program.Review.PR_RE.match('pr 1234').group('number'))

        self.assertEqual(None, program.Review.PR_RE.match('pr  1234'))
        self.assertEqual(None, program.Review.PR_RE.match('deadbeef'))
        self.assertEqual(None, program.Review.PR_RE.match('1234@main'))
        self.assertEqual(None, program.Review.PR_RE.match('r1234'))

    def test_editor_no_repo(self):
        with mocks.local.Git(self.path):
            self.assertEqual([shutil.which('vim')], program.Review.editor(None))

    def test_editor_repo(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path, editor=lambda path: print(path)):
            self.assertEqual(['/bin/example', '-n', '-w'], program.Review.editor(local.Git(self.path)))
            self.assertEqual(run(['/bin/example', '-n', '-w', 'PATH']).returncode, 0)

        self.assertEqual(captured.stdout.getvalue(), 'PATH\n')

    def test_invoke_wizard(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(
            self.path, editor=self.editor_callback(),
        ):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened'
                    },
                    messages=['Example Change\nhttps://bugs.webkit.org/show_bug.cgi?id=1234\n\nReviewed by NOBODY (OOPS!).\n\n* Source/file.cpp:\n'],
                    comments=['Person <person@webkit.org>: Top-level comment'],
                    diff=[
                        '--- a/ChangeLog\n',
                        '+++ b/ChangeLog\n',
                        '@@ -1,0 +1,0 @@\n',
                        '+Example Change\n',
                        '+https://bugs.webkit.org/show_bug.cgi?id=1234\n',
                        '+\n',
                        '+Reviewed by NOBODY (OOPS!).\n',
                        '+* Source/file.cpp:\n',
                    ],
                ), dict(),
            )

        self.assertEqual(
            captured.stdout.getvalue(),
            'Title: Example Change\n'
            'Status: Opened\n'
            '================================================================================\n'
            '    Example Change\n'
            '    https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '\n'
            '    Reviewed by NOBODY (OOPS!).\n'
            '\n'
            '    * Source/file.cpp:\n'
            '================================================================================\n'
            'Person <person@webkit.org>: Top-level comment\n'
            '================================================================================\n'
            '--- a/ChangeLog\n'
            '+++ b/ChangeLog\n'
            '@@ -1,0 +1,0 @@\n'
            '+Example Change\n'
            '+https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '+\n'
            '+Reviewed by NOBODY (OOPS!).\n'
            '+* Source/file.cpp:\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_help(self):
        with OutputCapture(level=logging.INFO) as captured, mocks.local.Git(self.path):
            with self.assertRaises(SystemExit):
                program.main(
                    args=('review', '--help'),
                    path=self.path,
                )

        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_bitbucket_read(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(),
        ):
            result = program.main(
                args=('review', '1', '-n', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Title: Example Change\n'
            'Status: Opened\n'
            'Author: Tim Contributor <tcontributor@apple.com>\n'
            'Approved by: Eager Reviewer\n'
            'Blocked by: Suspicious Reviewer\n'
            '================================================================================\n'
            '    Example Change\n'
            '    https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '\n'
            '    Reviewed by NOBODY (OOPS!).\n'
            '\n'
            '    * Source/file.cpp:\n'
            '================================================================================\n'
            '--- a/ChangeLog\n'
            '+++ b/ChangeLog\n'
            '@@ -1,0 +1,0 @@\n'
            '+Example Change\n'
            '+https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '+\n'
            '+Reviewed by NOBODY (OOPS!).\n'
            '+* Source/file.cpp:\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_read(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(),
        ):
            github.Tracker('https://{}'.format(rmt.remote)).issue(1).set_labels(['bug', 'good first issue'])

            result = program.main(
                args=('review', '1', '-n', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Title: Example Change\n'
            'Status: Opened\n'
            'Author: tcontributor <?>\n'
            'Approved by: Eager Reviewer\n'
            'Blocked by: Suspicious Reviewer\n'
            'Labels: bug and good first issue\n'
            '================================================================================\n'
            '    Example Change\n'
            '    https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '\n'
            '    Reviewed by NOBODY (OOPS!).\n'
            '\n'
            '    * Source/file.cpp:\n'
            '================================================================================\n'
            'diff --git a/ChangeLog b/ChangeLog\n'
            '--- a/ChangeLog\n'
            '+++ b/ChangeLog\n'
            '@@ -1,0 +1,0 @@\n'
            '+Example Change\n'
            '+https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '+\n'
            '+Reviewed by NOBODY (OOPS!).\n'
            '+* Source/file.cpp:\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_bitbucket_read_comments(self):
        self.maxDiff = None
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(),
        ):
            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            pr.comment('High-level comment')
            pr.review(diff_comments=dict(ChangeLog={4: ['Inline comment']}))

            result = program.main(
                args=('review', '1', '-n', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Title: Example Change\n'
            'Status: Opened\n'
            'Author: Tim Contributor <tcontributor@apple.com>\n'
            'Approved by: Eager Reviewer\n'
            'Blocked by: Suspicious Reviewer\n'
            '================================================================================\n'
            '    Example Change\n'
            '    https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '\n'
            '    Reviewed by NOBODY (OOPS!).\n'
            '\n'
            '    * Source/file.cpp:\n'
            '================================================================================\n'
            'Tim Committer <committer@webkit.org>: High-level comment\n'
            '================================================================================\n'
            '--- a/ChangeLog\n'
            '+++ b/ChangeLog\n'
            '@@ -1,0 +1,0 @@\n'
            '+Example Change\n'
            '+https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '+\n'
            '+Reviewed by NOBODY (OOPS!).\n'
            '>>>>\n'
            'Tim Committer <committer@webkit.org>: Inline comment\n'
            '<<<<\n'
            '+* Source/file.cpp:\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_read_comments(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(),
        ):
            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            pr.comment('High-level comment')
            pr.review(diff_comments=dict(ChangeLog={4: ['Inline comment']}))

            result = program.main(
                args=('review', '1', '-n', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Title: Example Change\n'
            'Status: Opened\n'
            'Author: tcontributor <?>\n'
            'Approved by: Eager Reviewer\n'
            'Blocked by: Suspicious Reviewer\n'
            '================================================================================\n'
            '    Example Change\n'
            '    https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '\n'
            '    Reviewed by NOBODY (OOPS!).\n'
            '\n'
            '    * Source/file.cpp:\n'
            '================================================================================\n'
            'tcontributor <?>: High-level comment\n'
            '================================================================================\n'
            'diff --git a/ChangeLog b/ChangeLog\n'
            '--- a/ChangeLog\n'
            '+++ b/ChangeLog\n'
            '@@ -1,0 +1,0 @@\n'
            '+Example Change\n'
            '+https://bugs.webkit.org/show_bug.cgi?id=1234\n'
            '+\n'
            '+Reviewed by NOBODY (OOPS!).\n'
            '>>>>\n'
            'tcontributor <?>: Inline comment\n'
            '<<<<\n'
            '+* Source/file.cpp:\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])
