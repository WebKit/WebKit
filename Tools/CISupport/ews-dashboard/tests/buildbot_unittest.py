# Copyright (C) 2026 Apple Inc. All rights reserved.
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

"""The Buildbot client's paging, log decoding and failure handling.

Every payload here is shaped the way ews-build.webkit.org shapes it: properties as [value, source]
pairs, logs delivered as chunks, and stream-type logs carrying a per-line o/e/h marker that is not
part of the log text.
"""

from __future__ import annotations

import http.client
import io
import json
import socket
import urllib.error
import unittest
from typing import Optional
from unittest import mock

from ews_dashboard import buildbot
from tests import fixtures

BUILDER_ID = 42

API_TEST_REPORT = {
    'Failed': [{'name': 'TestWebKitAPI.WKWebView.ClearAppHighlights', 'output': 'FAIL'}],
    'Timedout': [{'name': 'TestWebKitAPI.WKWebView.LoadInvalidURLRequest'}],
    'Crashed': [],
}


def _stream_marked(text: str, marker: str = 'o') -> str:
    """Buildbot prefixes every line of a stream-type log with o (stdout), e (stderr) or h (header)."""
    return ''.join(f'{marker}{line}\n' for line in text.splitlines())


class _Upstream:
    """Serves one queued JSON payload per urlopen call and records the URLs asked for.

    A fresh response object per call, since a body can only be read once.
    """

    def __init__(self, payloads: list) -> None:
        self.payloads = list(payloads)
        self.urls: list = []

    def __call__(self, request: object, timeout: Optional[int] = None) -> mock.MagicMock:
        self.urls.append(request.full_url)
        return _response(json.dumps(self.payloads.pop(0)).encode())


def _response(body: bytes) -> mock.MagicMock:
    response = mock.MagicMock()
    response.__enter__.return_value = io.BytesIO(body)
    response.__exit__.return_value = False
    return response


def _page(numbers: list, started_at: int = fixtures.DEFAULT_BUILD_TIME) -> dict:
    return {'builds': [fixtures.build(number=number, started_at=started_at) for number in numbers]}


def _logs(entries: list) -> dict:
    return {'logs': entries}


def _contents(chunks: list) -> dict:
    return {'logchunks': [{'firstline': index, 'content': chunk}
                          for index, chunk in enumerate(chunks)]}


class TestBuilds(unittest.TestCase):
    def setUp(self) -> None:
        self.client = buildbot.BuildbotClient(base_url='https://ews-build.webkit.org')

    def test_a_two_page_walk_asks_for_the_next_page_from_the_last_build_it_saw(self) -> None:
        newest = list(range(140, 140 - buildbot.PAGE_LIMIT, -1))
        upstream = _Upstream([_page(newest), _page([110, 109, 108]), _page([])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            numbers = [build['number'] for build in self.client.builds(BUILDER_ID)]
            self.assertEqual(numbers, newest + [110, 109, 108])
            self.assertNotIn('number__lt', upstream.urls[0])
            self.assertIn(f'number__lt={newest[-1]}', upstream.urls[1])
            self.assertIn(f'/api/v2/builders/{BUILDER_ID}/builds?', upstream.urls[1])

    def test_a_page_shorter_than_the_limit_does_not_end_the_walk(self) -> None:
        """This server truncates large responses, so a short page can be a cut-off one rather than
        the end of a builder's history."""
        upstream = _Upstream([_page([140, 139]), _page([138]), _page([])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            numbers = [build['number'] for build in self.client.builds(BUILDER_ID)]
            self.assertEqual(numbers, [140, 139, 138])
            self.assertEqual(len(upstream.urls), 3)

    def test_a_full_page_followed_by_an_empty_one_ends_the_walk(self) -> None:
        upstream = _Upstream([_page(list(range(140, 140 - buildbot.PAGE_LIMIT, -1))), _page([])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            self.assertEqual(len(list(self.client.builds(BUILDER_ID))), buildbot.PAGE_LIMIT)
            self.assertEqual(len(upstream.urls), 2)

    def test_a_build_older_than_since_stops_the_walk_without_asking_for_another_page(self) -> None:
        cutoff = fixtures.DEFAULT_BUILD_TIME - 500
        recent = [fixtures.build(number=number, started_at=fixtures.DEFAULT_BUILD_TIME)
                  for number in range(140, 120, -1)]
        older = [fixtures.build(number=number, started_at=cutoff - 1)
                 for number in range(120, 110, -1)]
        upstream = _Upstream([{'builds': recent + older}, _page([100])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            numbers = [build['number'] for build in self.client.builds(BUILDER_ID, since=cutoff)]
            self.assertEqual(numbers, [build['number'] for build in recent])
            self.assertEqual(len(upstream.urls), 1)

    def test_a_build_exactly_at_since_is_kept(self) -> None:
        upstream = _Upstream([_page([140], started_at=fixtures.DEFAULT_BUILD_TIME), _page([])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            self.assertEqual(
                [build['number'] for build in
                 self.client.builds(BUILDER_ID, since=fixtures.DEFAULT_BUILD_TIME)],
                [140],
            )

    def test_a_build_that_never_started_is_placed_by_the_time_it_finished(self) -> None:
        unstarted = fixtures.build(number=140, started_at=fixtures.DEFAULT_BUILD_TIME)
        unstarted['started_at'] = None
        upstream = _Upstream([{'builds': [unstarted]}])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            cutoff = unstarted['complete_at'] + 1
            self.assertEqual(list(self.client.builds(BUILDER_ID, since=cutoff)), [])

    def test_the_walk_asks_for_every_property_of_completed_builds_only(self) -> None:
        upstream = _Upstream([_page([140]), _page([])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            list(self.client.builds(BUILDER_ID))
            self.assertIn('complete=true', upstream.urls[0])
            self.assertIn('order=-number', upstream.urls[0])
            self.assertIn(f'limit={buildbot.PAGE_LIMIT}', upstream.urls[0])
            self.assertIn('property=%2A', upstream.urls[0])


class TestLogText(unittest.TestCase):
    def setUp(self) -> None:
        self.client = buildbot.BuildbotClient(base_url='https://ews-build.webkit.org')

    def test_a_stream_logs_per_line_marker_is_stripped_so_the_json_body_parses(self) -> None:
        body = _stream_marked(json.dumps(API_TEST_REPORT, indent=1))
        upstream = _Upstream([
            _logs([{'logid': 4321, 'name': 'json', 'slug': 'json', 'type': 's'}]),
            _contents([body[:40], body[40:]]),
        ])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            text = self.client.log_text(step_id=98765, log_name='json')
            self.assertEqual(json.loads(text), API_TEST_REPORT)
            self.assertNotIn('\n', text)
            self.assertIn('/api/v2/steps/98765/logs', upstream.urls[0])
            self.assertIn('/api/v2/logs/4321/contents', upstream.urls[1])

    def test_a_text_log_is_returned_verbatim_because_it_carries_no_markers(self) -> None:
        body = 'Tests that timed out:\n  TestWebKitAPI.WKWebView.LoadInvalidURLRequest\n'
        upstream = _Upstream([
            _logs([{'logid': 4322, 'name': 'json', 'slug': 'json', 'type': 't'}]),
            _contents([body]),
        ])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            self.assertEqual(self.client.log_text(step_id=98765, log_name='json'), body)

    def test_a_step_without_the_named_log_answers_none_without_fetching_contents(self) -> None:
        upstream = _Upstream([_logs([{'logid': 4323, 'name': 'stdio', 'slug': 'stdio',
                                      'type': 's'}])])
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen', side_effect=upstream):
            self.assertIsNone(self.client.log_text(step_id=98765, log_name='json'))
            self.assertEqual(len(upstream.urls), 1)


class TestBuilderId(unittest.TestCase):
    def setUp(self) -> None:
        self.client = buildbot.BuildbotClient(base_url='https://ews-build.webkit.org')
        self.builders = {'builders': [
            {'name': fixtures.LAYOUT_BUILDER, 'builderid': BUILDER_ID, 'masterids': [1]},
            {'name': fixtures.GTK_BUILDER, 'builderid': 43, 'masterids': [1]},
        ]}

    def test_a_builder_is_found_by_the_name_ews_shows_on_a_pull_request(self) -> None:
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=_Upstream([self.builders])):
            self.assertEqual(self.client.builder_id(fixtures.LAYOUT_BUILDER), BUILDER_ID)

    def test_a_builder_buildbot_does_not_expose_is_rejected_rather_than_guessed_at(self) -> None:
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=_Upstream([self.builders])):
            with self.assertRaises(ValueError) as raised:
                self.client.builder_id('macOS-Ventura-Release-WK2-Tests-EWS')
            self.assertIn('macOS-Ventura-Release-WK2-Tests-EWS', str(raised.exception))


class TestTransport(unittest.TestCase):
    def setUp(self) -> None:
        self.client = buildbot.BuildbotClient(base_url='https://ews-build.webkit.org')

    def test_a_body_that_is_not_json_is_retried_and_then_raised(self) -> None:
        gateway_error = _response(b'<html><body>502 Bad Gateway</body></html>')
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        return_value=gateway_error) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            with self.assertRaises(json.JSONDecodeError):
                self.client.builders()
            self.assertEqual(urlopen.call_count, buildbot.RETRY_ATTEMPTS)

    def test_a_server_error_is_retried_and_then_raised(self) -> None:
        error = urllib.error.HTTPError('https://ews-build.webkit.org', 500, 'no', {}, None)
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=error) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            with self.assertRaises(urllib.error.HTTPError):
                self.client.builders()
            self.assertEqual(urlopen.call_count, buildbot.RETRY_ATTEMPTS)

    def test_a_rejected_request_is_raised_at_once_because_waiting_would_not_change_it(self) -> None:
        error = urllib.error.HTTPError('https://ews-build.webkit.org', 404, 'no', {}, None)
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=error) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep') as sleep:
            with self.assertRaises(urllib.error.HTTPError):
                self.client.builders()
            self.assertEqual(urlopen.call_count, 1)
            self.assertEqual(sleep.call_count, 0)

    def test_a_throttled_request_is_retried_because_waiting_is_what_it_asked_for(self) -> None:
        error = urllib.error.HTTPError('https://ews-build.webkit.org', 429, 'slow down', {}, None)
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=error) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            with self.assertRaises(urllib.error.HTTPError):
                self.client.builders()
            self.assertEqual(urlopen.call_count, buildbot.RETRY_ATTEMPTS)

    def test_an_incomplete_read_is_retried_and_the_next_attempt_answers(self) -> None:
        payload = {'builders': [{'name': fixtures.LAYOUT_BUILDER, 'builderid': BUILDER_ID}]}
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=[http.client.IncompleteRead(b'{"builders": [{"nam'),
                                     _response(json.dumps(payload).encode())]) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            self.assertEqual(self.client.builders(),
                             [{'name': fixtures.LAYOUT_BUILDER, 'id': BUILDER_ID}])
            self.assertEqual(urlopen.call_count, 2)

    def test_a_read_that_times_out_is_retried_and_the_next_attempt_answers(self) -> None:
        """On Python 3.9 `socket.timeout` is an OSError that is not a TimeoutError, so listing only
        TimeoutError let a timed-out read past the retry loop and out of every caller's except."""
        payload = {'builders': [{'name': fixtures.LAYOUT_BUILDER, 'builderid': BUILDER_ID}]}
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=[socket.timeout('The read operation timed out'),
                                     _response(json.dumps(payload).encode())]) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            self.assertEqual(self.client.builders(),
                             [{'name': fixtures.LAYOUT_BUILDER, 'id': BUILDER_ID}])
            self.assertEqual(urlopen.call_count, 2)

    def test_a_read_that_keeps_timing_out_is_retried_and_then_raised(self) -> None:
        with mock.patch('ews_dashboard.buildbot.urllib.request.urlopen',
                        side_effect=socket.timeout('The read operation timed out')) as urlopen, \
                mock.patch('ews_dashboard.buildbot.time.sleep'):
            with self.assertRaises(socket.timeout):
                self.client.builders()
            self.assertEqual(urlopen.call_count, buildbot.RETRY_ATTEMPTS)
