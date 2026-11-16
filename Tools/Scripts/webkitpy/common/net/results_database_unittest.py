# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import unittest

import requests
from unittest.mock import MagicMock, patch

from webkitpy.common.net import results_database


def _make_response(data, status_code=200):
    response = MagicMock()
    response.status_code = status_code
    response.json.return_value = data
    if status_code >= 400:
        response.raise_for_status.side_effect = requests.HTTPError(response=response)
    else:
        response.raise_for_status.return_value = None
    return response


class GetResultsSummaryTest(unittest.TestCase):

    def test_returns_parsed_json_on_success(self):
        payload = {'pass': 95.0, 'fail': 5.0}
        with patch.object(results_database._session, 'get', return_value=_make_response(payload)):
            result = results_database.get_results_summary('fast/css/foo.html')
        self.assertEqual(result, payload)

    def test_url_encodes_test_name(self):
        captured = {}

        def fake_get(url, params=None, timeout=None):
            captured['url'] = url
            return _make_response({'pass': 100.0})

        with patch.object(results_database._session, 'get', side_effect=fake_get):
            results_database.get_results_summary('fast/css/foo bar.html')

        self.assertIn('foo%20bar', captured['url'])

    def test_returns_empty_on_http_error(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({}, status_code=404)):
            result = results_database.get_results_summary('fast/css/foo.html')
        self.assertEqual(result, {})

    def test_returns_empty_on_network_error(self):
        with patch.object(results_database._session, 'get', side_effect=requests.ConnectionError()):
            result = results_database.get_results_summary('fast/css/foo.html')
        self.assertEqual(result, {})

    def test_returns_empty_on_exception(self):
        with patch.object(results_database._session, 'get', side_effect=Exception('unexpected')):
            result = results_database.get_results_summary('fast/css/foo.html')
        self.assertEqual(result, {})

    def test_uses_custom_suite(self):
        captured = {}

        def fake_get(url, params=None, timeout=None):
            captured['url'] = url
            return _make_response({'pass': 100.0})

        with patch.object(results_database._session, 'get', side_effect=fake_get):
            results_database.get_results_summary('SomeTest', suite='api-tests')

        self.assertIn('/api-tests/', captured['url'])

    def test_treats_any_2xx_as_success(self):
        payload = {'pass': 100.0}
        response = _make_response(payload, status_code=202)
        with patch.object(results_database._session, 'get', return_value=response):
            result = results_database.get_results_summary('fast/css/foo.html')
        self.assertEqual(result, payload)


class IsPreExistingFailureTest(unittest.TestCase):

    def test_true_when_pass_rate_below_threshold(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({'pass': 70.0})):
            self.assertTrue(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_true_when_pass_plus_warning_below_threshold(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({'pass': 60.0, 'warning': 15.0})):
            self.assertTrue(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_false_when_pass_rate_at_threshold(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({'pass': 80.0})):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_false_when_pass_rate_above_threshold(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({'pass': 95.0})):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_false_when_no_data_from_server(self):
        with patch.object(results_database._session, 'get', side_effect=requests.ConnectionError()):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_false_when_server_returns_error(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({}, status_code=404)):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_true_for_zero_pass_rate(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({'pass': 0.0, 'fail': 100.0})):
            self.assertTrue(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_respects_custom_threshold(self):
        payload = {'pass': 85.0}
        with patch.object(results_database._session, 'get', return_value=_make_response(payload)):
            self.assertTrue(results_database.is_pre_existing_failure('fast/css/foo.html', threshold=90))
        with patch.object(results_database._session, 'get', return_value=_make_response(payload)):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_missing_pass_key_defaults_to_100(self):
        with patch.object(results_database._session, 'get', return_value=_make_response({})):
            self.assertFalse(results_database.is_pre_existing_failure('fast/css/foo.html'))

    def test_uses_custom_suite(self):
        captured = {}

        def fake_get(url, params=None, timeout=None):
            captured['url'] = url
            return _make_response({'pass': 70.0})

        with patch.object(results_database._session, 'get', side_effect=fake_get):
            result = results_database.is_pre_existing_failure('SomeTest', suite='api-tests')

        self.assertIn('/api-tests/', captured['url'])
        self.assertTrue(result)
