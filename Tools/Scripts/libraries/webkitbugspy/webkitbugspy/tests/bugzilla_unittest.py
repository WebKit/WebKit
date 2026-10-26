# Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

import copy
import json
import logging
import re
import time
import unittest
from contextlib import contextmanager
from unittest.mock import patch

from webkitcorepy import OutputCapture
from webkitcorepy import mocks as wkmocks

from webkitbugspy import Issue, Tracker, User, bugzilla, mocks, radar


ATTACHMENT_ISSUES = [
    dict(
        id=1,
        title='Issue with attachments',
        timestamp=1639536160,
        modified=1710884407,
        opened=True,
        creator=mocks.USERS['Felix Filer'],
        assignee=mocks.USERS['Tim Contributor'],
        description='An example issue with attachments',
        attachments=[
            dict(fileName='fix.patch', data=b'FIX PATCH BYTES', content_type='text/plain', is_patch=True),
            dict(fileName='notes.txt', data=b'not a patch', content_type='text/plain'),
            dict(fileName='old.patch', data=b'OBSOLETE', content_type='text/plain', is_patch=True, is_obsolete=True),
        ],
    ), dict(
        id=2,
        title='Issue without attachments',
        timestamp=1639540010,
        modified=1710884407,
        opened=True,
        creator=mocks.USERS['Felix Filer'],
        assignee=mocks.USERS['Tim Contributor'],
        description='An example issue without attachments',
    ),
]


def error_response(url, status_code, code, message):
    return wkmocks.Response(url=url, headers={'Content-Type': 'text/json'}, status_code=status_code, text=json.dumps(dict(
        code=code,
        error=True,
        message=message,
    )))


class TestBugzilla(unittest.TestCase):
    URL = 'https://bugs.example.com'

    def test_encoding(self):
        self.assertEqual(
            bugzilla.Tracker.Encoder().default(bugzilla.Tracker(
                self.URL,
                res=[re.compile(r'\Aexample.com/b/(?P<id>\d+)\Z')],
            )), dict(
                type='bugzilla',
                url='https://bugs.example.com',
                hide_title=False,
                res=['\\Aexample.com/b/(?P<id>\\d+)\\Z']
            ),
        )

    def test_decoding(self):
        decoded = Tracker.from_json(json.dumps(bugzilla.Tracker(
            self.URL,
            res=[re.compile(r'\Aexample.com/b/(?P<id>\d+)\Z')],
        ), cls=Tracker.Encoder))
        self.assertIsInstance(decoded, bugzilla.Tracker)
        self.assertEqual(decoded.url, 'https://bugs.example.com')
        self.assertEqual(decoded.from_string('example.com/b/1234').id, 1234)

    def test_default_timeout(self):
        self.assertEqual(bugzilla.Tracker(self.URL).timeout, bugzilla.Tracker.DEFAULT_TIMEOUT)

    def test_timeout(self):
        self.assertEqual(bugzilla.Tracker(self.URL, timeout=5).timeout, 5)

    def test_requests_use_timeout(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES) as mock:
            with patch.object(mock, 'request', wraps=mock.request) as request:
                self.assertEqual(bugzilla.Tracker(self.URL, timeout=5).issue(1).title, 'Example issue 1')
            self.assertTrue(request.call_args_list)
            self.assertEqual({call.kwargs.get('timeout') for call in request.call_args_list}, {5})

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

    def test_link_brackets(self):
        tracker = bugzilla.Tracker(self.URL)
        self.assertEqual(
            tracker.from_string('<http://bugs.example.com/show_bug.cgi?id=1234>').link,
            'https://bugs.example.com/show_bug.cgi?id=1234',
        )

    def test_title(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).title, 'Example issue 1')
            self.assertEqual(str(tracker.issue(1)), 'https://bugs.example.com/show_bug.cgi?id=1 Example issue 1')

    def test_timestamp(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(bugzilla.Tracker(self.URL).issue(1).timestamp, 1639510960)

    def test_modified(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            self.assertEqual(bugzilla.Tracker(self.URL).issue(1).modified, 1710859207)

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

    def test_attachments(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES):
            attachments = bugzilla.Tracker(self.URL).issue(1).attachments
            self.assertEqual([attachment.name for attachment in attachments], ['fix.patch', 'notes.txt'])
            self.assertEqual(attachments[0].content_type, 'text/plain')

    def test_patches(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES):
            patches = bugzilla.Tracker(self.URL).issue(1).patches
            self.assertEqual([patch.name for patch in patches], ['fix.patch'])
            self.assertEqual(patches[0].contents(), b'FIX PATCH BYTES')

    def test_no_attachments(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES):
            self.assertEqual(bugzilla.Tracker(self.URL).issue(2).attachments, [])
            self.assertEqual(bugzilla.Tracker(self.URL).issue(2).patches, [])

    def test_add_attachment(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual([attachment.name for attachment in issue.attachments], ['fix.patch', 'notes.txt'])

            added = issue.add_attachment('screenshot.png', b'PNG BYTES', content_type='image/png', summary='A screenshot')
            self.assertEqual(added.name, 'screenshot.png')
            self.assertEqual(added.content_type, 'image/png')
            self.assertEqual(added.contents(), b'PNG BYTES')
            self.assertEqual([attachment.name for attachment in issue.attachments], ['fix.patch', 'notes.txt', 'screenshot.png'])

            attachments = bugzilla.Tracker(self.URL).issue(1).attachments
            self.assertEqual([attachment.name for attachment in attachments], ['fix.patch', 'notes.txt', 'screenshot.png'])
            self.assertEqual(attachments[2].content_type, 'image/png')
            self.assertEqual(attachments[2].contents(), b'PNG BYTES')

    def test_add_attachment_before_listing(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(2)
            issue.add_attachment('notes.txt', b'some notes')
            self.assertEqual([attachment.name for attachment in issue.attachments], ['notes.txt'])
            self.assertEqual(issue.attachments[0].contents(), b'some notes')

    def test_add_attachment_request(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )) as mock:
            issue = bugzilla.Tracker(self.URL).issue(2)
            with patch.object(mock, 'request', wraps=mock.request) as request:
                issue.add_attachment('crash.log', b'LOG', content_type='text/x-log', summary='Crash log')

            posts = [call for call in request.call_args_list if call.args[0] == 'POST']
            self.assertEqual(len(posts), 1)
            self.assertTrue(posts[0].args[1].startswith('{}/rest/bug/2/attachment?login='.format(self.URL)))
            self.assertEqual(
                posts[0].kwargs['json'],
                dict(ids=[2], data='TE9H', file_name='crash.log', summary='Crash log', content_type='text/x-log'),
            )
            self.assertEqual(posts[0].kwargs['timeout'], bugzilla.Tracker.DEFAULT_TIMEOUT)

    def test_add_attachment_defaults(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )) as mock:
            issue = bugzilla.Tracker(self.URL).issue(2)
            with patch.object(mock, 'request', wraps=mock.request) as request:
                issue.add_attachment('build-log.txt', b'LOG')
                issue.add_attachment('artifact.unknownextension', b'DATA')

            payloads = [call.kwargs['json'] for call in request.call_args_list if call.args[0] == 'POST']
            self.assertEqual(
                [(payload['file_name'], payload['summary'], payload['content_type']) for payload in payloads], [
                    ('build-log.txt', 'build-log.txt', 'text/plain'),
                    ('artifact.unknownextension', 'artifact.unknownextension', 'application/octet-stream'),
                ],
            )

    def test_add_attachment_refused(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            issue = tracker.issue(2)
            self.assertEqual(issue.attachments, [])

            with self.assertRaises(RuntimeError) as caught:
                issue.add_attachment('empty.txt', b'')
            self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 400: The attachment is empty.')
            self.assertEqual(issue.attachments, [])
            self.assertEqual(bugzilla.Tracker(self.URL).issue(2).attachments, [])
            self.assertEqual(tracker._logins_left, 4)
        self.assertEqual(captured.stderr.getvalue(), '')

    def test_add_attachment_missing_bug(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            tracker = bugzilla.Tracker(self.URL)
            with self.assertRaises(RuntimeError) as caught:
                tracker.issue(100).add_attachment('notes.txt', b'some notes')
            self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 404: Bug #100 does not exist.')
            self.assertEqual(tracker._logins_left, 4)

    def test_add_attachment_refused_login(self):
        def refused_login(mock, url, id, credentials, data):
            response = error_response(url, 401, 300, 'The username or password you entered is not valid.')
            response.reason = 'Unauthorized'
            return response

        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch.object(mocks.Bugzilla, '_post_attachment', refused_login):
            tracker = bugzilla.Tracker(self.URL)
            with self.assertRaises(RuntimeError) as caught:
                tracker.issue(2).add_attachment('notes.txt', b'some notes')
            self.assertEqual(
                str(caught.exception),
                'Bugzilla attachment POST returned 401: The username or password you entered is not valid.',
            )
            self.assertEqual(tracker._logins_left, 3)

    def test_add_attachment_refused_without_a_message(self):
        def refusal(text):
            def bad_gateway(mock, url, id, credentials, data):
                response = wkmocks.Response(url=url, status_code=502, text=text)
                response.reason = 'Server Unavailable'
                return response
            return bad_gateway

        for text in ('Bad Gateway', json.dumps(dict(error=True, code=32000)), json.dumps(['Bad Gateway'])):
            with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch.object(mocks.Bugzilla, '_post_attachment', refusal(text)):
                tracker = bugzilla.Tracker(self.URL)
                with self.assertRaises(RuntimeError) as caught:
                    tracker.issue(2).add_attachment('notes.txt', b'some notes')
                self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 502: {}'.format(text))
                self.assertEqual(tracker._logins_left, 4)

    def test_add_attachment_refused_with_an_empty_body(self):
        def unauthorized(mock, url, id, credentials, data):
            response = wkmocks.Response(url=url, status_code=401)
            response.reason = 'Unauthorized'
            return response

        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch.object(mocks.Bugzilla, '_post_attachment', unauthorized):
            tracker = bugzilla.Tracker(self.URL)
            with self.assertRaises(RuntimeError) as caught:
                tracker.issue(2).add_attachment('notes.txt', b'some notes')
            self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 401: Unauthorized')
            self.assertEqual(tracker._logins_left, 4)

    def test_add_attachment_refused_with_an_empty_body_and_reason(self):
        def refusal(reason):
            def unauthorized(mock, url, id, credentials, data):
                response = wkmocks.Response(url=url, status_code=401)
                response.reason = reason
                return response
            return unauthorized

        for reason in (None, ''):
            with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            )), patch.object(mocks.Bugzilla, '_post_attachment', refusal(reason)):
                tracker = bugzilla.Tracker(self.URL)
                with self.assertRaises(RuntimeError) as caught:
                    tracker.issue(2).add_attachment('notes.txt', b'some notes')
                self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 401')
                self.assertEqual(tracker._logins_left, 4)

        # The mock's own answer to an unknown login has no body, and its response carries no reason at all.
        with mocks.Bugzilla(self.URL.split('://')[1], issues=ATTACHMENT_ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='nobody@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            with self.assertRaises(RuntimeError) as caught:
                bugzilla.Tracker(self.URL).issue(2).add_attachment('notes.txt', b'some notes')
            self.assertEqual(str(caught.exception), 'Bugzilla attachment POST returned 401')

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
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='wwatcher@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            tracker = bugzilla.Tracker(self.URL)
            tracker.issue(1).add_comment('Is this related to {}/show_bug.cgi?id=2?'.format(self.URL))
            self.assertEqual(tracker.issue(1).references, [])

    def test_reference_order(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='wwatcher@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            tracker = bugzilla.Tracker(self.URL)
            tracker.issue(1).add_comment('{}/show_bug.cgi?id=2?'.format(self.URL))
            tracker.issue(1).add_comment('{}/show_bug.cgi?id=3?'.format(self.URL))
            self.assertEqual(tracker.issue(1).references, [tracker.issue(3), tracker.issue(2)])

    def test_reference_multiline(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='wwatcher@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            tracker = bugzilla.Tracker(self.URL)
            tracker.issue(1).add_comment(f'{self.URL}/show_bug.cgi?id=2 is a bug.\n{self.URL}/show_bug.cgi?id=3 is another bug.')
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2), tracker.issue(3)])

            tracker.issue(1).add_comment(f'{self.URL}/show_bug.cgi?id=4')
            self.assertEqual(tracker.issue(1).references, [tracker.issue(4), tracker.issue(2), tracker.issue(3)])

    def test_me(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            self.assertEqual(
                User.Encoder().default(bugzilla.Tracker(self.URL).me()),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )

    def test_add_comment(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(len(issue.comments), 2)

            comment = issue.add_comment('Automated comment')
            self.assertEqual(comment.content, 'Automated comment')
            self.assertEqual(
                User.Encoder().default(comment.user),
                User.Encoder().default(bugzilla.Tracker(self.URL).me()),
            )

            self.assertEqual(len(issue.comments), 3)
            self.assertEqual(len(bugzilla.Tracker(self.URL).issue(1).comments), 3)

    def test_assign(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='ffiler@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            issue.assign(bugzilla.Tracker(self.URL).me())
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )

            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )

    def test_assign_why(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='ffiler@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            issue.assign(bugzilla.Tracker(self.URL).me(), why='Let me provide a better reproduction')
            self.assertEqual(
                User.Encoder().default(issue.assignee),
                dict(name='Felix Filer', username='ffiler@example.com', emails=['ffiler@example.com']),
            )
            self.assertEqual(issue.comments[-1].content, 'Let me provide a better reproduction')

    def test_state(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertTrue(issue.opened)
            self.assertFalse(issue.open())
            self.assertTrue(issue.close())
            self.assertFalse(issue.opened)

            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertFalse(issue.opened)
            self.assertFalse(issue.close())
            self.assertTrue(issue.open())
            self.assertTrue(issue.opened)

    def test_state_why(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertTrue(issue.opened)
            self.assertTrue(issue.close(why='Fixed in 1234@main'))
            self.assertFalse(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Fixed in 1234@main')

            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertFalse(issue.opened)
            self.assertTrue(issue.open(why='Need to revert, fix broke the build'))
            self.assertTrue(issue.opened)
            self.assertEqual(issue.comments[-1].content, 'Need to revert, fix broke the build')

    def test_set_state(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        )), OutputCapture() as captured:
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertFalse(issue.set_state(state='Analyze'))
            self.assertEqual(issue.state, None)
        self.assertEqual(
            captured.stderr.getvalue(),
            'Bugzilla does not support state at this time\n',
        )

    def test_set_substate(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        )), OutputCapture() as captured:
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertFalse(issue.set_state(state='Analyze', substate='Fix'))
            self.assertEqual(issue.state, None)
        self.assertEqual(
            captured.stderr.getvalue(),
            'Bugzilla does not support state at this time\n',
        )

    def test_duplicate(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )):
            tracker = bugzilla.Tracker(self.URL)
            issue = tracker.issue(1)
            self.assertTrue(issue.opened)
            self.assertTrue(issue.close(original=tracker.issue(2)))
            self.assertFalse(issue.opened)
            self.assertEqual(issue.original, tracker.issue(2))

            self.assertEqual(tracker.issue(1).original, tracker.issue(2))

    def test_projects(self):
        with mocks.Bugzilla(self.URL.split('://')[1], projects=mocks.PROJECTS):
            self.assertDictEqual(
                dict(
                    CFNetwork=dict(
                        description='Darwin networking framework',
                        versions=['All'],
                        components=dict(
                            IPv4=dict(description='Bugs involving IPv4 networking'),
                            IPv6=dict(description='Bugs involving IPv6 networking'),
                        ),
                    ), WebKit=dict(
                        description='The WebKit browser engine',
                        versions=['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
                        components=dict(
                            Scrolling=dict(description='Bugs related to main thread and off-main thread scrolling'),
                            SVG=dict(description='For bugs in the SVG implementation.'),
                            Tables=dict(description='For bugs specific to tables (both the DOM and rendering issues).'),
                            Text=dict(description='For bugs in text layout and rendering, including international text support.'),
                        ),
                    ),
                ), bugzilla.Tracker(self.URL).projects,
            )

    def test_create(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):
            created = bugzilla.Tracker(self.URL).create(
                'New bug', 'Creating new bug',
                project='WebKit', component='Tables', version='Other', keywords=['InRadar']
            )
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )

            self.assertEqual(created.project, 'WebKit')
            self.assertEqual(created.component, 'Tables')
            self.assertEqual(created.version, 'Other')
            self.assertEqual(created.keywords, ['InRadar'])

    def test_create_prompt(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES), wkmocks.Terminal.input('2', '1', '2'), OutputCapture() as captured:
            created = bugzilla.Tracker(self.URL).create('New bug', 'Creating new bug')
            self.assertEqual(created.id, 4)
            self.assertEqual(created.title, 'New bug')
            self.assertEqual(created.description, 'Creating new bug')
            self.assertTrue(created.opened)
            self.assertEqual(
                User.Encoder().default(created.creator),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )
            self.assertEqual(
                User.Encoder().default(created.assignee),
                dict(name='Tim Contributor', username='tcontributor@example.com', emails=['tcontributor@example.com']),
            )

            self.assertEqual(created.project, 'WebKit')
            self.assertEqual(created.component, 'SVG')

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
''',
        )

    def test_get_component(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Text')
            self.assertEqual(issue.version, 'Other')

    def test_invalid_keyword(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):

            with self.assertRaises(ValueError) as e:
                bugzilla.Tracker(self.URL).create(
                    'New bug', 'Creating new bug',
                    project='WebKit', component='Tables', version='Other', keywords=['InvalidKeyword']
                )
            self.assertEqual(f"'InvalidKeyword' is not a valid keyword for 'WebKit'", str(e.exception))

    def test_create_surfaces_server_error(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES), patch.object(bugzilla.Tracker, 'MAX_SUMMARY_LENGTH', 1000):
            tracker = bugzilla.Tracker(self.URL)
            with OutputCapture() as captured:
                created = tracker.create(
                    'A' * 300, 'Creating new bug',
                    project='WebKit', component='Tables', version='Other',
                )
            self.assertIsNone(created)
            self.assertEqual(tracker._logins_left, 4)
            self.assertEqual(
                captured.stderr.getvalue(),
                'Failed to create bug: The text you entered in the Summary field is too long (300 characters, above the maximum length allowed of 255 characters).\n',
            )

    def test_create_truncates_summary(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):
            long_title = 'A' * 300
            created = bugzilla.Tracker(self.URL).create(
                long_title, 'Creating new bug',
                project='WebKit', component='Tables', version='Other',
            )
            self.assertIsNotNone(created)
            self.assertEqual(len(created.title), 255)
            self.assertEqual(created.title, 'A' * 252 + '...')
            self.assertEqual(created.description, 'Creating new bug')

    def test_create_falls_back_to_default_version(self):
        projects = copy.deepcopy(mocks.PROJECTS)
        projects['WebKit']['versions'].insert(0, '528+ (Nightly build)')
        projects['WebKit']['inactive_versions'] = ['528+ (Nightly build)']
        projects['WebKit']['versions'].append('WebKit Nightly Build')

        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=projects, issues=mocks.ISSUES):
            with OutputCapture() as captured:
                created = bugzilla.Tracker(self.URL).create(
                    'New bug', 'Creating new bug',
                    project='WebKit', component='Tables', version='528+ (Nightly build)',
                )
            self.assertIsNotNone(created)
            self.assertEqual(created.version, 'WebKit Nightly Build')
            self.assertEqual(
                captured.stderr.getvalue(),
                "'528+ (Nightly build)' is not an active version for 'WebKit', using 'WebKit Nightly Build' instead\n",
            )

    def test_create_falls_back_to_last_version(self):
        projects = copy.deepcopy(mocks.PROJECTS)
        projects['WebKit']['versions'].insert(0, '528+ (Nightly build)')
        projects['WebKit']['inactive_versions'] = ['528+ (Nightly build)']

        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=projects, issues=mocks.ISSUES):
            with OutputCapture():
                created = bugzilla.Tracker(self.URL).create(
                    'New bug', 'Creating new bug',
                    project='WebKit', component='Tables', version='528+ (Nightly build)',
                )
            self.assertIsNotNone(created)
            self.assertEqual(created.version, 'WebKit Local Build')

    def test_create_no_version_is_silent(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):
            with OutputCapture() as captured:
                created = bugzilla.Tracker(self.URL).create(
                    'New bug', 'Creating new bug',
                    project='WebKit', component='Tables',
                )
            self.assertIsNotNone(created)
            self.assertEqual(created.version, 'WebKit Local Build')
            self.assertEqual(captured.stderr.getvalue(), '')

    def test_projects_failed_product_query(self):
        product_details = mocks.Bugzilla._product_details

        def failing_product_details(mock, url, id):
            if id == mocks.PROJECTS['CFNetwork']['id']:
                return wkmocks.Response(status_code=500, url=url)
            return product_details(mock, url, id)

        with mocks.Bugzilla(self.URL.split('://')[1], projects=mocks.PROJECTS):
            tracker = bugzilla.Tracker(self.URL)
            with patch.object(mocks.Bugzilla, '_product_details', failing_product_details), OutputCapture() as captured:
                self.assertEqual(list(tracker.projects.keys()), ['WebKit'])
            self.assertEqual(
                captured.stderr.getvalue(),
                "Failed to query bugzilla about prod '{}'\n".format(mocks.PROJECTS['CFNetwork']['id']),
            )
            self.assertEqual(sorted(tracker.projects.keys()), ['CFNetwork', 'WebKit'])

    def test_projects_complete_list_cached(self):
        request = mocks.Bugzilla.request
        urls = []

        def recording_request(mock, method, url, **kwargs):
            urls.append(url)
            return request(mock, method, url, **kwargs)

        with mocks.Bugzilla(self.URL.split('://')[1], projects=mocks.PROJECTS):
            tracker = bugzilla.Tracker(self.URL)
            with patch.object(mocks.Bugzilla, 'request', recording_request):
                tracker.projects
                first_read = len(urls)
                tracker.projects
            self.assertEqual(len(urls), first_read)

    def test_create_reads_partial_projects_once(self):
        product_details = mocks.Bugzilla._product_details
        request = mocks.Bugzilla.request
        urls = []

        def failing_product_details(mock, url, id):
            if id == mocks.PROJECTS['CFNetwork']['id']:
                return wkmocks.Response(status_code=500, url=url)
            return product_details(mock, url, id)

        def recording_request(mock, method, url, **kwargs):
            urls.append(url)
            return request(mock, method, url, **kwargs)

        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES), patch.object(mocks.Bugzilla, '_product_details', failing_product_details), \
                patch.object(mocks.Bugzilla, 'request', recording_request), OutputCapture():
            created = bugzilla.Tracker(self.URL).create(
                'Example issue', 'Creating new bug',
                project='WebKit', component='Tables', version='Other',
            )
        self.assertIsNotNone(created)
        self.assertEqual(len([url for url in urls if '/rest/product_enterable' in url]), 1)

    def test_projects_failed_list_query(self):
        request = mocks.Bugzilla.request

        def failing_request(mock, method, url, **kwargs):
            if '/rest/product_enterable' in url:
                return wkmocks.Response(status_code=500, url=url)
            return request(mock, method, url, **kwargs)

        with mocks.Bugzilla(self.URL.split('://')[1], projects=mocks.PROJECTS):
            tracker = bugzilla.Tracker(self.URL)
            with patch.object(mocks.Bugzilla, 'request', failing_request), OutputCapture() as captured:
                self.assertEqual(tracker.projects, {})
            self.assertEqual(captured.stderr.getvalue(), "Failed to retrieve project list'\n")
            self.assertEqual(sorted(tracker.projects.keys()), ['CFNetwork', 'WebKit'])

    def test_projects_excludes_inactive_versions(self):
        projects = copy.deepcopy(mocks.PROJECTS)
        projects['WebKit']['versions'].insert(0, '528+ (Nightly build)')
        projects['WebKit']['inactive_versions'] = ['528+ (Nightly build)']

        with mocks.Bugzilla(self.URL.split('://')[1], projects=projects):
            self.assertEqual(
                bugzilla.Tracker(self.URL).projects['WebKit']['versions'],
                ['Other', 'Safari 15', 'Safari Technology Preview', 'WebKit Local Build'],
            )

    def test_set_component(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):
            bugzilla.Tracker(self.URL).issue(1).set_component(project='WebKit', component='Tables', version='Safari 15')

            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(issue.project, 'WebKit')
            self.assertEqual(issue.component, 'Tables')
            self.assertEqual(issue.version, 'Safari 15')

    def test_labels(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = bugzilla.Tracker(self.URL).issue(1)
            self.assertEqual(issue.labels, [])

    def test_exhausted_logins(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS, issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            tracker._logins_left = 0

            with OutputCapture() as captured:
                self.assertFalse(tracker.issue(1).close())
            self.assertEqual(
                captured.stderr.getvalue(),
                'Exhausted login attempts\n'
                "Failed to modify 'https://bugs.example.com/show_bug.cgi?id=1 Example issue 1'\n",
            )

            with OutputCapture() as captured:
                self.assertIsNone(tracker.issue(1).add_comment('Failed comment'))
            self.assertEqual(
                captured.stderr.getvalue(),
                'Exhausted login attempts\n'
                "Failed to add comment to 'https://bugs.example.com/show_bug.cgi?id=1 Example issue 1'\n",
            )

            with OutputCapture() as captured, self.assertRaises(RuntimeError) as caught:
                tracker.issue(1).add_attachment('notes.txt', b'Failed attachment')
            self.assertEqual(str(caught.exception), 'Exhausted login attempts')
            self.assertEqual(captured.stderr.getvalue(), '')

            with OutputCapture() as captured:
                self.assertIsNone(tracker.create(
                    'New bug', 'Creating new bug',
                    project='WebKit', component='Tables', version='Other',
                ))
            self.assertEqual(
                captured.stderr.getvalue(),
                'Exhausted login attempts\n'
                'Failed to create bug: Login attempts exhausted\n',
            )

    def test_missing_bug_keeps_login_attempts(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            self.assertIsNone(tracker.issue(100).title)
            self.assertEqual(tracker._logins_left, 4)
        self.assertEqual(captured.stderr.getvalue(), 'Bug #100 does not exist.\n')

    def test_inaccessible_bug_keeps_login_attempts(self):
        def inaccessible_issue(mock, url, id, credentials=None, data=None):
            return error_response(url, 401, 102, 'You are not authorized to access bug #{}.'.format(id))

        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch.object(mocks.Bugzilla, '_issue', inaccessible_issue), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            self.assertIsNone(tracker.issue(1).title)
            self.assertEqual(tracker._logins_left, 4)
        self.assertEqual(captured.stderr.getvalue(), "Failed to fetch 'https://bugs.example.com/show_bug.cgi?id=1'\n")

    def test_refused_login_uses_login_attempt(self):
        def refused_login(mock, url, id, credentials=None, data=None):
            return error_response(url, 401, 300, 'The username or password you entered is not valid.')

        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch.object(mocks.Bugzilla, '_issue', refused_login), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            self.assertIsNone(tracker.issue(1).title)
            self.assertEqual(tracker._logins_left, 3)
        self.assertEqual(captured.stderr.getvalue(), "Failed to fetch 'https://bugs.example.com/show_bug.cgi?id=1'\n")

    def test_refused_logins_exhaust_login_attempts(self):
        def refused_login(mock, url, id, credentials=None, data=None):
            return error_response(url, 401, 300, 'The username or password you entered is not valid.')

        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        )), patch.object(mocks.Bugzilla, '_issue', refused_login), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            for _ in range(5):
                tracker.issue(1).title
            self.assertEqual(tracker._logins_left, 0)
            self.assertIsNone(tracker.issue(1).add_comment('Failed comment'))
        self.assertIn('Exhausted login attempts\n', captured.stderr.getvalue())

    def test_count_failed_login(self):
        for status_code, text, expected in (
            (401, json.dumps(dict(code=300)), 3),
            (401, json.dumps(dict(code=301)), 3),
            (401, json.dumps(dict(code=102)), 4),
            (404, json.dumps(dict(code=101)), 4),
            (400, json.dumps(dict(code=32000)), 4),
            (401, '', 4),
            (403, '<html></html>', 4),
            (401, json.dumps([300]), 4),
            (500, json.dumps(dict(code=300)), 4),
        ):
            tracker = bugzilla.Tracker(self.URL)
            tracker._count_failed_login(wkmocks.Response(url=self.URL, status_code=status_code, text=text))
            self.assertEqual(tracker._logins_left, expected, '{} {}'.format(status_code, text))
        tracker = bugzilla.Tracker(self.URL)
        tracker._count_failed_login(None)
        self.assertEqual(tracker._logins_left, 4)

    def test_missing_bug_with_exhausted_logins(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES), OutputCapture() as captured:
            tracker = bugzilla.Tracker(self.URL)
            tracker._logins_left = 0
            self.assertIsNone(tracker.issue(100).title)
        self.assertEqual(captured.stderr.getvalue(), 'Bug #100 does not exist.\n')

    def test_redaction(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            self.assertEqual(bugzilla.Tracker(self.URL, redact=None).issue(1).redacted, False)
            self.assertTrue(bool(bugzilla.Tracker(self.URL, redact={'.*': True}).issue(1).redacted))
            self.assertEqual(
                bugzilla.Tracker(self.URL, redact={'.*': True}).issue(1).redacted,
                bugzilla.Tracker.Redaction(True, 'is a Bugzilla'),
            )
            self.assertEqual(
                bugzilla.Tracker(self.URL, redact={'project:WebKit': True}).issue(1).redacted,
                bugzilla.Tracker.Redaction(True, "matches 'project:WebKit'"),
            )
            self.assertEqual(
                bugzilla.Tracker(self.URL, redact={'component:Text': True}).issue(1).redacted,
                bugzilla.Tracker.Redaction(True, "matches 'component:Text'"),
            )
            self.assertEqual(
                bugzilla.Tracker(self.URL, redact={'version:Other': True}).issue(1).redacted,
                bugzilla.Tracker.Redaction(True, "matches 'version:Other'"),
            )

    def test_redacted_duplicate(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS):
            tracker = bugzilla.Tracker(self.URL, redact={'component:Text': True})
            self.assertEqual(tracker.issue(1).redacted, bugzilla.Tracker.Redaction(True, "matches 'component:Text'"))
            self.assertEqual(tracker.issue(2).redacted, False)
            tracker.issue(1).close(original=tracker.issue(2))
            self.assertEqual(tracker.issue(2).redacted, bugzilla.Tracker.Redaction(True, "is related to https://bugs.example.com/show_bug.cgi?id=1 which matches 'component:Text'"))

    def test_redacted_original(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), projects=mocks.PROJECTS):
            tracker = bugzilla.Tracker(self.URL, redact={'component:Text': True})
            self.assertEqual(tracker.issue(1).redacted, bugzilla.Tracker.Redaction(True, "matches 'component:Text'"))
            self.assertEqual(tracker.issue(2).redacted, False)
            tracker.issue(2).close(original=tracker.issue(1))
            self.assertEqual(tracker.issue(2).redacted, bugzilla.Tracker.Redaction(True, "is related to https://bugs.example.com/show_bug.cgi?id=1 which matches 'component:Text'"))

    def test_redaction_exception(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES, projects=mocks.PROJECTS):
            self.assertEqual(bugzilla.Tracker(
                self.URL,
                redact={'.*': True},
                redact_exemption={'component:Text': True},
            ).issue(1).redacted, bugzilla.Tracker.Redaction(
                exemption=True, reason="matches 'component:Text'",
            ))
            self.assertEqual(bugzilla.Tracker(
                self.URL,
                redact={'.*': True},
                redact_exemption={'component:Scrolling': True},
            ).issue(1).redacted, bugzilla.Tracker.Redaction(True, 'is a Bugzilla'))

    def test_cc_no_radar(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS):
            issue = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer']).issue(1)
            self.assertEqual(issue.references, [])
            self.assertIsNone(issue.cc_radar(block=True))

    def test_cc_no_importer(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.NoRadar():
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL)

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertEqual(issue.references, [])
                self.assertIsNone(issue.cc_radar(block=True))

    def test_cc_with_radar(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.NoRadar():
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertEqual(issue.references, [])
                self.assertIsNotNone(issue.cc_radar(block=True))
                self.assertEqual(len(issue.references), 1)
                self.assertEqual(issue.references[0].link, 'rdar://1')
                self.assertEqual(issue.references[0].title, None)

    def test_cc_with_radarclient(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ):
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertEqual(issue.references, [])
                self.assertIsNotNone(issue.cc_radar(block=True))
                self.assertEqual(len(issue.references), 1)
                self.assertEqual(issue.references[0].link, 'rdar://4')
                self.assertEqual(issue.references[0].title, 'An example issue for testing (1)')

    def test_cc_existing_radar(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertEqual(issue.references, [])
                self.assertIsNotNone(issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>')))
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                self.assertEqual(len(issue.references), 1)
                self.assertEqual(issue.references[0].link, 'rdar://1')
                self.assertEqual(issue.references[0].title, 'Example issue 1')

    def test_cc_radar_after_create_waits_for_next_second(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.create('New bug', 'Creating new bug', project='WebKit', component='Tables', version='Other')
                start = time.time()
                issue.cc_radar()
                self.assertGreater(time.time() - start, 1)
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], bugzilla.Tracker(self.URL).issue(issue.id).watchers)

    def test_cc_radar_existing_bug_does_not_wait(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
            BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
            BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                start = time.time()
                issue.cc_radar()
                self.assertEqual(time.time() - start, 0)
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], bugzilla.Tracker(self.URL).issue(1).watchers)

    def test_cc_radar_importer_already_cced_with_explicit_radar(self):
        """When the importer is already CC'd (e.g. auto-CC on Security product) and
        an explicit radar is provided, cc_radar should add InRadar + comment without
        prompting.  This tests the fix for the unwanted Terminal.choose prompt."""
        # Use issue data where the importer is already in watchers
        issues_with_importer = [dict(
            title='Security bug',
            timestamp=1639536160,
            opened=True,
            creator=mocks.USERS['Tim Contributor'],
            assignee=mocks.USERS['Tim Contributor'],
            description='A security issue',
            project='WebKit',
            component='Text',
            version='Other',
            keywords=['InRadar'],
            comments=[],
            watchers=[
                mocks.USERS['Tim Contributor'],
                mocks.USERS['Radar WebKit Bug Importer'],  # Already CC'd
            ],
        )]
        with OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_importer, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                # Importer is already a watcher
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], issue.watchers)
                # cc_radar should succeed without prompting
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                # Should have added the rdar comment
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                # Should NOT have prompted (no Terminal.choose in output)
                self.assertNotIn('Would you like to CC', captured.stderr.getvalue())
                self.assertNotIn('Would you like to CC', captured.stdout.getvalue())

    def test_cc_radar_importer_already_cced_no_inradar_prompts(self):
        """When the importer is already CC'd, no radar is tracked, and InRadar
        is NOT in keywords, cc_radar should prompt and proceed when user says Yes."""
        issues_with_importer = [dict(
            title='Bug without InRadar',
            timestamp=1639536160,
            opened=True,
            creator=mocks.USERS['Tim Contributor'],
            assignee=mocks.USERS['Tim Contributor'],
            description='A test issue',
            project='WebKit',
            component='Text',
            version='Other',
            keywords=[],
            comments=[],
            watchers=[
                mocks.USERS['Tim Contributor'],
                mocks.USERS['Radar WebKit Bug Importer'],
            ],
        )]
        with wkmocks.Terminal.input('Yes'), OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_importer, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                # Should have prompted
                self.assertIn('already CCed but no Radar was imported', captured.stderr.getvalue())

    def test_cc_radar_importer_already_cced_different_tracked_bug(self):
        """When the importer is already CC'd and a different radar is already
        tracked, cc_radar should prompt to overwrite and proceed when user says Yes."""
        issues_with_tracked = [
            dict(
                title='Bug with tracked reference',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='A test issue\n<rdar://problem/2>',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[
                    mocks.USERS['Tim Contributor'],
                    mocks.USERS['Radar WebKit Bug Importer'],
                ],
            ),
        ]
        with wkmocks.Terminal.input('Yes'), OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_tracked, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                # Should have prompted about overwriting
                self.assertIn('tracking a different bug', captured.stderr.getvalue())

    @contextmanager
    def importer_cced(self, answer, description):
        issues = [dict(
            title='Bug with the importer CCed',
            timestamp=1639536160,
            opened=True,
            creator=mocks.USERS['Tim Contributor'],
            assignee=mocks.USERS['Tim Contributor'],
            description=description,
            project='WebKit',
            component='Text',
            version='Other',
            keywords=[],
            comments=[],
            watchers=[
                mocks.USERS['Tim Contributor'],
                mocks.USERS['Radar WebKit Bug Importer'],
            ],
        )]
        with wkmocks.Terminal.input(answer), OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                yield bugzilla_tracker.issue(1), captured

    def test_cc_radar_importer_already_cced_no_inradar_exit(self):
        with self.importer_cced('Exit', 'A test issue') as (issue, _):
            with self.assertRaises(ValueError) as context:
                issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
            self.assertEqual(str(context.exception), "Radar Importer is already CC'd")
            self.assertEqual(issue.comments, [])

    def test_cc_radar_importer_already_cced_no_inradar_default_is_exit(self):
        with self.importer_cced('', 'A test issue') as (issue, _):
            with self.assertRaises(ValueError) as context:
                issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
            self.assertEqual(str(context.exception), "Radar Importer is already CC'd")
            self.assertEqual(issue.comments, [])

    def test_cc_radar_importer_already_cced_no_inradar_skip_cc(self):
        with self.importer_cced('Skip CC', 'A test issue') as (issue, captured):
            self.assertIsNone(issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>')))
            self.assertEqual(issue.comments, [])
            self.assertIn('Skipping CC for', captured.stdout.getvalue())

    def test_cc_radar_importer_already_cced_different_tracked_bug_exit(self):
        with self.importer_cced('Exit', 'A test issue\n<rdar://problem/2>') as (issue, _):
            with self.assertRaises(ValueError) as context:
                issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
            self.assertEqual(str(context.exception), 'Radar is tracking a different bug')
            self.assertEqual(issue.comments, [])

    def test_cc_radar_importer_already_cced_different_tracked_bug_default_is_exit(self):
        with self.importer_cced('', 'A test issue\n<rdar://problem/2>') as (issue, _):
            with self.assertRaises(ValueError) as context:
                issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
            self.assertEqual(str(context.exception), 'Radar is tracking a different bug')
            self.assertEqual(issue.comments, [])

    def test_cc_radar_importer_already_cced_different_tracked_bug_skip_cc(self):
        with self.importer_cced('Skip CC', 'A test issue\n<rdar://problem/2>') as (issue, captured):
            self.assertIsNone(issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>')))
            self.assertEqual(issue.comments, [])
            self.assertIn('Skipping CC for', captured.stdout.getvalue())

    def test_cc_radar_see_also_not_confused_as_tracked_radar(self):
        """When the importer is already CC'd and see-also links reference other
        bugzilla bugs, cc_radar should not treat those as tracked radars.
        Regression test: see-also bugzilla references in issue.references[0]
        caused cc_radar to prompt 'tracking a different bug' for a non-radar."""
        issues_with_see_also = [
            dict(
                title='Security bug with see-also',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='A security issue',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=['InRadar'],
                comments=[],
                watchers=[
                    mocks.USERS['Tim Contributor'],
                    mocks.USERS['Radar WebKit Bug Importer'],
                ],
                references=[2],
            ),
            dict(
                title='Related bug via see-also',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='Another bug',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[mocks.USERS['Tim Contributor']],
            ),
        ]
        with OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_see_also, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], issue.watchers)
                self.assertTrue(len(issue.references) > 0)
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                self.assertNotIn('tracking a different bug', captured.stderr.getvalue())

    def test_cc_radar_multiple_see_also_no_radar_references(self):
        """When the importer is already CC'd and multiple see-also links reference
        other bugzilla bugs (no radars), cc_radar should not prompt about tracking
        a different bug."""
        issues_with_multiple_see_also = [
            dict(
                title='Bug with multiple see-also',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='A test issue',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=['InRadar'],
                comments=[],
                watchers=[
                    mocks.USERS['Tim Contributor'],
                    mocks.USERS['Radar WebKit Bug Importer'],
                ],
                references=[2, 3],
            ),
            dict(
                title='Related bug one',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='First related bug',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[mocks.USERS['Tim Contributor']],
            ),
            dict(
                title='Related bug two',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='Second related bug',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[mocks.USERS['Tim Contributor']],
            ),
        ]
        with OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_multiple_see_also, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], issue.watchers)
                self.assertTrue(len(issue.references) >= 2)
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                self.assertNotIn('tracking a different bug', captured.stderr.getvalue())

    def test_cc_radar_multiple_references_with_one_radar(self):
        """When the importer is already CC'd and references include both bugzilla
        see-also links and a tracked radar, cc_radar should prompt about the
        tracked radar and ignore the bugzilla references."""
        issues_with_mixed_refs = [
            dict(
                title='Bug with see-also and radar',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='A test issue\n<rdar://problem/2>',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[
                    mocks.USERS['Tim Contributor'],
                    mocks.USERS['Radar WebKit Bug Importer'],
                ],
                references=[2],
            ),
            dict(
                title='Related bug via see-also',
                timestamp=1639536160,
                opened=True,
                creator=mocks.USERS['Tim Contributor'],
                assignee=mocks.USERS['Tim Contributor'],
                description='Another bug',
                project='WebKit',
                component='Text',
                version='Other',
                keywords=[],
                comments=[],
                watchers=[mocks.USERS['Tim Contributor']],
            ),
        ]
        with wkmocks.Terminal.input('Yes'), OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_mixed_refs, projects=mocks.PROJECTS,
        ), mocks.Radar(
            users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,
        ), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                issue = bugzilla_tracker.issue(1)
                self.assertIn(mocks.USERS['Radar WebKit Bug Importer'], issue.watchers)
                self.assertTrue(len(issue.references) >= 2)
                result = issue.cc_radar(block=True, radar=Tracker.from_string('<rdar://1>'))
                self.assertIsNotNone(result)
                self.assertEqual(issue.comments[-1].content, '<rdar://problem/1>')
                self.assertIn('tracking a different bug', captured.stderr.getvalue())

    def test_cc_radar_importer_cced_by_default(self):
        """When the component CCs the importer by default, cc_radar should wait for it
        to import the bug, just as it does after CCing the importer itself."""
        issues_with_importer = [dict(
            title='Bug in a component which CCs the importer',
            timestamp=1639536160,
            opened=True,
            creator=mocks.USERS['Tim Contributor'],
            assignee=mocks.USERS['Tim Contributor'],
            description='A test issue',
            project='WebKit',
            component='Text',
            version='Other',
            keywords=[],
            comments=[],
            watchers=[
                mocks.USERS['Tim Contributor'],
                mocks.USERS['Radar WebKit Bug Importer'],
            ],
        )]
        with OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_with_importer, projects=mocks.PROJECTS,
        ) as bugzilla_mock, mocks.NoRadar(), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])
            sleep = time.sleep

            def import_bug(seconds):
                sleep(seconds)
                bugzilla_mock.issues[1]['comments'].append(Issue.Comment(
                    user=mocks.USERS['Radar WebKit Bug Importer'],
                    timestamp=int(time.time()),
                    content='<rdar://problem/1>',
                ))

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]), patch('time.sleep', new=import_bug):
                issue = bugzilla_tracker.issue(1)
                self.assertEqual(issue.references, [])
                self.assertIsNotNone(issue.cc_radar(block=True))
                self.assertEqual(len(issue.references), 1)
                self.assertEqual(issue.references[0].link, 'rdar://1')

        self.assertEqual(captured.stdout.getvalue(), 'Waiting until Radar WebKit Bug Importer imports bug...\n')

    def test_cc_radar_importer_cced_by_default_in_radar(self):
        """When the importer is already CC'd and the bug is InRadar, there is no import to wait for."""
        issues_in_radar = [dict(
            title='Bug which is already in radar',
            timestamp=1639536160,
            opened=True,
            creator=mocks.USERS['Tim Contributor'],
            assignee=mocks.USERS['Tim Contributor'],
            description='A test issue',
            project='WebKit',
            component='Text',
            version='Other',
            keywords=['InRadar'],
            comments=[],
            watchers=[
                mocks.USERS['Tim Contributor'],
                mocks.USERS['Radar WebKit Bug Importer'],
            ],
        )]
        with OutputCapture(level=logging.INFO) as captured, mocks.Bugzilla(
            self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=issues_in_radar, projects=mocks.PROJECTS,
        ), mocks.NoRadar(), wkmocks.Time:
            radar_tracker = radar.Tracker()
            bugzilla_tracker = bugzilla.Tracker(self.URL, radar_importer=mocks.USERS['Radar WebKit Bug Importer'])

            with patch('webkitbugspy.Tracker._trackers', [radar_tracker, bugzilla_tracker]):
                self.assertIsNone(bugzilla_tracker.issue(1).cc_radar(block=True))

        self.assertEqual(captured.stdout.getvalue(), '')

    def test_milestone(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).milestone, None)

    def test_keywords(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).keywords, ['Keyword A'])

    def test_classification(self):
        with mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).classification, '')

    def test_set_keywords(self):
        with OutputCapture(level=logging.INFO), mocks.Bugzilla(
            self.URL.split('://')[1],
            projects=mocks.PROJECTS,
            environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
            ), users=mocks.USERS, issues=mocks.ISSUES,
        ):

            tracker = bugzilla.Tracker(self.URL)
            issue = tracker.issue(1)

            self.assertEqual(issue.keywords, ['Keyword A'])
            issue.set_keywords(['REGRESSION'])
            self.assertEqual(issue.keywords, ['REGRESSION'])
            self.assertEqual(tracker.issue(1).keywords, ['REGRESSION'])

    def test_relate_simple(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            issue = tracker.issue(1)

            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})
            issue.relate(depends_on=tracker.issue(2))
            self.assertEqual(issue.related['depends_on'], [tracker.issue(2)])
            self.assertEqual(issue.related['blocks'], [])

            # The edge runs both ways
            self.assertEqual(tracker.issue(2).related['blocks'], [issue])

            issue.unrelate(depends_on=tracker.issue(2))
            self.assertEqual(issue.related['depends_on'], [])
            self.assertEqual(tracker.issue(2).related['blocks'], [])

    def test_relate(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            issue = tracker.issue(1)

            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})
            issue.relate(depends_on=tracker.issue(2), blocks=tracker.issue(3))
            self.assertEqual(issue.related['depends_on'], [tracker.issue(2)])
            self.assertEqual(issue.related['blocks'], [tracker.issue(3)])

    def test_relate_fail(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)

            issue = tracker.issue(1)

            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})
            with self.assertRaises(TypeError) as c:
                issue.relate(fake_relation=0)
            self.assertEqual('\'fake_relation\' is an invalid relation', str(c.exception))
            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})

    def test_relate_fail_type(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)

            issue = tracker.issue(1)

            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})
            with self.assertRaises(AttributeError) as c:
                issue.relate(blocks=17)
            self.assertEqual('\'int\' object has no attribute \'tracker\'', str(c.exception))
            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})

    def test_relate_fail_radar(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES), mocks.Radar(users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,):
            bugzilla_tracker = bugzilla.Tracker(self.URL)
            radar_tracker = radar.Tracker()

            issue = bugzilla_tracker.issue(1)

            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})
            with self.assertRaises(TypeError) as c:
                issue.relate(blocks=radar_tracker.issue(1))
            self.assertEqual('Cannot relate issues of different types.', str(c.exception))
            self.assertEqual(issue.related, {'depends_on': [], 'blocks': [], 'regressions': [], 'regressed_by': []})

    def test_source_changes(self):
        with OutputCapture() as captured, mocks.Bugzilla(self.URL.split('://')[1], issues=mocks.ISSUES):
            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).source_changes, [])
            self.assertIsNone(tracker.issue(1).add_source_change('WebKit, merge, a4daad5b9fbd26d557088037f54dc0935a437182'))

        self.assertEqual(
            captured.stderr.getvalue(),
            'Bugzilla does not support source changes at this time\n',
        )

    def test_related_links(self):
        with mocks.Bugzilla(self.URL.split('://')[1], environment=wkmocks.Environment(
                BUGS_EXAMPLE_COM_USERNAME='tcontributor@example.com',
                BUGS_EXAMPLE_COM_PASSWORD='password',
        ), users=mocks.USERS, issues=mocks.ISSUES), mocks.Radar(users=mocks.USERS, issues=mocks.ISSUES, projects=mocks.PROJECTS,):

            tracker = bugzilla.Tracker(self.URL)
            self.assertEqual(tracker.issue(1).related_links, [])
            self.assertTrue(tracker.issue(1).add_related_links([f'{self.URL}/show_bug.cgi?id=2', 'other.tracker.com/123']))
            self.assertEqual(tracker.issue(1).related_links, [f'{self.URL}/show_bug.cgi?id=2', 'other.tracker.com/123'])
            self.assertEqual(tracker.issue(1).references, [tracker.issue(2)])
