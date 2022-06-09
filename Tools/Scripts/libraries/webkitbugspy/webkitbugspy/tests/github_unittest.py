# Copyright (C) 2021 Apple Inc. All rights reserved.
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

import json
import unittest

from webkitbugspy import Issue, User, github, mocks


class TestGitHub(unittest.TestCase):
    URL = 'https://github.example.com/WebKit/WebKit'

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
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES) as mock:
            tracker = github.Tracker(self.URL)
            mock.issues[2]['comments'].append(
                Issue.Comment(
                    user=mocks.USERS['Tim Contributor'],
                    timestamp=1639539630,
                    content='Looks like @ffiler stumbled upon this in #3',
                ),
            )
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
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES) as mock:
            tracker = github.Tracker(self.URL)
            mock.issues[1]['comments'].append(
                Issue.Comment(
                    user=mocks.USERS['Wilma Watcher'],
                    timestamp=1639539630,
                    content='Is this related to #2?',
                ),
            )
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2)])
