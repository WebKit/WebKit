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
from webkitcorepy import mocks as wkmocks, OutputCapture


class TestRadar(unittest.TestCase):
    def test_encoding(self):
        self.assertEqual(
            radar.Tracker.Encoder().default(radar.Tracker(project='WebKit')),
            dict(type='radar', projects=['WebKit']),
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
            self.assertEqual(
                User.Encoder().default(tracker.user(name='Olivia Outsider', email='ooutsider@example.com')),
                dict(name='Olivia Outsider', emails=['ooutsider@example.com']),
            )

    def test_link(self):
        with mocks.Radar(users=mocks.USERS):
            tracker = radar.Tracker()
            self.assertEqual(tracker.issue(1234).link, 'rdar://1234')
            self.assertEqual(
                tracker.from_string('<rdar://problem/1234>').link,
                'rdar://1234',
            )
            self.assertEqual(
                tracker.from_string('<radar://1234>').link,
                'rdar://1234',
            )
            self.assertEqual(
                tracker.from_string('<radar://problem/1234>').link,
                'rdar://1234',
            )

    def test_title(self):
        with mocks.Radar(issues=mocks.ISSUES):
            tracker = radar.Tracker()
            self.assertEqual(tracker.issue(1).title, 'Example issue 1')
            self.assertEqual(str(tracker.issue(1)), 'rdar://1 Example issue 1')

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

    def test_assign(self):
        with wkmocks.Environment(RADAR_USERNAME='ffiler'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            issue.assign(radar.Tracker().me())
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )

            issue = radar.Tracker().issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )

    def test_assign_why(self):
        with wkmocks.Environment(RADAR_USERNAME='ffiler'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            issue.assign(radar.Tracker().me(), why='Let me provide a better reproduction')
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username=809, emails=['ffiler@example.com']),
            )
            self.assertEqual(issue.comments[-1].content, 'Let me provide a better reproduction')

    def test_state(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertTrue(issue.opened)
            self.assertFalse(issue.open())
            self.assertTrue(issue.close())
            self.assertFalse(issue.opened)

            issue = radar.Tracker().issue(1)
            self.assertFalse(issue.opened)
            self.assertFalse(issue.close())
            self.assertTrue(issue.open())
            self.assertTrue(issue.opened)

    def test_state_why(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertTrue(issue.opened)
            self.assertTrue(issue.close(why='Fixed in 1234@main'))
            self.assertFalse(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Fixed in 1234@main')

            issue = radar.Tracker().issue(1)
            self.assertFalse(issue.opened)
            self.assertTrue(issue.open(why='Need to revert, fix broke the build'))
            self.assertTrue(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Need to revert, fix broke the build')

    def test_projects(self):
        with mocks.Radar(projects=mocks.PROJECTS):
            self.assertDictEqual(
                dict(
                    WebKit=dict(
                        description=None,
                        versions=[],
                        components=dict(
                            Scrolling=dict(
                                versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                                description='Bugs related to main thread and off-main thread scrolling',
                            ), SVG=dict(
                                versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                                description='For bugs in the SVG implementation.',
                            ), Tables=dict(
                                versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                                description='For bugs specific to tables (both the DOM and rendering issues).',
                            ), Text=dict(
                                versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                                description='For bugs in text layout and rendering, including international text support.',
                            ),
                        ),
                    ),
                ), radar.Tracker(project='WebKit').projects,
            )

    def test_create(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES, projects=mocks.PROJECTS):
            created = radar.Tracker(projects=['CFNetwork', 'WebKit']).create(
                'New bug', 'Creating new bug',
                project='WebKit', component='Tables', version='Other',
            )
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )

            self.assertEqual(created.project, 'WebKit')
            self.assertEqual(created.component, 'Tables')
            self.assertEqual(created.version, 'Other')

    def test_create_prompt(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES, projects=mocks.PROJECTS), \
            wkmocks.Terminal.input('2', '4', '4'), OutputCapture() as captured:

            created = radar.Tracker(projects=['CFNetwork', 'WebKit']).create('New bug', 'Creating new bug')
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username=504, emails=['tcontributor@example.com']),
            )

            self.assertEqual(created.project, 'WebKit')
            self.assertEqual(created.component, 'Text')
            self.assertEqual(created.version, 'WebKit Local Build')

        self.assertEqual(
            captured.stdout.getvalue(),
            '''What project should the bug be associated with?:
    1) CFNetwork
    2) WebKit
: 
What component in 'WebKit' should the bug be associated with?:
    1) SVG
    2) Scrolling
    3) Tables
    4) Text
: 
What version of 'WebKit Text' should the bug be associated with?:
    1) Other
    2) Safari 15
    3) Safari Technology Preview
    4) WebKit Local Build
: 
''',
        )

    def test_get_component(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = radar.Tracker(project='WebKit').issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Text')
            self.assertEqual(issue.version, 'Other')

    def test_set_component(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES, projects=mocks.PROJECTS):
            radar.Tracker(project='WebKit').issue(1).set_component(component='Tables', version='Safari 15')

            issue = radar.Tracker(project='WebKit').issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Tables')
            self.assertEqual(issue.version, 'Safari 15')

    def test_labels(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES):
            issue = radar.Tracker().issue(1)
            self.assertEqual(issue.labels, [])

    def test_redaction(self):
        with wkmocks.Environment(RADAR_USERNAME='tcontributor'), mocks.Radar(issues=mocks.ISSUES, projects=mocks.PROJECTS):
            self.assertEqual(radar.Tracker(
                project='WebKit',
                redact=None,
            ).issue(1).redacted, False)

            self.assertEqual(radar.Tracker(
                project='WebKit',
                redact={'.*': True},
            ).issue(1).redacted, True)

            self.assertEqual(radar.Tracker(
                project='WebKit',
                redact={'project:WebKit': True},
            ).issue(1).redacted, True)

            self.assertEqual(radar.Tracker(
                project='WebKit',
                redact={'component:Text': True},
            ).issue(1).redacted, True)

            self.assertEqual(radar.Tracker(
                project='WebKit',
                redact={'version:Other': True},
            ).issue(1).redacted, True)
