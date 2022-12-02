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
from webkitcorepy import OutputCapture, mocks as wkmocks


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

    def test_labels(self):
        with mocks.GitHub(self.URL.split('://')[1]) as mocked:
            self.assertDictEqual(github.Tracker(self.URL).labels, mocked.DEFAULT_LABELS)

    def test_projects(self):
        with mocks.GitHub(self.URL.split('://')[1], projects=mocks.PROJECTS):
            self.assertDictEqual(
                dict(
                    WebKit=dict(
                        versions=['All', 'Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                        components=dict(
                            IPv4=dict(
                                description='Bugs involving IPv4 networking',
                                color='FFFFFF',
                            ), IPv6=dict(
                                description='Bugs involving IPv6 networking',
                                color='FFFFFF',
                            ), Scrolling=dict(
                                description='Bugs related to main thread and off-main thread scrolling',
                                color='FFFFFF',
                            ), SVG=dict(
                                description='For bugs in the SVG implementation.',
                                color='FFFFFF',
                            ), Tables=dict(
                                description='For bugs specific to tables (both the DOM and rendering issues).',
                                color='FFFFFF',
                            ), Text=dict(
                                description='For bugs in text layout and rendering, including international text support.',
                                color='FFFFFF',
                            ),
                        ),
                    ),
                ), github.Tracker(self.URL).projects,
            )

    def test_create(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            created = github.Tracker(self.URL).create('New bug', 'Creating new bug')
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )

    def test_create_projects(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )), wkmocks.Terminal.input('3', '2'), OutputCapture() as captured:
            created = github.Tracker(self.URL).create('New bug', 'Creating new bug')
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username='tcontributor', emails=['tcontributor@example.com']),
            )

            self.assertEqual(created.project, 'WebKit')
            self.assertEqual(created.component, 'SVG')

        self.assertEqual(
            captured.stdout.getvalue(),
            '''What component in 'WebKit' should the bug be associated with?:
    1) IPv4
    2) IPv6
    3) SVG
    4) Scrolling
    5) Tables
    6) Text
: 
''',
        )

    def test_get_component(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Text')
            self.assertEqual(issue.version, 'Other')

    def test_set_component(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS, environment=wkmocks.Environment(
            GITHUB_EXAMPLE_COM_USERNAME='tcontributor',
            GITHUB_EXAMPLE_COM_TOKEN='token',
        )):
            github.Tracker(self.URL).issue(1).set_component(component='Tables', version='Safari 15')

            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Tables')
            self.assertEqual(issue.version, 'Safari 15')

    def test_issue_label(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = github.Tracker(self.URL).issue(1)
            self.assertEqual(issue.labels, ['Other', 'Text'])

    def test_redaction(self):
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            self.assertEqual(github.Tracker(self.URL, redact=None).issue(1).redacted, False)
            self.assertTrue(bool(github.Tracker(self.URL, redact={'.*': True}).issue(1).redacted))
            self.assertEqual(
                github.Tracker(self.URL, redact={'.*': True}).issue(1).redacted,
                github.Tracker.Redaction(True, 'is a GitHub Issue'),
            )
            self.assertEqual(
                github.Tracker(self.URL, redact={'project:WebKit': True}).issue(1).redacted,
                github.Tracker.Redaction(True, "matches 'project:WebKit'"),
            )
            self.assertEqual(
                github.Tracker(self.URL, redact={'component:Text': True}).issue(1).redacted,
                github.Tracker.Redaction(True, "matches 'component:Text'"),
            )
            self.assertEqual(
                github.Tracker(self.URL, redact={'version:Other': True}).issue(1).redacted,
                github.Tracker.Redaction(True, "matches 'version:Other'"),
            )

    def test_parse_error(self):
        error_json = {'message': 'Validation Failed', 'errors': [{'resource': 'Issue', 'code': 'custom', 'field': 'body', 'message': 'body is too long (maximum is 65536 characters)'}], 'documentation_url': 'https://docs.github.com/rest/reference/pulls#create-a-pull-request'}
        with mocks.GitHub(self.URL.split('://')[1], issues=mocks.ISSUES):
            parsed_error = '''Error Message: Validation Failed
---\tERROR\t---
Type: body is too long (maximum is 65536 characters)
Resource: Issue
Field: body
---\t---\t---
Documentation URL: https://docs.github.com/rest/reference/pulls#create-a-pull-request
'''
            self.assertEqual(github.Tracker(self.URL).parse_error(error_json), parsed_error)
