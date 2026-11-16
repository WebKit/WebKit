# Copyright (C) 2020-2026 Apple Inc. All rights reserved.
# Copyright (C) 2021 Igalia S.L.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
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
import os
import time
import unittest
from unittest.mock import Mock, patch

from buildbot.test import fakedb
from buildbot.test.fake import fakemaster
from buildbot.test.reactor import TestReactorMixin
from twisted.internet import defer, task
from twisted.python import log
from twisted.trial import unittest as trial_unittest

from . import loadConfig  # noqa: F401 (applies the set_build_requests_priority backport)
from .events import CommitClassifier, Events, GitHubEventHandlerNoEdits
from .steps import GitHub
from .twisted_additions import TwistedAdditions


class TestCommitClassifier(unittest.TestCase):
    def test_regex_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('versioning.'))
        self.assertTrue(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning'))

        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Bumped versioning'))
        self.assertFalse(CommitClassifier.LineFilter('^[Vv]ersioning\\.?$')('Versioning bumped.'))

    def test_fuzzy_header_filter(self):
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[Gardening] Skip n tests'))
        self.assertTrue(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[girdening] Skip n tests'))

        self.assertFalse(CommitClassifier.LineFilter({"value": "gardening", "ratio": 85})('[gdening] Skip n tests'))

    def test_commit_class_gardening(self):
        c = CommitClassifier(
            name='Gardening',
            pickable=False,
            headers=[dict(value='gardening', ratio=85)],
            paths=[
                'LayoutTests/',
                'Tools/TestWebKitAPI',
            ],
        )
        self.assertEqual(c.name, 'Gardening')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('[Gardening] Skip n tests', [], ['LayoutTests/TestExpectations']))

        self.assertFalse(c.matches('[Gardening] Skip n tests', [], []))
        self.assertFalse(c.matches('[gdening] Skip n tests', [], ['LayoutTests/TestExpectations']))
        self.assertFalse(c.matches('[Gardening] Skip n tests', [], ['Makefile', 'LayoutTests/TestExpectations']))

    def test_commit_class_cherry_pick(self):
        c = CommitClassifier(
            name='Cherry-pick',
            pickable=False,
            headers=['^[Cc]herry[- ][Pp]ick'],
            trailers=['^[Oo]riginally[- ]landed[- ]as:'],
        )
        self.assertEqual(c.name, 'Cherry-pick')
        self.assertFalse(c.pickable)
        self.assertTrue(c.matches('Cherry-pick abcde', [], ['somefile']))
        self.assertTrue(c.matches('cherry-pick abcde', [], ['otherfile']))
        self.assertTrue(c.matches('Cherry-Pick abcde', [], []))
        self.assertTrue(c.matches('cherry pick abcde', [], []))
        self.assertTrue(c.matches('[Component] Some change', ['Originally-landed-as: 1234.1@branch (abcde)'], []))

        self.assertFalse(c.matches('Partial cherry-pick', [], []))
        self.assertFalse(c.matches('Took cherry-pick', [], []))

    def test_commit_class_tools(self):
        c = CommitClassifier(
            name='Tools',
            pickable=True,
            paths=['Tools', 'LayoutTests', 'metadata'],
        )
        self.assertEqual(c.name, 'Tools')
        self.assertTrue(c.pickable)
        self.assertTrue(c.matches('Some change', [], ['Tools/Scripts/git-webkit']))
        self.assertTrue(c.matches('', [], ['LayoutTests/some/test.html']))
        self.assertTrue(c.matches('', [], ['metadata/contributors.json']))
        self.assertTrue(c.matches('', [], ['Tools/Scripts/run-webkit-tests', 'metadata/commit_classes.json']))

        self.assertFalse(c.matches('', [], []))
        self.assertFalse(c.matches('', [], ['metadata/contributors.json', 'Makefile']))

    def test_duplicate(self):
        key_a = 'pull_request-3babd344c6c08255c3ce2db7459b57d99209ae43'
        key_b = 'merge_queue-3babd344c6c08255c3ce2db7459b57d99209ae43'

        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertTrue(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_b, bucket=1))

        time.sleep(.5)
        self.assertTrue(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))

        time.sleep(2)
        self.assertFalse(GitHubEventHandlerNoEdits.is_duped(key_a, bucket=1))
        self.assertTrue(len(GitHubEventHandlerNoEdits.DUPE_DETECTOR.keys()), 2)


class TestSendDataToEWS(unittest.TestCase):
    STEP_DATA = dict(type='ews-step', status='finished', step_id=123, build_id=456)
    BUILD_DATA = dict(type='ews-build', status='finished', build_id=456)

    def setUp(self):
        self.events = Events('ews-build.webkit.org', type_prefix='ews')
        self.delays = []
        self.logs = []

        def fake_defer_later(clock, delay, callable, *args, **kwargs):
            self.delays.append(delay)
            return defer.succeed(callable(*args, **kwargs))

        for patcher in (
            patch.object(task, 'deferLater', fake_defer_later),
            patch.object(log, 'msg', lambda message, *args, **kwargs: self.logs.append(message)),
            patch.dict(os.environ, {'EWS_API_KEY': 'secret-api-key'}),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    def send(self, responses, data=None, retry=False):
        responses = list(responses)
        calls = []

        def fake_request(**kwargs):
            calls.append(kwargs)
            response = responses.pop(0)
            if isinstance(response, Exception):
                return defer.fail(response)
            return defer.succeed(None if response is None else TwistedAdditions.Response(status_code=response))

        results = []
        errors = []
        with patch.object(TwistedAdditions, 'request', side_effect=fake_request):
            d = self.events.sendDataToEWS(dict(data or self.STEP_DATA), retry=retry)
            d.addCallbacks(results.append, errors.append)
        self.assertEqual(errors, [])
        self.assertEqual(len(results), 1)
        return results[0], calls

    def test_success_first_try(self):
        result, calls = self.send([200])
        self.assertTrue(result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0]['json']['EWS_API_KEY'], 'secret-api-key')
        self.assertIsNotNone(calls[0]['logger'])
        self.assertEqual(calls[0]['timeout'], 60)
        self.assertEqual(self.delays, [])
        self.assertEqual(self.logs, [])

    def test_no_retry_by_default(self):
        result, calls = self.send([None])
        self.assertFalse(result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.delays, [])
        self.assertEqual(self.logs, ['Failed to send ews-step finished step_id=123 build_id=456 to EWS (no response)'])

    def test_retry_on_no_response(self):
        result, calls = self.send([None, 200], data=self.BUILD_DATA, retry=True)
        self.assertTrue(result)
        self.assertEqual(len(calls), 2)
        self.assertEqual(self.delays, [5])
        self.assertEqual(self.logs, ['Failed to send ews-build finished build_id=456 to EWS (no response), retrying in 5 seconds'])

    def test_retry_on_server_error(self):
        result, calls = self.send([500, 503, 200], data=self.BUILD_DATA, retry=True)
        self.assertTrue(result)
        self.assertEqual(len(calls), 3)
        self.assertEqual(self.delays, [5, 30])
        self.assertEqual(self.logs, [
            'Failed to send ews-build finished build_id=456 to EWS (status 500), retrying in 5 seconds',
            'Failed to send ews-build finished build_id=456 to EWS (status 503), retrying in 30 seconds',
        ])

    def test_no_retry_on_client_error(self):
        result, calls = self.send([403], retry=True)
        self.assertFalse(result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.delays, [])
        self.assertEqual(self.logs, ['EWS rejected ews-step finished step_id=123 build_id=456 with status 403, not retrying'])

    def test_no_retry_on_success_status(self):
        result, calls = self.send([201], retry=True)
        self.assertTrue(result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.delays, [])

    def test_give_up_after_max_retries(self):
        result, calls = self.send([None, 500, 502, None, 503, None], data=self.BUILD_DATA, retry=True)
        self.assertFalse(result)
        self.assertEqual(len(calls), 6)
        self.assertEqual(self.delays, [5, 30, 120, 600, 1800])
        self.assertEqual(self.logs[-1], 'Giving up sending ews-build finished build_id=456 to EWS after 6 attempts (no response)')
        self.assertEqual(len(self.logs), 6)

    def test_retry_delays_overridable(self):
        self.events.EWS_RETRY_DELAYS = [0]
        result, calls = self.send([None, None], retry=True)
        self.assertFalse(result)
        self.assertEqual(len(calls), 2)
        self.assertEqual(self.delays, [0])

    def test_unexpected_exception_does_not_errback(self):
        result, calls = self.send([RuntimeError('boom')])
        self.assertFalse(result)
        self.assertEqual(len(calls), 1)
        self.assertEqual(self.logs, ['Unexpected RuntimeError while sending event to EWS: boom'])

    def test_api_key_not_logged(self):
        self.send([None, 500, 404], retry=True)
        self.assertTrue(self.logs)
        for message in self.logs:
            self.assertNotIn('secret-api-key', message)

    def test_only_build_finished_events_retry(self):
        build = dict(buildid=456, builderid=10, number=5, results=0, started_at=1, complete_at=2, state_string='done',
                     properties={'buildername': ['WebKitPy-Tests-EWS', 'Builder'], 'github.head.sha': ['abc', 'Build']})
        step = dict(stepid=123, buildid=456, name='layout-tests', results=None, state_string='pending', started_at=1, complete_at=None)
        master = Mock()
        master.db.builders.getBuilder.side_effect = lambda _: defer.succeed(dict(description='webkitpy'))
        calls = []
        with patch.object(Events, 'master', master), \
                patch.object(Events, 'sendDataToEWS', lambda _, data, retry=False: calls.append((data['type'], data['status'], retry))):
            self.events.buildStarted(None, dict(build))
            self.events.buildFinished(None, dict(build))
            self.events.stepStarted(None, dict(step))
            self.events.stepFinished(None, dict(step))
        self.assertEqual(calls, [
            ('ews-build', 'started', False),
            ('ews-build', 'finished', True),
            ('ews-step', 'started', False),
            ('ews-step', 'finished', False),
        ])


class TestPriorityLabels(TestReactorMixin, trial_unittest.TestCase):
    PR_NUMBER = 80135
    HEAD_SHA = 'd2daf5dde4d47a1ecc8a42b31d68287a6265563e'
    OLD_SHA = 'cf887897c1e787a39e9c50cc07fbd6a4b98dbf36'

    @defer.inlineCallbacks
    def setUp(self):
        self.setup_test_reactor()
        self.master = yield defer.ensureDeferred(fakemaster.make_master(self, wantDb=True))
        yield self.master.db.insert_test_data([
            fakedb.Master(id=88),
            fakedb.Builder(id=77, name='iOS-27-Simulator-Build-EWS'),
            fakedb.SourceStamp(id=234),
            *[row for buildsetid, pr_number, sha in [
                (20, self.PR_NUMBER, self.HEAD_SHA),
                (21, self.PR_NUMBER, self.OLD_SHA),
                (22, 80136, self.HEAD_SHA),
            ] for row in [
                fakedb.Buildset(id=buildsetid),
                fakedb.BuildsetSourceStamp(buildsetid=buildsetid, sourcestampid=234),
                fakedb.BuildsetProperty(buildsetid=buildsetid, property_name='github.number', property_value=json.dumps([pr_number, 'Change'])),
                fakedb.BuildsetProperty(buildsetid=buildsetid, property_name='github.head.sha', property_value=json.dumps([sha, 'Change'])),
            ]],
            fakedb.BuildRequest(id=44, buildsetid=20, builderid=77),
            fakedb.BuildRequest(id=45, buildsetid=20, builderid=77),
            fakedb.BuildRequestClaim(brid=45, masterid=88, claimed_at=1794253903),
            fakedb.BuildRequest(id=46, buildsetid=20, builderid=77, complete=1),
            fakedb.BuildRequest(id=47, buildsetid=21, builderid=77),
            fakedb.BuildRequest(id=48, buildsetid=22, builderid=77),
        ])
        self.handler = GitHubEventHandlerNoEdits(secret=None, strict=False, master=self.master, github_api_endpoint='https://api.github.com')
        for patcher in (
            patch.object(task, 'deferLater', lambda clock, delay, callable, *args, **kwargs: defer.succeed(callable(*args, **kwargs))),
            patch.object(log, 'msg', lambda *args, **kwargs: None),
            patch.object(GitHub, 'credentials', lambda user=None: ('webkit-ews', 'token')),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    @defer.inlineCallbacks
    def priorities(self):
        requests = yield self.master.db.buildrequests.getBuildRequests()
        return {request.buildrequestid: request.priority for request in requests}

    def membership(self, status_code, state=None):
        content = json.dumps(dict(state=state)).encode('utf-8') if state else b''
        return patch.object(TwistedAdditions, 'request', return_value=defer.succeed(TwistedAdditions.Response(status_code=status_code, content=content)))

    @defer.inlineCallbacks
    def test_incomplete_build_requests(self):
        brids = yield self.handler.incomplete_build_requests(self.PR_NUMBER, self.HEAD_SHA)
        self.assertEqual(brids, [44, 45])

    @defer.inlineCallbacks
    def test_update_priority(self):
        with self.membership(200, state='active'):
            yield self.handler.update_priority(self.PR_NUMBER, self.HEAD_SHA, ['ews-priority-high', 'ews-priority-urgent'], 'admin')
        self.assertEqual((yield self.priorities()), {44: 2, 45: 2, 46: 0, 47: 0, 48: 0})

    @defer.inlineCallbacks
    def test_update_priority_after_label_removed(self):
        yield self.master.db.buildrequests.set_build_requests_priority([44, 45], 2)
        with self.membership(200, state='active'):
            yield self.handler.update_priority(self.PR_NUMBER, self.HEAD_SHA, ['merge-queue'], 'admin')
        self.assertEqual((yield self.priorities()), {44: 0, 45: 0, 46: 0, 47: 0, 48: 0})

    @defer.inlineCallbacks
    def test_update_priority_not_admin(self):
        with self.membership(404):
            yield self.handler.update_priority(self.PR_NUMBER, self.HEAD_SHA, ['ews-priority-urgent'], 'contributor')
        self.assertEqual((yield self.priorities())[44], 0)

    @defer.inlineCallbacks
    def test_is_priority_admin(self):
        with self.membership(200, state='active') as request:
            self.assertTrue((yield self.handler.is_priority_admin('admin')))
        self.assertEqual(request.call_args.kwargs['url'], 'https://api.github.com/orgs/WebKit/teams/buildbot-administrators/memberships/admin')

    @defer.inlineCallbacks
    def test_is_priority_admin_not_a_member(self):
        with self.membership(404), patch.object(log, 'msg') as log_msg:
            self.assertFalse((yield self.handler.is_priority_admin('contributor')))
        log_msg.assert_not_called()

    @defer.inlineCallbacks
    def test_is_priority_admin_github_error(self):
        with self.membership(502), patch.object(log, 'msg') as log_msg:
            self.assertFalse((yield self.handler.is_priority_admin('admin')))
        log_msg.assert_called_once_with("Failed to check whether 'admin' is a member of buildbot-administrators: HTTP 502")

    @defer.inlineCallbacks
    def test_is_priority_admin_no_response(self):
        with patch.object(TwistedAdditions, 'request', return_value=defer.succeed(None)), patch.object(log, 'msg') as log_msg:
            self.assertFalse((yield self.handler.is_priority_admin('admin')))
        log_msg.assert_called_once_with("Failed to check whether 'admin' is a member of buildbot-administrators: no response")

    @defer.inlineCallbacks
    def test_is_priority_admin_pending_invitation(self):
        with self.membership(200, state='pending'):
            self.assertFalse((yield self.handler.is_priority_admin('invitee')))

    @defer.inlineCallbacks
    def test_is_priority_admin_no_user(self):
        with self.membership(200, state='active') as request:
            self.assertFalse((yield self.handler.is_priority_admin('')))
        request.assert_not_called()

    @defer.inlineCallbacks
    def test_priority_label_event(self):
        payload = dict(
            number=self.PR_NUMBER,
            action='labeled',
            label=dict(name='ews-priority-urgent'),
            sender=dict(login='admin'),
            pull_request=dict(state='open', labels=[dict(name='ews-priority-urgent')], head=dict(sha=self.HEAD_SHA)),
        )
        with patch.object(GitHubEventHandlerNoEdits, 'update_priority', return_value=defer.succeed(None)) as update_priority:
            result = yield self.handler.handle_pull_request(payload, 'pull_request')
        self.assertEqual(result, ([], 'git'))
        update_priority.assert_called_once_with(self.PR_NUMBER, self.HEAD_SHA, ['ews-priority-urgent'], 'admin')
