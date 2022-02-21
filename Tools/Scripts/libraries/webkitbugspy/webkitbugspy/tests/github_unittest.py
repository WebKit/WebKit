# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

import re
import json
import unittest

from webkitbugspy import Issue, Tracker, User, github, mocks
from webkitcorepy import mocks as wkmocks


class TestGitHub(unittest.TestCase):
    URL = 'https://github.example.com/WebKit/WebKit'

    def test_encoding(self):
        self.assertEqual(
            github.Tracker.Encoder().default(github.Tracker(
                self.URL,
                res=[re.compile(r'\Aexample.com/b/(?P<id>\d+)\Z')],
            )), dict(
                type='github',
                url='https://github.example.com/WebKit/WebKit',
                res=['\\Aexample.com/b/(?P<id>\\d+)\\Z']
            ),
        )

    def test_decoding(self):
        decoded = Tracker.from_json(json.dumps(github.Tracker(
            self.URL,
            res=[re.compile(r'\Aexample.com/b/(?P<id>\d+)\Z')],
        ), cls=Tracker.Encoder))
        self.assertIsInstance(decoded, github.Tracker)
        self.assertEqual(decoded.url, 'https://github.example.com/WebKit/WebKit')
        self.assertEqual(decoded.from_string('example.com/b/1234').id, 1234)

    def test_users(self):
        with mocks.GitHub(self.URL.split('://')[1], users=mocks.USERS):
            tracker = github.Tracker(self.URL)
            self.assertEqual(
                User.Encoder().default(tracker.user(username='tcontributor')),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(tracker.user(name='Tim Contributor')),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            with self.assertRaises(RuntimeError):
                tracker.user(email='ffiler@example.com')

    def test_link(self):
        tracker = github.Tracker(self.URL)
        self.assertEqual(tracker.issue(1234).link, 'https://github.example.com/WebKit/WebKit/issues/1234')
        self.assertEqual(
            tracker.from_string('http://github.example.com/WebKit/WebKit/issues/1234').link,
            'https://github.example.com/WebKit/WebKit/issues/1234',
        )
        self.assertEqual(
            tracker.from_string('http://api.github.example.com/repos/WebKit/WebKit/issues/1234').link,
            'https://github.example.com/WebKit/WebKit/issues/1234',
        )
        self.assertEqual(tracker.from_string('https://github.example.com/Apple/Swift/issues/1234'), None)

    def test_title(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = github.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).title, 'Example issue 1')
            self.assertEqual(str(tracker.issue(1)), 'https://github.example.com/WebKit/WebKit/issues/1 Example issue 1')

    def test_timestamp(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(github.Tracker(self.URL).issue(1).timestamp, 1639510960)

    def test_creator(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(github.Tracker(self.URL).issue(1).creator),
                dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
            )

    def test_description(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                github.Tracker(self.URL).issue(1).description,
                'An example issue for testing',
            )

    def test_assignee(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(github.Tracker(self.URL).issue(1).assignee),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )

    def test_comments(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            comments = github.Tracker(self.URL).issue(1).comments
            self.assertEqual(len(comments), 2)
            self.assertEqual(comments[0].timestamp, 1639511020)
            self.assertEqual(comments[0].content, 'Was able to reproduce on version 1.2.3')
            self.assertEqual(
                User.Encoder().default(comments[0].user),
                dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
            )

    def test_watchers(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(github.Tracker(self.URL).issue(1).watchers), [
                    dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
                    dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
                    dict(name='Wilma Watcher', username='wwatcher', emails=['wwatcher@example.com']),
                ],
            )

    def test_watcher_parse(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
                GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
                GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            tracker = github.Tracker(self.URL)
            tracker.issue(2).add_comment('Looks like @ffiler stumbled upon this in #3')
            self.assertEqual(
                User.Encoder().default(tracker.issue(2).watchers), [
                    dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
                    dict(name='Wilma Watcher', username='wwatcher', emails=['wwatcher@example.com']),
                    dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
                ],
            )

    def test_references(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = github.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).references, [])
            self.assertEqual(tracker.issue(2).references, [tracker.issue(3)])
            self.assertEqual(tracker.issue(3).references, [tracker.issue(2)])

    def test_reference_parse(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='wwatcher',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            tracker = github.Tracker(self.URL)
            tracker.issue(1).add_comment('Is this related to #2?')
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2)])

    def test_me(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            self.assertEqual(
                User.Encoder().default(github.Tracker(self.URL).me()),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )

    def test_add_comment(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(len(issue.comments), 2)

            comment = issue.add_comment('Automated comment')
            self.assertEqual(comment.content, 'Automated comment')
            self.assertEqual(
                User.Encoder().default(comment.user),
                User.Encoder().default(github.Tracker(self.URL).me()),
            )

            self.assertEqual(len(issue.comments), 3)
            self.assertEqual(len(github.Tracker(self.URL).issue(1).comments), 3)

    def test_assign(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='ffiler',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            issue.assign(github.Tracker(self.URL).me())
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
            )

            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
            )

    def test_assign_why(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='ffiler',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            issue.assign(github.Tracker(self.URL).me(), why='Let me provide a better reproduction')
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler', emails=['ffiler@example.com']),
            )
            self.assertEqual(issue.comments[-1].content, 'Let me provide a better reproduction')

    def test_state(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            issue = github.Tracker(self.URL).issue(1)
            self.assertTrue(issue.opened)
            self.assertFalse(issue.open())
            self.assertTrue(issue.close())
            self.assertFalse(issue.opened)

            issue = github.Tracker(self.URL).issue(1)
            self.assertFalse(issue.opened)
            self.assertFalse(issue.close())
            self.assertTrue(issue.open())
            self.assertTrue(issue.opened)

    def test_state_why(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            issue = github.Tracker(self.URL).issue(1)
            self.assertTrue(issue.opened)
            self.assertTrue(issue.close(why='Fixed in 1234@main'))
            self.assertFalse(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Fixed in 1234@main')

            issue = github.Tracker(self.URL).issue(1)
            self.assertFalse(issue.opened)
            self.assertTrue(issue.open(why='Need to revert, fix broke the build'))
            self.assertTrue(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Need to revert, fix broke the build')
