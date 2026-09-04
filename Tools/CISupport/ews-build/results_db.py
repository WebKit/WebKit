#!/usr/bin/env python3

# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

import argparse
import os
import sys
import time
import urllib.parse

from collections.abc import Callable, Generator, Iterable
from dataclasses import asdict, dataclass, field
from typing import Any, Optional

from .twisted_additions import TwistedAdditions
from .utils import load_password, get_custom_suffix
from twisted.internet import defer, reactor


@dataclass
class FlakyVerdict:
    """Whether the results database can account for a test failing, and what says so."""
    flaky_type: str = None
    build_urls: set = field(default_factory=set)
    pr_numbers: set = field(default_factory=set)
    authors: set = field(default_factory=set)
    request_failed: bool = False
    intra_build_evidence: bool = False

    @property
    def is_flaky(self):
        return self.flaky_type is not None

    @property
    def evidence(self):
        """Whichever sets the rule that fired populated, sorted for a stable log line."""
        return {
            name: sorted(value)
            for name, value in (
                ('build_urls', self.build_urls),
                ('pr_numbers', self.pr_numbers),
                ('authors', self.authors),
            ) if value
        }


@dataclass
class EWSRowDetails:
    """A row's `details` blob: build provenance the results database stores but cannot be queried on."""
    worker: Optional[str] = None
    remote: Optional[str] = None
    pr_number: Optional[int] = None
    commit_hash: Optional[str] = None
    retry_count: int = 0
    authors: list[str] = field(default_factory=list)
    build_url: Optional[str] = None
    stage: Optional[str] = None

    @classmethod
    def from_json(cls, data: Optional[dict]) -> 'EWSRowDetails':
        data = data or {}
        return cls(
            worker=data.get('worker'),
            remote=data.get('remote'),
            pr_number=data.get('pr_number'),
            commit_hash=data.get('commit_hash'),
            retry_count=data.get('retry_count') or 0,
            authors=[author for author in (data.get('authors') or []) if author],
            build_url=data.get('build_url'),
            stage=data.get('stage'),
        )

    def to_json(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class EWSRow:
    """One row the results database has recorded for one test in one configuration."""
    uuid: int = 0
    start_time: int = 0
    # Whatever the suite that ran the test reports, which differs per suite.
    result: dict[str, Any] = field(default_factory=dict)
    flaky_type: Optional[str] = None
    details: EWSRowDetails = field(default_factory=EWSRowDetails)

    @classmethod
    def from_json(cls, data: dict) -> 'EWSRow':
        return cls(
            uuid=data.get('uuid') or 0,
            start_time=data.get('start_time') or 0,
            result=data.get('result') or {},
            flaky_type=data.get('flaky_type'),
            details=EWSRowDetails.from_json(data.get('details')),
        )


@dataclass
class EWSPayload:
    """What EWS uploads about one run. The api_key is deliberately absent: this object is logged, and
    a field would put the live key in every __repr__ and asdict of it."""
    suite: str
    configuration: dict
    commits: list[dict]
    results: dict
    timestamp: int
    details: EWSRowDetails
    flaky_type: Optional[str] = None

    def to_json(self) -> dict[str, Any]:
        return {
            'suite': self.suite,
            'flaky_type': self.flaky_type,
            'configuration': self.configuration,
            'commits': self.commits,
            'test_results': {'results': self.results},
            'timestamp': self.timestamp,
            'details': self.details.to_json(),
        }


EWSRowByTest = dict[str, list[EWSRow]]
EWSRowByType = dict[str, list[EWSRow]]


class ResultsDatabase(object):
    # Default to dev if not prod since resultsDB doesn't have a uat instance
    custom_suffix = '' if get_custom_suffix() == '' else '-dev'
    HOSTNAME = load_password('RESULTS_SERVER_HOST', default=f'https://results.webkit{custom_suffix}.org').rstrip('/')

    # TODO: Support more suites (Note, the API we're talking to already does)
    DEFAULT_SUITE = 'layout-tests'
    SUITES = ('layout-tests', 'api-tests', 'safer-cpp-checks', 'javascriptcore-tests')
    PERCENT_THRESHOLD = 10
    PERCENT_SUCCESS_RATE_FOR_PRE_EXISTING_FAILURE = 80
    CONFIGURATION_KEYS = [
        'architecture',
        'platform',
        'is_simulator',
        'version',
        'flavor',
        'style',
        'model',
        'version_name',
        'sdk',
    ]

    PRS_FOR_DIRTY_TREE_FLAKE = 2
    AUTHORS_FOR_DIRTY_TREE_FLAKE = 2
    BUILDS_FOR_CLEAN_TREE_FLAKE = 2
    PRS_FOR_BETWEEN_BUILD_FLAKE = 3
    AUTHORS_FOR_BETWEEN_BUILD_FLAKE = 2

    TESTS_PER_FLAKY_QUERY = 20
    FLAKY_QUERY_LIMIT = 100
    FLAKY_QUERY_TIMEOUT_SECONDS = 30
    FLAKY_WINDOW_SECONDS = 3 * 24 * 60 * 60

    WITHIN_STEP_CLEAN_TREE = 'WithinStepCleanTree'
    WITHIN_STEP_DIRTY_TREE = 'WithinStepDirtyTree'
    BETWEEN_STEPS_DIRTY_TREE = 'BetweenStepsDirtyTree'
    FAILED_ROWS = 'Failed'
    WITHIN_BUILD_ROWS = (WITHIN_STEP_CLEAN_TREE, WITHIN_STEP_DIRTY_TREE, BETWEEN_STEPS_DIRTY_TREE)

    # What the read path concludes, which is what a caller decides to act on.
    CLEAN_TREE_VERDICT = 'CleanTree'
    DIRTY_TREE_VERDICT = 'DirtyTree'
    BETWEEN_BUILDS_VERDICT = 'BetweenBuilds'

    @classmethod
    def platform_for_query(cls, platform):
        if platform.lower() in ('gtk', 'wpe'):
            return platform.upper()
        return platform

    @classmethod
    @defer.inlineCallbacks
    def make_request(cls, endpoint, suite=None, test=None, commit=None, configuration=None, logger=None):
        logger = logger or (lambda log: None)
        params = dict()
        suite = suite or cls.DEFAULT_SUITE
        if suite not in cls.SUITES:
            logger(f"'{suite}' is not a valid suite name\n")
            defer.returnValue({})
            return
        if not configuration:
            configuration = {}
        for key, value in configuration.items():
            if key not in cls.CONFIGURATION_KEYS:
                logger(f"'{key}' is not a valid configuration key\n")
            params[key] = value
        if commit:
            params['ref'] = commit

        url = f"{cls.HOSTNAME}/api/{endpoint}/{urllib.parse.quote(suite)}{f'/{urllib.parse.quote(test)}' if test else ''}"
        response = yield TwistedAdditions.request(url, params=params, logger=logger)

        if not response:
            logger(f'No response from {url}\n')
            defer.returnValue(None)
            return
        if response.status_code == 200:
            defer.returnValue(response.json())
            return
        logger(f'Failed to query {url} (status {response.status_code})\n{response.content}\n')
        defer.returnValue(None)

    @classmethod
    @defer.inlineCallbacks
    def get_results_summary(cls, test, commit=None, configuration=None, logger=None, suite=None):
        if not test:
            logger('Test name not provided\n')
            defer.returnValue({})
            return
        response = yield cls.make_request('results-summary', suite=suite, test=test, commit=commit, configuration=configuration, logger=logger)
        return defer.returnValue(response)

    @classmethod
    @defer.inlineCallbacks
    def get_results(cls, suite, test=None, commit=None, configuration=None, logger=None):
        response = yield cls.make_request('results', suite=suite, test=test, commit=commit, configuration=configuration, logger=logger)
        return defer.returnValue(response)

    @classmethod
    @defer.inlineCallbacks
    def is_test_pre_existing_failure(cls, test, commit=None, configuration=None, suite=None):
        logs = []
        data = yield cls.get_results_summary(test, commit, configuration, logger=lambda log: logs.append(log), suite=suite)
        request_failed = data is None
        data = data or {}
        pass_rate = data.get('pass', 100) + data.get('warning', 0)
        is_existing_failure = (pass_rate <= cls.PERCENT_SUCCESS_RATE_FOR_PRE_EXISTING_FAILURE)
        output = {
            'is_existing_failure': is_existing_failure,
            'pass_rate': data.get('pass', 'Unknown'),
            'raw_data': data,
            'logs': ''.join(logs),
            'request_failed': request_failed,
        }
        defer.returnValue(output)

    @classmethod
    @defer.inlineCallbacks
    def does_result_match(cls, test, result_type=None, commit=None, configuration=None, suite=None):
        logs = []
        data = yield cls.get_results(suite, test, commit, configuration, logger=lambda log: logs.append(log))
        if not data:
            defer.returnValue(None)
            return

        results = data[0].get('results')[0]
        actual = results.get('actual')
        does_result_match = actual == result_type
        output = {
            'does_result_match': does_result_match,
            'raw_data': data,
            'logs': ''.join(logs),
        }
        defer.returnValue(output)

    @classmethod
    @defer.inlineCallbacks
    def is_test_expected_to(cls, test, result_type=None, commit=None, configuration=None, logger=None, suite=None):
        logger = logger or (lambda log: None)
        has_commit = False
        if commit:
            has_commit = yield cls.has_commit(commit=commit)
            if not has_commit:
                logger(f"'{commit}' is not registered on '{cls.HOSTNAME}'\n")

        data = yield cls.get_results_summary(
            test, configuration=configuration, logger=logger,
            commit=commit if has_commit else None,
            suite=suite,
        )
        logger(f'{test}\n')
        for key, value in (data or dict()).items():
            if not value:
                continue
            logger(f'    {key}: {value}%\n')
        if not data:
            logger('    No historic data found for query\n')
        if not data:
            return defer.returnValue(-1)
        if result_type:
            return defer.returnValue(data.get(result_type.lower(), 0) > cls.PERCENT_THRESHOLD)
        return defer.returnValue(100 - (data.get('pass', 0) + data.get('warning', 0)) > cls.PERCENT_THRESHOLD)

    @classmethod
    @defer.inlineCallbacks
    def has_commit(cls, commit, logger=None):
        response = yield TwistedAdditions.request(f'{cls.HOSTNAME}/api/commits', params=dict(ref=commit), logger=logger)
        defer.returnValue(response and response.status_code == 200)

    @classmethod
    @defer.inlineCallbacks
    def report_ews(cls, payload: EWSPayload, logger: Callable[[str], None] = lambda _: None) -> Generator[Any, Any, bool]:
        if not payload.results:
            return False

        document = payload.to_json()
        body = {**document, 'api_key': os.environ.get('RESULTS_SERVER_API_KEY', '')}

        url = f'{cls.HOSTNAME}/api/upload/ews'
        label = f'{payload.flaky_type} flaky tests' if payload.flaky_type else 'failed tests'
        logger(f"Reporting {label} to {url}: {', '.join(sorted(payload.results))}\n")

        response = yield TwistedAdditions.request(url, type=b'POST', json=body, logger=logger)
        if not response:
            logger(f'No response from {url}, so {label} were not reported\n')
            return False

        if response.status_code != 200:
            logger(f'Failed to report {label} (status {response.status_code})\n{response.content}\n')
            return False

        try:
            stored = (response.json() or {}).get('tests')
        except ValueError:
            logger(f'{url} answered 200 with a body that is not JSON\n{response.content}\n')
            return False

        if stored is None:
            logger(f'{url} stored {label}, but does not report which tests\n')
            return True

        logger(f"Reported {label}: {', '.join(sorted(stored))}\n")
        dropped = sorted(set(payload.results) - set(stored))
        if dropped:
            logger(f"{url} did not store {', '.join(dropped)}\n")
            return False
        return True

    @classmethod
    def _evidence_in(cls, rows: list[EWSRow]) -> FlakyVerdict:
        """Who was building across these rows, as a verdict with no type decided yet."""
        evidence = FlakyVerdict()
        for row in rows:
            if build_url := row.details.build_url:
                evidence.build_urls.add(build_url)
            if pr_number := row.details.pr_number:
                evidence.pr_numbers.add(pr_number)
            evidence.authors |= set(row.details.authors)
        return evidence

    @classmethod
    def _convict(
        cls, evidence: FlakyVerdict, flaky_type: str,
        prs_needed: int = 0, authors_needed: int = 0, builds_needed: int = 0,
    ) -> Optional[FlakyVerdict]:
        if (
            len(evidence.pr_numbers) >= prs_needed
            and len(evidence.authors) >= authors_needed
            and len(evidence.build_urls) >= builds_needed
        ):
            evidence.flaky_type = flaky_type
            return evidence

    @classmethod
    def _rows_by_type(
        cls, rows: list[EWSRow], curr_authors: Optional[Iterable[str]],
        curr_pr: Optional[int], logger: Callable[[str], None],
    ) -> EWSRowByType:
        """Rows grouped by `flaky_type`, dropping those attributable to the change being judged."""
        curr = {author for author in (curr_authors or []) if author}

        def independent_of_this_pull_request(row: EWSRow) -> bool:
            return not curr_pr or row.details.pr_number != curr_pr

        def independent_of_this_changes_authors(row: EWSRow) -> bool:
            recorded = set(row.details.authors)
            return not recorded or bool(recorded - curr)

        by_type: EWSRowByType = {}
        same_pull_request: EWSRowByType = {}
        same_authors: EWSRowByType = {}
        for row in rows:
            flaky_type = row.flaky_type or cls.FAILED_ROWS
            # A clean-tree row is recorded with the change reverted; its pr_number and authors name the build.
            if flaky_type == cls.WITHIN_STEP_CLEAN_TREE:
                bucket = by_type
            elif not independent_of_this_pull_request(row):
                bucket = same_pull_request
            elif not independent_of_this_changes_authors(row):
                bucket = same_authors
            else:
                bucket = by_type
            bucket.setdefault(flaky_type, []).append(row)

        for flaky_type, dropped in sorted(same_pull_request.items()):
            logger(f"Ignored {len(dropped)} {flaky_type} row(s) recorded by this change's own pull request (#{curr_pr})\n")
        for flaky_type, dropped in sorted(same_authors.items()):
            logger(f"Ignored {len(dropped)} {flaky_type} row(s) recorded by this change's own author(s)\n")
        return by_type

    @classmethod
    def _is_intra_build_flake(cls, rows: EWSRowByType, logger: Callable[[str], None]) -> Optional[FlakyVerdict]:
        if failed_with_no_flaky_type := rows.get(cls.FAILED_ROWS, []):
            logger(f'Ignored {len(failed_with_no_flaky_type)} flake row(s) that carried no flaky_type\n')
        if unrecognized := {name for name in rows if name not in cls.WITHIN_BUILD_ROWS and name != cls.FAILED_ROWS}:
            logger(f"Ignored flakiness recorded as {', '.join(sorted(unrecognized))}\n")

        if clean_tree := rows.get(cls.WITHIN_STEP_CLEAN_TREE, []):
            evidence = cls._evidence_in(clean_tree)
            if verdict := cls._convict(evidence, cls.CLEAN_TREE_VERDICT, builds_needed=cls.BUILDS_FOR_CLEAN_TREE_FLAKE):
                return verdict
            logger(
                f'{len(clean_tree)} clean-tree row(s) come from {len(evidence.build_urls)} build(s), '
                f'fewer than the {cls.BUILDS_FOR_CLEAN_TREE_FLAKE} the clean-tree rule needs\n'
            )

        # Folding a below-threshold clean-tree row here would let the change's own rows fill these quotas.
        with_change = rows.get(cls.WITHIN_STEP_DIRTY_TREE, []) + rows.get(cls.BETWEEN_STEPS_DIRTY_TREE, [])
        return cls._convict(
            cls._evidence_in(with_change), cls.DIRTY_TREE_VERDICT,
            cls.PRS_FOR_DIRTY_TREE_FLAKE, cls.AUTHORS_FOR_DIRTY_TREE_FLAKE,
        )

    @classmethod
    def _is_inter_build_flake(cls, rows: EWSRowByType, logger: Callable[[str], None]) -> Optional[FlakyVerdict]:
        failed = rows.get(cls.FAILED_ROWS, [])
        evidence = cls._evidence_in(failed)

        if verdict := cls._convict(
            evidence, cls.BETWEEN_BUILDS_VERDICT,
            cls.PRS_FOR_BETWEEN_BUILD_FLAKE, cls.AUTHORS_FOR_BETWEEN_BUILD_FLAKE,
        ):
            return verdict

        rows_without_an_author = sum(1 for row in failed if not row.details.authors)
        if rows_without_an_author and len(evidence.authors) < cls.AUTHORS_FOR_BETWEEN_BUILD_FLAKE:
            logger(
                f'{rows_without_an_author} row(s) record no author, so the between-builds rule saw '
                f'{len(evidence.authors)} of the {cls.AUTHORS_FOR_BETWEEN_BUILD_FLAKE} it needs '
                f'across {len(evidence.pr_numbers)} pull request(s)\n'
            )

    @classmethod
    def _parse_results_ews_response(cls, response: 'TwistedAdditions.Response', logger: Callable[[str], None]) -> Optional[list[dict]]:
        """The response body as a list of per-test entries, or None if it is not one."""
        try:
            entries = response.json()
        except ValueError:
            logger(f'Results database answered 200 with a body that is not json\n{response.content}\n')
            return None
        if not isinstance(entries, list) or any(not isinstance(entry, dict) for entry in entries):
            logger(f'Results database answered 200 with an unexpected shape\n{response.content}\n')
            return None
        if any(not entry.get('test') for entry in entries):
            logger(f'Results database answered 200 with an entry naming no test\n{response.content}\n')
            return None
        return entries

    @classmethod
    @defer.inlineCallbacks
    def _query_flaky(
        cls, tests: Iterable[str], flaky: bool, configuration: Optional[dict],
        suite: Optional[str], logger: Callable[[str], None],
    ) -> Generator[Any, Any, Optional[EWSRowByTest]]:
        """Every test named, in chunks small enough to survive the request line, or None if any
        chunk failed: a partial answer would read as an absence of history for the rest."""
        base_params = {key: value for key, value in (configuration or {}).items() if key in cls.CONFIGURATION_KEYS}
        base_params.update(dict(
            suite=suite or cls.DEFAULT_SUITE, flaky='true' if flaky else 'false',
            after_time=int(time.time() - cls.FLAKY_WINDOW_SECONDS), limit=cls.FLAKY_QUERY_LIMIT,
        ))

        url = f'{cls.HOSTNAME}/api/results-ews'
        tests = list(tests)
        by_test: EWSRowByTest = {}

        # Chunked because the names ride in the query string: a whole step's worth of long WPT
        # paths overflows nginx's 8k request line, and a 414 fails every test in the batch.
        for index in range(0, len(tests), cls.TESTS_PER_FLAKY_QUERY):
            chunk = tests[index:index + cls.TESTS_PER_FLAKY_QUERY]
            res = yield TwistedAdditions.request(
                url, params=dict(base_params, tests=chunk), logger=logger,
                timeout=cls.FLAKY_QUERY_TIMEOUT_SECONDS,
            )

            status = res.status_code if res else None
            if status == 404:  # the endpoint's answer for a chunk with nothing recorded
                continue
            if status != 200:
                logger(f"Failed to query {url} ({flaky=}, status {status})\n{getattr(res, 'content', None)}\n")
                return None

            entries = cls._parse_results_ews_response(res, logger)
            if entries is None:
                return None

            for entry in entries:
                rows = [EWSRow.from_json(row) for row in (entry.get('results') or [])]
                by_test.setdefault(entry['test'], []).extend(rows)

        return by_test

    @classmethod
    @defer.inlineCallbacks
    def flaky_verdicts_for(
        cls, tests: Iterable[str], configuration: Optional[dict] = None, suite: Optional[str] = None,
        authors: Optional[Iterable[str]] = None, pr_number: Optional[int] = None,
    ) -> Generator[Any, Any, tuple[dict[str, FlakyVerdict], str]]:
        logs = []

        def logger(log):
            logs.append(log)

        verdicts: dict[str, FlakyVerdict] = {}
        remaining = list(tests)
        intra_build_rows: dict[str, EWSRowByType] = {}
        for flaky, classify in ((True, cls._is_intra_build_flake), (False, cls._is_inter_build_flake)):
            if not remaining:
                break

            result = yield cls._query_flaky(remaining, flaky, configuration, suite, logger)
            if result is None:
                for test in remaining:
                    verdicts[test] = FlakyVerdict(request_failed=True)
                return verdicts, ''.join(logs)

            still_unexplained = []
            for test in remaining:
                rows = cls._rows_by_type(result.get(test, []), authors, pr_number, logger)
                if flaky:
                    intra_build_rows[test] = rows
                if found := classify(rows, logger):
                    found.intra_build_evidence = any(
                        intra_build_rows.get(test, {}).get(name) for name in cls.WITHIN_BUILD_ROWS
                    )
                    verdicts[test] = found
                else:
                    still_unexplained.append(test)
            remaining = still_unexplained

        for test in remaining:
            verdicts[test] = FlakyVerdict()

        if unsupported := sorted(
            test for test, verdict in verdicts.items()
            if verdict.flaky_type == cls.BETWEEN_BUILDS_VERDICT and not verdict.intra_build_evidence
        ):
            logger(f"BetweenBuilds with no within-build flakiness recorded: {', '.join(unsupported)}\n")
        return verdicts, ''.join(logs)

    @classmethod
    def main(cls, args=None):
        parser = argparse.ArgumentParser(
            description='A script which uses the same logic ews-build.webkit.org does to determine if a given ' +
                        'test in a given configuration on a specific commit is currently expected to fail.',
        )
        parser.add_argument(
            'test', type=str,
            help='The test to return results for.',
        )
        parser.add_argument(
            '-c', '--commit',
            type=str, default=None,
            help='Commit ref to focus on',
        )
        parser.add_argument(
            '--suite',
            type=str, default=None,
            help=f'Suite test is found in ({cls.DEFAULT_SUITE} by default)',
        )
        parser.add_argument(
            '-r', '--result',
            type=str, default=None,
            help='Result to filter for (failure, crash, timeout, ect.)',
        )
        parser.add_argument(
            '-s', '--style',
            type=str, default=None,
            help='Build style configuration to focus on (debug, release, ect.)',
        )
        parser.add_argument(
            '-p', '--platform',
            type=str, default=None,
            help='Platform to focus on (mac, ios, gtk, ect.)',
        )
        parser.add_argument(
            '-f', '--flavor',
            type=str, default=None,
            help='Flavor to focus on (wk1, wk2, ect.)',
        )
        parser.add_argument(
            '-v', '--version',
            type=str, default=None,
            help='OS version focus on (Ventura, Monterey, ect.)',
        )
        parser.add_argument(
            '-a', '--architecture',
            type=str, default=None,
            help='Architecture focus on (arm64, x86_64, ect.)',
        )
        parsed = parser.parse_args(args)

        configuration = dict()
        for key in cls.CONFIGURATION_KEYS:
            attr = getattr(parsed, key, None)
            if attr:
                configuration[key] = attr

        d = cls.is_test_expected_to(
            parsed.test,
            result_type=parsed.result, commit=parsed.commit,
            logger=sys.stdout.write, configuration=configuration,
            suite=getattr(parsed, 'suite', None)
        )

        def callback(result):
            if result:
                print('EXPECTED')
            else:
                print('UNEXPECTED')

        d.addCallback(callback)
        d.addBoth(lambda _: reactor.stop())

        return reactor.run()


if __name__ == '__main__':
    sys.exit(ResultsDatabase.main())
