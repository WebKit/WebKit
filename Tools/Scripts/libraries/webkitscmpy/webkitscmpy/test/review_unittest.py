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
    def editor_callback(cls, will_print=True, replace=None, insert=None):
        def callback(path, will_print=will_print, replace=replace, insert=insert):
            replace = replace or dict()
            insert = insert or dict()

            with open(path, 'r') as input_file:
                lines = input_file.readlines()
            with open(path, 'w') as output_file:
                for pos in range(len(lines)):
                    if will_print:
                        sys.stdout.write(lines[pos])
                    if pos in replace:
                        output_file.write(replace[pos])
                    else:
                        output_file.write(lines[pos])
                    if pos in insert:
                        output_file.write(insert[pos])

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
            self.assertEqual(['/bin/Example Program', '-n', '-w'], program.Review.editor(local.Git(self.path)))
            self.assertEqual(run(['/bin/Example Program', '-n', '-w', 'PATH']).returncode, 0)

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

    def test_truncate_strs(self):
        self.assertEqual(
            program.Review.truncate_strs(dict(keya='value a ', keyb=' value b')),
            dict(keya=['value a'], keyb=[' value b']),
        )
        self.assertEqual(
            program.Review.truncate_strs(dict(key='value ')),
            dict(key=['value']),
        )
        self.assertEqual(
            program.Review.truncate_strs(dict(nested=dict(key='value '))),
            dict(nested=dict(key=['value'])),
        )

    def test_user_delta_github(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt:
            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)

            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer'),
                (False, [], [], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, ''),
                (False, [], [pr.generator.repository.contributors.get('Eager Reviewer')], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer, Suspicious Reviewer'),
                (False, [pr.generator.repository.contributors.get('Suspicious Reviewer')], [], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer, tcontributor'),
                (True, [], [], 0),
            )

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_user_delta_bitbucket(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt:
            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)

            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer'),
                (False, [], [], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, ''),
                (False, [], [pr.generator.repository.contributors.get('Eager Reviewer')], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer, Suspicious Reviewer'),
                (False, [pr.generator.repository.contributors.get('Suspicious Reviewer')], [], 0),
            )
            self.assertEqual(
                program.Review.user_delta(pr, pr.approvers, 'Eager Reviewer, tcontributor'),
                (True, [], [], 0),
            )

        self.assertEqual(captured.stdout.getvalue(), '')
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_edit_metadata(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            replace={0: 'Title: Replacement title\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(metadata=dict(Title='Replacement title')),
            )

    def test_comment_commit_message(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={6: "We need a reviewer!\n"},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(comments=['> Reviewed by NOBODY (OOPS!).\n\nWe need a reviewer!']),
            )

    def test_comment(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={9: 'Not sure about this change\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(comments=['Not sure about this change']),
            )

    def test_comment_reply(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={10: 'Response to top-level comment\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(comments=[('Person <person@webkit.org>: Top-level comment', '> Top-level comment\nResponse to top-level comment')]),
            )

    def test_comment_reply_only_one(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={12: 'Response to top-level comment\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
                    },
                    messages=['Example Change\nhttps://bugs.webkit.org/show_bug.cgi?id=1234\n\nReviewed by NOBODY (OOPS!).\n\n* Source/file.cpp:\n'],
                    comments=[
                        'Person <person@webkit.org>: Top-level comment 1',
                        'Person <person@webkit.org>: Top-level comment 2',
                    ], diff=[
                        '--- a/ChangeLog\n',
                        '+++ b/ChangeLog\n',
                        '@@ -1,0 +1,0 @@\n',
                        '+Example Change\n',
                        '+https://bugs.webkit.org/show_bug.cgi?id=1234\n',
                        '+\n',
                        '+Reviewed by NOBODY (OOPS!).\n',
                        '+* Source/file.cpp:\n',
                    ],
                ), dict(comments=[('Person <person@webkit.org>: Top-level comment 2', '> Top-level comment 2\nResponse to top-level comment')]),
            )

    def test_comment_reply_both(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={
                10: 'Response to top-level comment 1\n',
                12: 'Response to top-level comment 2\n',
            }, will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
                    },
                    messages=['Example Change\nhttps://bugs.webkit.org/show_bug.cgi?id=1234\n\nReviewed by NOBODY (OOPS!).\n\n* Source/file.cpp:\n'],
                    comments=[
                        'Person <person@webkit.org>: Top-level comment 1',
                        'Person <person@webkit.org>: Top-level comment 2',
                    ], diff=[
                        '--- a/ChangeLog\n',
                        '+++ b/ChangeLog\n',
                        '@@ -1,0 +1,0 @@\n',
                        '+Example Change\n',
                        '+https://bugs.webkit.org/show_bug.cgi?id=1234\n',
                        '+\n',
                        '+Reviewed by NOBODY (OOPS!).\n',
                        '+* Source/file.cpp:\n',
                    ],
                ), dict(comments=[
                    ('Person <person@webkit.org>: Top-level comment 1', '> Top-level comment 1\nResponse to top-level comment 1'),
                    ('Person <person@webkit.org>: Top-level comment 2', '> Top-level comment 2\nResponse to top-level comment 2'),
                ]),
            )

    def test_diff_file_comment(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={12: 'ChangeLogs are deprecated\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(diff=dict(diff_comments=dict(ChangeLog={0: ['ChangeLogs are deprecated']}))),
            )

    def test_diff_inline_comment(self):
        with mocks.local.Git(self.path, editor=self.editor_callback(
            insert={16: 'Is this the correct bug?\n'},
            will_print=False,
        )):
            self.assertEqual(
                program.Review.invoke_wizard(
                    editor=program.Review.editor(local.Git(self.path)),
                    name='pr-1234',
                    header={
                        'Title': 'Example Change',
                        'Status': 'Opened',
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
                ), dict(diff=dict(diff_comments=dict(ChangeLog={2: ['Is this the correct bug?']}))),
            )

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

    def test_bitbucket_no_edit(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(will_print=False),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n',
        )
        self.assertEqual(captured.stderr.getvalue(), 'No pull-request edits detected\n')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_no_edit(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(will_print=False),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n',
        )
        self.assertEqual(captured.stderr.getvalue(), 'No pull-request edits detected\n')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_bitbucket_approve(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver(user='rreviewer') as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(will_print=False),
        ), MockTerminal.input('Approve'):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer', 'rreviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Would you like to approve this change? (Approve/Comment/Request changes): \n'
            '1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_github_deny(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver(user='rreviewer') as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(will_print=False),
        ), MockTerminal.input('Request changes'):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer', 'Reluctant Reviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n'
            'Would you like to approve this change? (Approve/Comment/Request changes): \n'
            '1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_bitbucket_edit_deny(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver(user='rreviewer') as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={4: 'Blocked by: Suspicious Reviewer, me\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer', 'rreviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_github_edit_approve(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver(user='rreviewer') as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={3: 'Approved by: Eager Reviewer, rreviewer\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer', 'Reluctant Reviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_bitbucket_edit_refresh(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={4: 'Blocked by:\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), 'Failed to request Suspicious Reviewer as reviewer\n')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_edit_refresh(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={3: 'Approved by:\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([p.name for p in pr.approvers], ['Eager Reviewer'])
            self.assertEqual([p.name for p in pr.blockers], ['Suspicious Reviewer'])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), 'Failed to request Eager Reviewer as reviewer\n')
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_bitbucket_edit_comment(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={5: 'Not sure about this change\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(['Not sure about this change'], [c.content for c in pr.comments])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Making top-level comment on pull-request...'])

    def test_github_edit_comment(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={5: 'Not sure about this change\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(['Not sure about this change'], [c.content for c in pr.comments])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Making top-level comment on pull-request...'])

    def test_bitbucket_edit_comment_commit_message(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={9: 'We need a reviewer!\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(['> Reviewed by NOBODY (OOPS!).\n\nWe need a reviewer!'], [c.content for c in pr.comments])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Making top-level comment on pull-request...'])

    def test_github_edit_comment_commit_message(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={9: 'We need a reviewer!\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(['> Reviewed by NOBODY (OOPS!).\n\nWe need a reviewer!'], [c.content for c in pr.comments])

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Making top-level comment on pull-request...'])

    def test_bitbucket_edit_comment_inline_diff(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={19: 'We need a reviewer!\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+Example Change',
                '+https://bugs.webkit.org/show_bug.cgi?id=1234',
                '+',
                '+Reviewed by NOBODY (OOPS!).',
                '>>>>',
                'Tim Committer <committer@webkit.org>: We need a reviewer!',
                '<<<<',
                '+* Source/file.cpp:',
            ], list(pr.diff(comments=True)))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_github_edit_comment_inline_diff(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={20: 'We need a reviewer!\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual([
                'diff --git a/ChangeLog b/ChangeLog',
                '--- a/ChangeLog',
                '+++ b/ChangeLog',
                '@@ -1,0 +1,0 @@',
                '+Example Change',
                '+https://bugs.webkit.org/show_bug.cgi?id=1234',
                '+',
                '+Reviewed by NOBODY (OOPS!).',
                '>>>>',
                'tcontributor <?>: We need a reviewer!',
                '<<<<',
                '+* Source/file.cpp:',
            ], list(pr.diff(comments=True)))

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Posting review on pull-request...'])

    def test_bitbucket_edit_title(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={0: 'Title: New Title\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(pr.title, 'New Title')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Setting title of pull-request...'])

    def test_github_edit_title(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={0: 'Title: New Title\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertEqual(pr.title, 'New Title')

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Setting title of pull-request...'])

    def test_bitbucket_edit_open(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={1: 'Status: Open\n'},
                will_print=False,
            ),
        ):
            remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1).close()

            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertTrue(pr.opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Opening pull-request...'])

    def test_github_edit_close(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={1: 'Status: Close\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertFalse(pr.opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Closing pull-request...'])

    def test_bitbucket_edit_merged_close(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={1: 'Status: Close\n'},
                will_print=False,
            ),
        ):
            rmt.pull_requests[0]['state'] = 'MERGED'
            rmt.pull_requests[0]['open'] = False
            rmt.pull_requests[0]['closed'] = True

            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

            pr = remote.BitBucket('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertFalse(pr.opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), "Cannot change pull-request status to 'Close'\n")
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_edit_merged_open(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={1: 'Status: Open\n'},
                will_print=False,
            ),
        ):
            rmt.pull_requests[0]['state'] = 'closed'
            rmt.pull_requests[0]['merged'] = True

            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

            pr = remote.GitHub('https://{}'.format(rmt.remote)).pull_requests.get(1)
            self.assertFalse(pr.opened)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), "Cannot change pull-request status to 'Open'\n")
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_bitbucket_add_labels(self):
        self.maxDiff = None
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestBitBucket.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                insert={4: 'Labels: example\n'},
                will_print=False,
            ),
        ):
            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), "Cannot modify the 'Labels' key on this pull-request\n")
        self.assertEqual(captured.root.log.getvalue().splitlines(), [])

    def test_github_edit_labels(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={5: 'Labels: bug and wontfix\n'},
                will_print=False,
            ),
        ):
            github.Tracker('https://{}'.format(rmt.remote)).issue(1).set_labels(['bug', 'good first issue'])

            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(0, result)

            self.assertEqual(['bug', 'wontfix'], github.Tracker('https://{}'.format(rmt.remote)).issue(1).labels)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n1 change made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), '')
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Setting labels on pull-request...'])

    def test_github_edit_invalid_labels(self):
        with OutputCapture(level=logging.INFO) as captured, self.TestNetworkPullRequestGitHub.webserver() as rmt, mocks.local.Git(
            self.path, remote='https://{}'.format(rmt.remote),
            editor=self.editor_callback(
                replace={5: 'Labels: invalid-label\n'},
                will_print=False,
            ),
        ):
            github.Tracker('https://{}'.format(rmt.remote)).issue(1).set_labels(['bug', 'good first issue'])

            result = program.main(
                args=('review', '1', '-v'),
                path=self.path,
            )
            self.assertEqual(1, result)

            self.assertEqual(['bug', 'good first issue'], github.Tracker('https://{}'.format(rmt.remote)).issue(1).labels)

        self.assertEqual(
            captured.stdout.getvalue(),
            'Waiting for user to modify textual pull-request...\n0 changes made\n',
        )
        self.assertEqual(captured.stderr.getvalue(), "'invalid-label' is not a label for 'https://github.example.com/WebKit/WebKit'\n")
        self.assertEqual(captured.root.log.getvalue().splitlines(), ['Setting labels on pull-request...'])
