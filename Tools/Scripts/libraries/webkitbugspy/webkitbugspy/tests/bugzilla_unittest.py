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

from webkitbugspy import Issue, User, bugzilla, mocks


class TestBugzilla(unittest.TestCase):
    URL = 'https://bugs.example.com'

    def test_no_users(self):
        with mocks.Bugzilla(self.URL.split('://')[1]):
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).user(
                    name='Tim Contributor', username='tcontributor@example.com', email='tcontributor@example.com',
                )), dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).user(username='tcontributor@example.com')),
                dict(username='tcontributor@example.com'),
            )
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).user(email='tcontributor@example.com')),
                dict(emails=['tcontributor@example.com']),
            )

    def test_users(self):
        with mocks.Bugzilla(self.URL.split('://')[1], users=mocks.USERS):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(
                User.Encoder().default(tracker.user(username='tcontributor@example.com')),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(tracker.user(name='Tim Contributor')),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(tracker.user(email='ffiler@example.com')),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )

    def test_link(self):
        tracker = bugzilla.Tracker(self.URL)
        self.assertEqual(tracker.issue(1234).link, 'https://bugs.example.com/show_bug.cgi?id=1234')
        self.assertEqual(
            tracker.from_string('http://bugs.example.com/show_bug.cgi?id=1234').link,
            'https://bugs.example.com/show_bug.cgi?id=1234',
        )
        self.assertEqual(tracker.from_string('http://bugs.other.com/show_bug.cgi?id=1234'), None)

    def test_title(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).title, 'Example issue 1')
            self.assertEqual(str(tracker.issue(1)), 'https://bugs.example.com/show_bug.cgi?id=1 Example issue 1')

    def test_timestamp(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(bugzilla.Tracker(self.URL).issue(1).timestamp, 1639510960)

    def test_creator(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).issue(1).creator),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )

    def test_description(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                bugzilla.Tracker(self.URL).issue(1).description,
                'An example issue for testing',
            )

    def test_assignee(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).issue(1).assignee),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )

    def test_comments(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            comments = bugzilla.Tracker(self.URL).issue(1).comments
            self.assertEqual(len(comments), 2)
            self.assertEqual(comments[0].timestamp, 1639511020)
            self.assertEqual(comments[0].content, 'Was able to reproduce on version 1.2.3')
            self.assertEqual(
                User.Encoder().default(comments[0].user),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )

    def test_watchers(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).issue(1).watchers), [
                    dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
                    dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
                    dict(name='Wilma Watcher', username='wwatcher@example.com', emails=['wwatcher@example.com']),
                ],
            )

    def test_references(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).references, [])
            self.assertEqual(tracker.issue(2).references, [tracker.issue(3)])
            self.assertEqual(tracker.issue(3).references, [tracker.issue(2)])

    def test_reference_parse(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES) as mock:
            tracker = bugzilla.Tracker(self.URL)
            mock.issues[1]['comments'].append(
                Issue.Comment(
                    user=mocks.USERS['Wilma Watcher'],
                    timestamp=1639539630,
                    content='Is this related to {}/show_bug.cgi?id=2?'.format(self.URL),
                ),
            )
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2)])
