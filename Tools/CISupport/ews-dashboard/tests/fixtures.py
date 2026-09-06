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

"""Fixtures shared by the tests.

Buildbot returns every property as a [value, source] pair and every failure list as a JSON string
inside one, so the builds here are shaped that way. A fixture of bare values would pass against code
that cannot read a real build.
"""

from __future__ import annotations

import json
import os
import shutil
import sqlite3
import tempfile
import time
import unittest
from typing import Iterator, Optional

from ews_dashboard import db, ingest, results, suites

LAYOUT_BUILDER = 'macOS-Sequoia-Release-WK2-Tests-EWS'
API_BUILDER = 'macOS-Tahoe-Debug-API-Tests-EWS'
GTK_BUILDER = 'GTK-Linux-64-bit-Release-Tests-EWS'
IOS_BUILDER = 'iOS-18-Simulator-WK2-Tests-EWS'
WPE_BUILDER = 'WPE-Linux-64-bit-Release-Tests-EWS'

IDENTIFIER = '314546@main'
CONFIGURATION_PROPERTIES = {
    'platform': 'mac',
    'configuration': 'release',
    'identifier': IDENTIFIER,
    'github.number': 12345,
    'github.head.sha': 'a' * 40,
}

DEFAULT_BUILD_TIME = 1_770_000_000


def properties(values: dict, source: str = 'test') -> dict:
    return {key: [value, source] for key, value in values.items()}


def failure_list_properties(values: dict) -> dict:
    """Failure lists arrive JSON-encoded inside the property pair, as steps.py writes them."""
    return properties({key: json.dumps(value) for key, value in values.items()})


def build(
    number: int = 1,
    build_id: Optional[int] = None,
    extra_properties: Optional[dict] = None,
    results_code: int = 2,
    started_at: int = DEFAULT_BUILD_TIME,
    complete: bool = True,
) -> dict:
    """One Buildbot build payload. A property whose pair holds None is left out, so a test can say a
    build never reported it rather than reporting it empty."""
    merged = dict(properties(CONFIGURATION_PROPERTIES), **(extra_properties or {}))
    return {
        'buildid': build_id if build_id is not None else 1000 + number,
        'number': number,
        'complete': complete,
        'results': results_code,
        'started_at': started_at,
        'complete_at': started_at + 1800,
        'properties': {key: pair for key, pair in merged.items() if pair[0] is not None},
    }


class StubBuildbot:
    """Answers only what ingest asks of it, and records what was asked.

    A test that reaches a step or log it was not given data for gets nothing, rather than a silent
    HTTP call.
    """

    def __init__(self, steps_by_build: Optional[dict] = None, logs: Optional[dict] = None) -> None:
        self.steps_by_build = steps_by_build or {}
        self.logs = logs or {}
        self.requested_logs: list = []

    def steps(self, build_id: int) -> list:
        return self.steps_by_build.get(build_id, [])

    def log_text(self, step_id: int, log_name: str) -> Optional[str]:
        self.requested_logs.append((step_id, log_name))
        return self.logs.get(step_id)


class WalkableBuildbot(StubBuildbot):
    """Serves one builder's builds, newest first, the way `ingest_builder` walks them.

    `since` and `limit` cut the walk off where the real client does, so a test can hand a builder more
    history than the window it asks for.
    """

    def __init__(self, builds: list, **kwargs: object) -> None:
        super().__init__(**kwargs)
        self.builds_available = builds

    def builder_id(self, name: str) -> int:
        return 7

    def builds(self, builder_id: int, since: Optional[int] = None,
               limit: Optional[int] = None) -> Iterator[dict]:
        for yielded, payload in enumerate(self.builds_available, start=1):
            if since is not None and payload['started_at'] < since:
                return
            yield payload
            if limit is not None and yielded >= limit:
                return


class HalfDeadBuildbot(StubBuildbot):
    """Answers some builds and then loses the connection, which is what a dropped page looks like
    from ingest's side. `error_on_listing` moves the failure to the builder lookup instead."""

    def __init__(self, builds: list, error: Exception, error_on_listing: bool = False,
                 **kwargs: object) -> None:
        super().__init__(**kwargs)
        self.builds_before_error = builds
        self.error = error
        self.error_on_listing = error_on_listing

    def builder_id(self, name: str) -> int:
        if self.error_on_listing:
            raise self.error
        return 7

    def builds(self, builder_id: int, since: Optional[int] = None,
               limit: Optional[int] = None) -> Iterator[dict]:
        for payload in self.builds_before_error:
            yield payload
        raise self.error


class UnreachableHistory:
    """Stands in for results.webkit.org where a lookup must never happen."""

    def pass_rate(self, query: object) -> float:
        raise AssertionError(f'something asked results.webkit.org about {query}')


class StubHistory:
    """Pass rates by test name.

    A test in `unavailable` raises the way an outage does; a test simply absent from `pass_rates`
    answers None, the way a configuration with no recorded history does.
    """

    def __init__(self, pass_rates: dict, unavailable: Optional[set] = None) -> None:
        self.pass_rates = pass_rates
        self.unavailable = unavailable or set()
        self.asked: list = []
        self.queries: list = []

    def pass_rate(self, query: results.Query) -> Optional[float]:
        self.asked.append(query.test_name)
        self.queries.append(query)
        if query.test_name in self.unavailable:
            raise results.HistoryUnavailable(query.test_name)
        return self.pass_rates.get(query.test_name)


class StubRunHistory:
    """Runs on main by test name, for the escape check.

    A window is answered by filtering the timeline a test hands over, inclusive at both ends, since
    the endpoint's own inclusivity is not documented: code that relies on it rather than on the
    commit each run tested fails here.
    """

    def __init__(self, runs_by_test: dict, unavailable: Optional[set] = None) -> None:
        self.runs_by_test = runs_by_test
        self.unavailable = unavailable or set()
        self.queries: list = []

    def runs(self, query: results.RunQuery) -> list:
        self.queries.append(query)
        if query.test_name in self.unavailable:
            raise results.HistoryUnavailable(query.test_name)
        return [run for run in self.runs_by_test.get(query.test_name, [])
                if query.after <= run.commit_at <= query.before]


def run(actual: str = 'PASS', commit_at: int = DEFAULT_BUILD_TIME, expected: str = 'PASS',
        version_name: str = 'Sequoia') -> results.Run:
    """One run of one test on main. A run reports later than the commit it tested, so `started_at`
    is deliberately not `commit_at`."""
    return results.Run(actual=actual, expected=expected, started_at=commit_at + 1800,
                       commit_at=commit_at, version_name=version_name)


class DatabaseTest(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.mkdtemp(prefix='ews-dashboard-test-')
        self.database_path = os.path.join(self.directory, 'test.db')
        db.initialize(self.database_path)
        self.connection = db.connect(self.database_path)

    def tearDown(self) -> None:
        self.connection.close()
        shutil.rmtree(self.directory)

    def store_build(
        self,
        number: int,
        first: Optional[list] = None,
        second: Optional[list] = None,
        clean: Optional[list] = None,
        flaky: Optional[dict] = None,
        flaky_first_run: Optional[dict] = None,
        query_failed: Optional[list] = None,
        builder: str = LAYOUT_BUILDER,
        builder_id: int = 7,
        started_at: int = DEFAULT_BUILD_TIME,
        results_code: int = 2,
        exceeded_failure_limit: bool = False,
        identifier: Optional[str] = IDENTIFIER,
        pr_id: Optional[int] = None,
        pr_title: Optional[str] = None,
        sha: Optional[str] = None,
    ) -> int:
        """Store one build the way ingest would, and return its build id.

        api-tests publishes its failure lists only under the filtered names, since the raw ones live
        in the retry steps' logs rather than in properties.
        """
        filtered = suites.suite_for_builder(builder).failures_from_logs
        lists = {}
        if first is not None:
            lists['first_run_failures_filtered' if filtered else 'first_run_failures'] = first
        if second is not None:
            lists['second_run_failures_filtered' if filtered else 'second_run_failures'] = second
        if clean is not None:
            lists['clean_tree_run_failures'] = clean
        if flaky is not None:
            lists['results-db_second_run_flaky'] = flaky
        if flaky_first_run is not None:
            lists['results-db_first_run_flaky'] = flaky_first_run
        if query_failed is not None:
            lists['results-db_second_run_flaky_unknown'] = query_failed
        extra = failure_list_properties(lists)
        extra.update(properties({'identifier': identifier}))
        if pr_id is not None:
            extra.update(properties({'github.number': pr_id}))
        if pr_title is not None:
            extra.update(properties({'github.title': pr_title}))
        if sha is not None:
            extra.update(properties({'github.head.sha': sha}))
        if exceeded_failure_limit:
            extra.update(properties({'first_results_exceed_failure_limit': True}))

        payload = build(number=number, build_id=builder_id * 100_000 + number,
                        extra_properties=extra, started_at=started_at, results_code=results_code)
        ingest.ingest_build(self.connection, StubBuildbot(), builder,
                            builder_id=builder_id, build=payload)
        return payload['buildid']

    def cache_answer(self, query: results.Query, outcomes: Optional[dict],
                     age_seconds: int = 0) -> None:
        """Write the row a refresh's fetch would have left behind, so a cache-only reader can be
        tested without a fetch of its own.

        `outcomes` of None is the row that records upstream having no history for the
        configuration, which is an answer rather than the absence of one.
        """
        configuration = query.configuration
        with self.connection:
            self.connection.execute(
                '''INSERT OR REPLACE INTO results_summary_cache (
                    test_name, suite, platform, style, flavor, commit_ref, has_history,
                    pass_pct, fail_pct, timeout_pct, crash_pct,
                    image_pct, audio_pct, text_pct, error_pct, warning_pct, fetched_at
                ) VALUES (?,?,?,?,?,?,?, ?,?,?,?, ?,?,?,?,?, ?)''',
                (
                    query.test_name, configuration.suite, configuration.platform,
                    configuration.style, configuration.flavor, query.commit_ref,
                    int(outcomes is not None),
                    *[(outcomes or {}).get(outcome) for outcome in results.OUTCOMES],
                    int(time.time()) - age_seconds,
                ),
            )

    def store_landing(self, pr_id: int, status: str = 'landed',
                      landed_at: int = DEFAULT_BUILD_TIME + 86400,
                      commit_hash: str = 'f' * 40, matches: int = 1) -> None:
        """Write the row `landings.resolve` would have left, so the escape check can be tested
        without a checkout to read commit messages from."""
        with self.connection:
            self.connection.execute(
                '''INSERT OR REPLACE INTO landings (
                    pr_id, status, matches, commit_hash, identifier, landed_at, subject,
                    branch_head, resolved_at
                ) VALUES (?,?,?,?,?,?,?,?,?)''',
                (pr_id, status, matches,
                 commit_hash if status == 'landed' else None,
                 '314600@main' if status == 'landed' else None,
                 landed_at if status == 'landed' else None,
                 'A change that landed' if status == 'landed' else None,
                 'e' * 40, int(time.time())),
            )

    def stored_build(self, build_id: int) -> sqlite3.Row:
        return self.connection.execute(
            'SELECT * FROM build_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()

    def stored_verdicts(self, build_id: int) -> list:
        return self.connection.execute(
            'SELECT run_number, test_name, rule, query_failed, within_build_evidence '
            'FROM flakiness_verdicts WHERE build_id = ? ORDER BY run_number, test_name',
            (build_id,),
        ).fetchall()
