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
import time

from fakeredis import FakeStrictRedis
from redis import StrictRedis
from resultsdbpy.controller.api_routes import APIRoutes
from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.ews_controller import EWSController
from resultsdbpy.flask_support.flask_testcase import FlaskTestCase
from resultsdbpy.model.cassandra_context import CassandraContext
from resultsdbpy.model.mock_model_factory import MockModelFactory
from resultsdbpy.model.mock_cassandra_context import MockCassandraContext
from resultsdbpy.model.wait_for_docker_test_case import WaitForDockerTestCase
from resultsdbpy.model.configuration_context_unittest import ConfigurationContextTest
from urllib.parse import quote


class EWSControllerTest(FlaskTestCase, WaitForDockerTestCase):
    KEYSPACE = 'ews_controller_test_keyspace'
    EWS_UPLOAD_ENDPOINT = 'api/upload/ews'
    EWS_RESULTS_ENDPOINT = 'api/results-ews'
    EWS_TESTS_ENDPOINT = 'api/results-ews/tests'
    COMMITS_ENDPOINT = 'api/commits'
    DEFAULT_COMMITS = [
        {'repository_id': 'safari', 'hash': 'd8bce26fa65c6fc8f39c17927abb77f69fab82fc'},
        {'repository_id': 'webkit', 'hash': '75eaef1c9242f92a8d7694e8ccd310f69cf9683b'},
    ]
    DEFAULT_TEST_RESULTS = dict(
        results={
            'fast/encoding/css-cached-bom.html': {'report': 'REGRESSION', 'expected': 'PASS', 'actual': 'TEXT'},
            'fast/encoding/css-charset.html': {'report': 'FLAKY', 'expected': 'PASS', 'actual': 'TIMEOUT PASS', 'has_stderr': True},
            'fast/encoding/css-link-charset.html': {'report': 'MISSING', 'expected': 'PASS', 'actual': 'MISSING', 'is_missing_text': True},
        },
    )
    REGRESSION_TEST = 'fast/encoding/css-cached-bom.html'

    @classmethod
    def setup_webserver(cls, app, redis=StrictRedis, cassandra=CassandraContext):
        with MockModelFactory.safari(), MockModelFactory.webkit():
            cassandra.drop_keyspace(keyspace=cls.KEYSPACE)
            model = MockModelFactory.create(redis=redis(), cassandra=cassandra(keyspace=cls.KEYSPACE, create_keyspace=True))
            app.register_blueprint(APIRoutes(model))

    @classmethod
    def upload_ews_results(cls, client, configuration=None, suite=None, commits=None, test_results=None, flaky_type=None, details=None, timestamp=None):
        if configuration is None:
            configuration = ConfigurationContextTest.CONFIGURATIONS[0]
        data = {
            'configuration': json.dumps(configuration, cls=Configuration.Encoder),
            'suite': suite if suite is not None else 'layout-tests',
            'commits': commits if commits is not None else cls.DEFAULT_COMMITS,
            'test_results': test_results if test_results is not None else cls.DEFAULT_TEST_RESULTS,
        }
        if flaky_type is not None:
            data['flaky_type'] = flaky_type
        if details is not None:
            data['details'] = details
        if timestamp is not None:
            data['timestamp'] = timestamp
        return client.post(f'{cls.URL}/{cls.EWS_UPLOAD_ENDPOINT}', data=json.dumps(data))

    @classmethod
    def find_ews_results(cls, client, configuration=None, suite='layout-tests', tests=..., flaky=None, recent=None, after_time=None, before_time=None):
        if configuration is None:
            configuration = ConfigurationContextTest.CONFIGURATIONS[0]
        if tests is ...:
            tests = [cls.REGRESSION_TEST]
        params = {
            'suite': suite, 'flaky': flaky, 'recent': recent,
            'after_time': after_time, 'before_time': before_time,
            'platform': configuration.platform, 'style': configuration.style, 'flavor': configuration.flavor,
        }
        query = '&'.join(
            [
                f'{key}={str(value).lower() if isinstance(value, bool) else value}'
                for key, value in params.items() if value is not None
            ] + [f'tests={quote(name, safe="")}' for name in tests or []]
        )
        return client.get(f'{cls.URL}/{cls.EWS_RESULTS_ENDPOINT}?{query}')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_and_find(self, client, **kwargs):
        response = self.upload_ews_results(
            client,
            details={
                'remote': 'github.com/WebKit/WebKit', 'pr_number': 12345, 'commit_hash': 'abc123def456',
                'build_number': 678, 'link': 'https://ews.example.com/build/1',
            },
        )
        self.assertEqual(response.status_code, 200, response.json())

        response = self.find_ews_results(client)
        self.assertEqual(response.status_code, 200, response.json())
        entries = response.json()[0]['results']
        self.assertGreater(len(entries), 0)

        result = entries[0]
        self.assertEqual(result['result']['report'], 'REGRESSION')
        self.assertEqual(result['result']['actual'], 'TEXT')
        self.assertEqual(result['result']['expected'], 'PASS')

        details = result.get('details') or {}
        self.assertEqual(details.get('remote'), 'github.com/WebKit/WebKit')
        self.assertEqual(details.get('pr_number'), 12345)
        self.assertEqual(details.get('commit_hash'), 'abc123def456')
        self.assertEqual(details.get('build_number'), 678)
        self.assertEqual(details.get('link'), 'https://ews.example.com/build/1')

        response = self.find_ews_results(client, tests=['fast/encoding/css-charset.html'])
        self.assertEqual(response.status_code, 200, response.json())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_flaky_upload_and_find(self, client, **kwargs):
        response = self.upload_ews_results(
            client,
            test_results={'results': {
                self.REGRESSION_TEST: {'report': 'FLAKY', 'expected': 'PASS', 'actual': 'TIMEOUT PASS'},
            }},
            flaky_type='BetweenStepsDirtyTree',
            details={'pr_number': 12345},
        )
        self.assertEqual(response.status_code, 200, response.json())

        # The flaky test we reported is queryable with its flaky_type.
        response = self.find_ews_results(client, flaky=True)
        self.assertEqual(response.status_code, 200, response.json())
        result = response.json()[0]['results'][0]
        self.assertEqual(result['result']['actual'], 'TIMEOUT PASS')
        self.assertEqual(result['flaky_type'], 'BetweenStepsDirtyTree')
        self.assertEqual((result.get('details') or {}).get('pr_number'), 12345)

        # A test that wasn't reported flaky is not in the flaky table.
        response = self.find_ews_results(client, tests=['fast/encoding/css-charset.html'], flaky=True)
        self.assertEqual(response.status_code, 404)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_body_is_not_an_object(self, client, **kwargs):
        # Every field check below reads data.get(...), so a non-object body would raise
        # AttributeError, which is neither a TypeError nor a ValueError, and answer 500.
        for body in ('[]', '5', '"a string"', 'null', 'not json at all'):
            response = client.post(f'{self.URL}/{self.EWS_UPLOAD_ENDPOINT}', data=body)
            self.assertEqual(response.status_code, 400, f'{body} gave {response.status_code}')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_missing_suite(self, client, **kwargs):
        response = client.post(f'{self.URL}/{self.EWS_UPLOAD_ENDPOINT}', data=json.dumps({
            'configuration': json.dumps(ConfigurationContextTest.CONFIGURATIONS[0], cls=Configuration.Encoder),
            'commits': [{'repository_id': 'webkit', 'id': '6'}],
            'test_results': {'results': {}},
        }))
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Invalid suite')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_malformed_columns(self, client, **kwargs):
        # These reach cqlengine columns or a commit lookup directly, and neither a ValidationError nor
        # an AttributeError is a TypeError or a ValueError, so anything unchecked here answers 500 with
        # the raw exception instead of 400.
        for field, value in (
            ('timestamp', 'yesterday'), ('timestamp', 0), ('timestamp', 1e300), ('timestamp', time.time() + 86400),
            ('flaky_type', 5), ('details', 'a string'),
            ('commits', 'webkit@main'), ('commits', {'repository_id': 'webkit'}), ('commits', ['webkit@main']),
            ('test_results', {'results': {}}), ('test_results', {'no_results': {}}),
            ('test_results', {'results': ['fast/a.html']}), ('test_results', {'results': {'fast/a.html': 'TEXT'}}),
        ):
            response = self.upload_ews_results(client, **{field: value})
            self.assertEqual(response.status_code, 400, f'{field}={value!r} gave {response.status_code}')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_missing_commits(self, client, **kwargs):
        response = self.upload_ews_results(client, commits=[])
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Invalid commits')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_missing_test_results(self, client, **kwargs):
        response = self.upload_ews_results(client, test_results={})
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'Invalid test results')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_no_results(self, client, **kwargs):
        response = self.find_ews_results(client)
        self.assertEqual(response.status_code, 404)
        self.assertEqual(response.json()['description'], 'No EWS results matching the specified criteria')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_a_batch_names_the_test_each_entry_answers_for(self, client, **kwargs):
        """A client batching its queries can only match answers to questions by the test field, and
        can only tell a test with no history from one it never asked about by its absence."""
        response = self.upload_ews_results(client)
        self.assertEqual(response.status_code, 200, response.json())

        reported = sorted(self.DEFAULT_TEST_RESULTS['results'])
        self.assertGreater(len(reported), 1)
        never_reported = 'fast/never/reported.html'

        response = self.find_ews_results(client, tests=reported + [never_reported])
        self.assertEqual(response.status_code, 200, response.json())

        by_test = {}
        for entry in response.json():
            by_test.setdefault(entry['test'], []).extend(entry['results'])

        self.assertEqual(sorted(by_test), reported)
        self.assertNotIn(never_reported, by_test)
        for test in reported:
            self.assertTrue(by_test[test], test)
            for result in by_test[test]:
                self.assertEqual(result['result'], self.DEFAULT_TEST_RESULTS['results'][test])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_rejects_a_commit_range(self, client, **kwargs):
        # Results are clustered by start_time, so a commit range could only be honored as a
        # client-side filter over rows Cassandra already truncated with LIMIT. Reject it instead.
        for param in ('ref=6', 'uuid=153802947300000', 'timestamp=1538029473', 'begin=5', 'end=7'):
            response = client.get(f'{self.URL}/{self.EWS_RESULTS_ENDPOINT}?suite=layout-tests&tests={self.REGRESSION_TEST}&{param}')
            self.assertEqual(response.status_code, 400, f'{param} gave {response.status_code}')
            self.assertIn('not supported in queries by this endpoint', response.json()['description'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_without_test(self, client, **kwargs):
        response = self.find_ews_results(client, tests=None)
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No valid test specified')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_a_test_whose_name_needs_escaping(self, client, **kwargs):
        """WPT names carry their own query string. Unescaped, everything from the first & on is read
        as further query parameters, so the name arrives truncated and the request is rejected."""
        test = 'imported/w3c/web-platform-tests/webrtc/RTCIceTransport.html?type=relay&mode=full'
        response = self.upload_ews_results(client, test_results={'results': {
            test: {'report': 'REGRESSION', 'expected': 'PASS', 'actual': 'TEXT'},
        }})
        self.assertEqual(response.status_code, 200, response.json())

        response = self.find_ews_results(client, tests=[test])
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual([entry['test'] for entry in response.json()], [test])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_ignores_blank_and_repeated_names(self, client, **kwargs):
        """A blank repeated value must not reject a batch that names a real test, nor reach the
        database as an empty test name, and a name repeated twice must not cost two queries."""
        self.assertEqual(self.upload_ews_results(client).status_code, 200)

        for tests in (['', self.REGRESSION_TEST], [self.REGRESSION_TEST, ''], [self.REGRESSION_TEST] * 2):
            response = self.find_ews_results(client, tests=tests)
            self.assertEqual(response.status_code, 200, f'{tests} gave {response.status_code}')
            self.assertEqual([entry['test'] for entry in response.json()], [self.REGRESSION_TEST], tests)

        response = self.find_ews_results(client, tests=['', ''])
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No valid test specified')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_find_rejects_an_oversized_batch(self, client, **kwargs):
        """The limit is per test, so an unbounded batch is an unbounded response and one query per test."""
        cap = EWSController.MAX_TESTS_PER_QUERY

        response = self.find_ews_results(client, tests=[f'fast/{index}.html' for index in range(cap)])
        self.assertEqual(response.status_code, 404, response.json())

        response = self.find_ews_results(client, tests=[f'fast/{index}.html' for index in range(cap + 1)])
        self.assertEqual(response.status_code, 400, response.json())
        self.assertIn(f'Cannot query more than {cap} tests at a time', response.json()['description'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_omitting_optional_fields(self, client, **kwargs):
        response = self.upload_ews_results(client)
        self.assertEqual(response.status_code, 200, response.json())
        response = self.find_ews_results(client)
        self.assertEqual(response.status_code, 200, response.json())
        result = response.json()[0]['results'][0]
        self.assertEqual(result['result']['actual'], 'TEXT')
        self.assertIsNone(result.get('details'))
        self.assertNotIn('flaky_type', result)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_non_string_suite(self, client, **kwargs):
        response = self.upload_ews_results(client, suite=123)
        self.assertEqual(response.status_code, 400, response.json())

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_preserves_commit_metadata(self, client, **kwargs):
        # EWS knows the hash, branch and timestamp but never the author or message, and uploading
        # must not drop what a post-commit upload already stored.
        with MockModelFactory.webkit() as webkit:
            commit = webkit.commits['main'][-1]

        response = self.upload_ews_results(client, commits=[{
            'repository_id': 'webkit', 'hash': commit.hash,
            'branch': 'main', 'timestamp': commit.timestamp, 'identifier': str(commit),
        }])
        self.assertEqual(response.status_code, 200, response.json())

        response = client.get(f'{self.URL}/{self.COMMITS_ENDPOINT}?repository_id=webkit&ref={commit.hash}')
        self.assertEqual(response.status_code, 200, response.json())
        found = response.json()[0]
        self.assertIsNotNone(found.get('author'))
        self.assertTrue(found.get('message', '').startswith(commit.message))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_registers_an_unsynced_commit(self, client, **kwargs):
        hash = 'e1c8f9d3a24b5670bd91a2f4e6c8037b5da91e42'
        response = self.upload_ews_results(client, commits=[{
            'repository_id': 'webkit', 'hash': hash,
            'branch': 'main', 'timestamp': int(time.time()), 'identifier': '999999@main',
        }])
        self.assertEqual(response.status_code, 200, response.json())

        response = client.get(f'{self.URL}/{self.COMMITS_ENDPOINT}?repository_id=webkit&ref={hash}')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json()[0]['hash'], hash)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_upload_reports_the_tests_it_stored(self, client, **kwargs):
        response = self.upload_ews_results(client)
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), {
            'status': 'ok', 'tests': sorted(self.DEFAULT_TEST_RESULTS['results']),
        })

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_invalid_upload_partial_configuration(self, client, **kwargs):
        # A configuration missing a required member cannot be written, so the upload is refused
        # rather than answered with a success the caller would believe.
        response = self.upload_ews_results(client, configuration=Configuration(platform='mac', style='release'))
        self.assertEqual(response.status_code, 400, response.json())
        self.assertIn('partial configuration', response.json()['description'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_time_window_query(self, client, **kwargs):
        HOUR = 60 * 60
        now = int(time.time())
        recent, day_old, old = now - HOUR, now - 30 * HOUR, now - 72 * HOUR
        for timestamp in (recent, day_old, old):
            response = self.upload_ews_results(
                client,
                test_results={'results': {self.REGRESSION_TEST: {'report': 'REGRESSION', 'expected': 'PASS', 'actual': 'TEXT'}}},
                timestamp=timestamp,
            )
            self.assertEqual(response.status_code, 200, response.json())

        def start_times_after(cutoff):
            response = self.find_ews_results(client, recent=False, after_time=cutoff)
            self.assertEqual(response.status_code, 200, response.json())
            return {entry['start_time'] for config in response.json() for entry in config['results']}

        self.assertEqual(start_times_after(now - 24 * HOUR), {recent})
        self.assertEqual(start_times_after(now - 48 * HOUR), {recent, day_old})
        self.assertEqual(start_times_after(now - 96 * HOUR), {recent, day_old, old})

        # A window that ends before every upload yields no results.
        response = self.find_ews_results(client, recent=False, before_time=old - HOUR)
        self.assertEqual(response.status_code, 404)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests(self, client, **kwargs):
        self.assertEqual(self.upload_ews_results(client).status_code, 200)

        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), sorted(self.DEFAULT_TEST_RESULTS['results']))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_flaky(self, client, **kwargs):
        self.assertEqual(self.upload_ews_results(client, flaky_type='WithinStepDirtyTree').status_code, 200)

        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests&flaky=true')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), sorted(self.DEFAULT_TEST_RESULTS['results']))
        # The failed-table index must stay empty when everything was reported flaky.
        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests')
        self.assertEqual(response.json(), [])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_prefix(self, client, **kwargs):
        self.assertEqual(self.upload_ews_results(client).status_code, 200)

        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests&prefixes=fast/encoding/css-c')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), ['fast/encoding/css-cached-bom.html', 'fast/encoding/css-charset.html'])

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_honours_the_limit_across_prefixes(self, client, **kwargs):
        # Each prefix is a separate query, so the limit has to be spent down rather than reapplied.
        # cqlengine reads a limit of 0 as no limit, so exhausting it must stop the loop.
        self.assertEqual(self.upload_ews_results(client).status_code, 200)

        prefixes = '&'.join(f'prefixes={name}' for name in sorted(self.DEFAULT_TEST_RESULTS['results']))
        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests&limit=2&{prefixes}')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(len(response.json()), 2)

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_percent_encoded_name(self, client, **kwargs):
        # A '?' in a test name is percent-encoded by the client and decoded by Flask before we see it.
        test = 'http/tests/cache/disk-cache/disk-cache-validation.html?bar'
        results = {test: {'report': 'REGRESSION', 'expected': 'PASS', 'actual': 'TEXT'}}
        self.assertEqual(self.upload_ews_results(client, test_results=dict(results=results)).status_code, 200)

        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests&prefixes={quote(test, safe="")}')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), list(results))

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_without_suite(self, client, **kwargs):
        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}')
        self.assertEqual(response.status_code, 400)
        self.assertEqual(response.json()['description'], 'No valid test suite specified')

    @WaitForDockerTestCase.mock_if_no_docker(mock_redis=FakeStrictRedis, mock_cassandra=MockCassandraContext)
    @FlaskTestCase.run_with_webserver()
    def test_list_tests_empty(self, client, **kwargs):
        response = client.get(f'{self.URL}/{self.EWS_TESTS_ENDPOINT}?suite=layout-tests')
        self.assertEqual(response.status_code, 200, response.json())
        self.assertEqual(response.json(), [])
