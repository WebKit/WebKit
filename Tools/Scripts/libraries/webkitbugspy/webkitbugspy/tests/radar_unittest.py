# Copyright (C) 2022 Apple Inc. All rights reserved.
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

from webkitbugspy import Issue, Tracker, User, radar, mocks
from webkitcorepy import mocks as wkmocks


class TestRadar(unittest.TestCase):
    def test_encoding(self):
        self.assertEqual(
            radar.Tracker.Encoder().default(radar.Tracker()),
            dict(type='radar'),
        )

    def test_decoding(self):
        decoded = Tracker.from_json(json.dumps(radar.Tracker(), cls=Tracker.Encoder))
        self.assertIsInstance(decoded, radar.Tracker)
        self.assertEqual(decoded.from_string('rdar://1234').id, 1234)

    def test_no_radar(self):
        with mocks.NoRadar():
            tracker = radar.Tracker()
            self.assertIsNone(tracker.library)
            self.assertIsNone(tracker.client)

    def test_users(self):
        with mocks.Radar(users=mocks.USERS):
            tracker = radar.Tracker()
            self.assertEqual(
                User.Encoder().default(tracker.user(username=504)),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(tracker.user(email='tcontributor@example.com')),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(tracker.user(name='Felix Filer')),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )

    def test_link(self):
        with mocks.Radar(users=mocks.USERS):
            tracker = radar.Tracker()
            self.assertEqual(tracker.issue(1234).link, '<rdar://1234>')
            self.assertEqual(
                tracker.from_string('<rdar://problem/1234>').link,
                '<rdar://1234>',
            )
            self.assertEqual(
                tracker.from_string('<radar://1234>').link,
                '<rdar://1234>',
            )
            self.assertEqual(
                tracker.from_string('<radar://problem/1234>').link,
                '<rdar://1234>',
            )

    def test_title(self):
        with mocks.Radar(issues=mocks.ISSUES):
            tracker = radar.Tracker()
            self.assertEqual(tracker.issue(1).title, 'Example issue 1')
            self.assertEqual(str(tracker.issue(1)), '<rdar://1> Example issue 1')

    def test_timestamp(self):
        with mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(radar.Tracker().issue(1).timestamp, 1639510960)

    def test_creator(self):
        with mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(radar.Tracker().issue(1).creator),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )

    def test_description(self):
        with mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(
                radar.Tracker().issue(1).description,
                'An example issue for testing',
            )

    def test_assignee(self):
        with mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(radar.Tracker().issue(1).assignee),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )

    def test_comments(self):
        with mocks.Radar(issues=mocks.ISSUES):
            comments = radar.Tracker().issue(1).comments
            self.assertEqual(len(comments), 2)
            self.assertEqual(comments[0].timestamp, 1639511020)
            self.assertEqual(comments[0].content, 'Was able to reproduce on version 1.2.3')
            self.assertEqual(
                User.Encoder().default(comments[0].user),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )

    def test_watchers(self):
        with mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(radar.Tracker().issue(1).watchers), [
                    dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
                    dict(name='Wilma Watcher', username=46, emails=['wwatcher@example.com']),
                ],
            )

    def test_references(self):
        with mocks.Radar(issues=mocks.ISSUES):
            tracker = radar.Tracker()
            self.assertEqual(tracker.issue(1).references, [])
            self.assertEqual(tracker.issue(2).references, [tracker.issue(3)])
            self.assertEqual(tracker.issue(3).references, [tracker.issue(2)])

    def test_reference_parse(self):
        with wkmocks.Environment(RADAR_USERNAME='wwatcher'), mocks.Radar(issues=mocks.ISSUES) as mock:
            tracker = radar.Tracker()
            tracker.issue(1).add_comment('Is this related to <rdar://2> ?')
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2)])

    def test_me(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES):
            self.assertEqual(
                User.Encoder().default(radar.Tracker().me()),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )

    def test_add_comment(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertEqual(len(issue.comments), 2)

            comment = issue.add_comment('Automated comment')
            self.assertEqual(comment.content, 'Automated comment')
            self.assertEqual(
                User.Encoder().default(comment.user),
                User.Encoder().default(radar.Tracker().me()),
            )

            self.assertEqual(len(issue.comments), 3)
            self.assertEqual(len(radar.Tracker().issue(1).comments), 3)
