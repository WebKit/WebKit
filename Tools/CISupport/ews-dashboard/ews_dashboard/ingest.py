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

"""Turn EWS builds into rows this dashboard can query.

One build becomes one build_verdicts row plus one flakiness_verdicts row per test the flakiness
classifier answered about, so no page has to parse JSON to count convictions.
"""

from __future__ import annotations

import json
import sqlite3
import time
from collections import Counter
from concurrent import futures
from dataclasses import dataclass, field
from typing import Iterable, Iterator, Optional

from ews_dashboard import buildbot, config, suites

BUILDBOT_RESULTS = {
    0: 'SUCCESS',
    1: 'WARNINGS',
    2: 'FAILURE',
    3: 'SKIPPED',
    4: 'EXCEPTION',
    5: 'RETRY',
    6: 'CANCELLED',
}

# results.webkit.org's platform enum capitalizes these two; Buildbot reports them lowercase.
RESULTS_PLATFORM_NAMES = {'gtk': 'GTK', 'wpe': 'WPE'}

# The only flavors EWS puts in a history query, from `results_db_query_configuration` in steps.py.
RESULTS_FLAVORS = ('wk1', 'wk2', 'site-isolation')

FLAKINESS_RUNS = ((1, 'first'), (2, 'second'))

# api-tests has one flakiness read, not two runs of its own: AnalyzeAPITestsResults' classifier
# check happens at ReRunAPITests.doOnFailure, which is the rerun still carrying the change -- the
# same slot run_number 2 already means for layout's second run. The property carries no run prefix
# at all (`results-db_api_flaky`, not `results-db_second_run_flaky`), so it is handled alongside
# FLAKINESS_RUNS rather than folded into its key template.
API_FLAKY_RUN_NUMBER = 2
API_FLAKY_KEY = 'results-db_api_flaky'

# An api-tests build whose properties carry no failure list costs three serial round trips against
# Buildbot, which measured at 10s per 50 builds against 0.8s for a layout builder. Fetching a page's
# worth of them together is where a refresh gets its time back.
LOG_FETCH_WORKERS = 8

# How many builds already in the database end a walk. Buildbot numbers do not complete in order, so a
# straggler can finish after a newer build and sit below it in a -number listing; a single hit would
# stop the walk above it and never ingest it. A full page cannot hide one.
SETTLED_RUN = buildbot.PAGE_LIMIT

# A single malformed build must not end a builder's walk, so the loop catches everything a bad
# payload or a dropped connection can raise and records it against the build number.
INGEST_ERRORS = (sqlite3.Error, KeyError, TypeError, ValueError) + buildbot.TRANSIENT_ERRORS


@dataclass(frozen=True)
class FailureLists:
    """The three failure sets EWS compares to decide what to blame the author for."""

    first_run: Optional[list]
    second_run: Optional[list]
    clean_tree: Optional[list]

    @classmethod
    def unknown(cls) -> 'FailureLists':
        return cls(None, None, None)


@dataclass(frozen=True)
class FlakinessVerdict:
    run_number: int
    test_name: str
    rule: Optional[str]
    query_failed: bool = False
    within_build_evidence: Optional[bool] = None


@dataclass(frozen=True)
class IngestError:
    builder: str
    build_number: Optional[int]
    message: str

    def __str__(self) -> str:
        where = self.builder if self.build_number is None else f'{self.builder} build {self.build_number}'
        return f'{where}: {self.message}'

    @classmethod
    def of(cls, builder: str, build_number: Optional[int], error: BaseException) -> 'IngestError':
        return cls(builder, build_number, f'{type(error).__name__}: {error}')


@dataclass
class IngestReport:
    outcomes: 'Counter' = field(default_factory=Counter)
    errors: list = field(default_factory=list)

    def record(self, outcome: str) -> None:
        self.outcomes[outcome] += 1

    def add(self, other: 'IngestReport') -> None:
        self.outcomes.update(other.outcomes)
        self.errors.extend(other.errors)

    @property
    def failed(self) -> int:
        return len(self.errors)


def verdict_of(buildbot_result: Optional[int]) -> str:
    if buildbot_result is None:
        return 'UNKNOWN'
    return BUILDBOT_RESULTS.get(buildbot_result, 'UNKNOWN')


def platform_of(reported: Optional[str]) -> Optional[str]:
    if reported is None:
        return None
    return RESULTS_PLATFORM_NAMES.get(reported, reported)


def flavor_of(properties: dict, builder_name: str, suite: suites.Suite) -> str:
    """The flavor EWS would send with this build's history query.

    The build carries it: the step that starts a run sets the `flavor` property from its own
    `results_db_flavor`, which is how a site-isolation queue reports site-isolation and how a rerun
    keeps the flavor the first run set (steps.py). Only the three values in `RESULTS_FLAVORS` reach
    results.webkit.org, since `results_db_query_configuration` drops anything else, so a builder-name
    guess of jsc would pin the query to a flavor EWS never asked about.

    api-tests sends no flavor in any configuration: `RunAPITests` builds its query from platform and
    style alone.

    The builder name is the fallback for a build from before EWS set the property.
    """
    if suite.name == 'api-tests':
        return ''
    reported = _text_property(properties, 'flavor')
    if reported in RESULTS_FLAVORS:
        return reported
    name = builder_name.upper()
    for marker, flavor in (('SITE-ISOLATION', 'site-isolation'), ('WK2', 'wk2'), ('WK1', 'wk1')):
        if marker in name:
            return flavor
    return ''


def _text_property(properties: dict, key: str) -> Optional[str]:
    value = buildbot.property_value(properties, key)
    return None if value is None else str(value)


def _flag_property(properties: dict, key: str) -> bool:
    return bool(buildbot.property_value(properties, key, False))


def _decoded_property(properties: dict, key: str) -> Optional[object]:
    value = buildbot.property_value(properties, key)
    if isinstance(value, str):
        try:
            return json.loads(value)
        except ValueError:
            return None
    return value


def _list_property(properties: dict, key: str) -> Optional[list]:
    value = _decoded_property(properties, key)
    return value if isinstance(value, list) else None


def _map_property(properties: dict, key: str) -> Optional[dict]:
    value = _decoded_property(properties, key)
    return value if isinstance(value, dict) else None


def _as_json(value: Optional[list]) -> Optional[str]:
    if value is None:
        return None
    return json.dumps(value, separators=(',', ':'))


def _has_results_db_filtered_lists(properties: dict) -> bool:
    """Whether the analyzer already removed results-db pre-existing failures for us.

    Since WebKit commits f554adccfd9e and 60edc560c7b8, AnalyzeLayoutTestsResults and
    AnalyzeAPITestsResults blame the author for the intersection of the *filtered* lists. Ingesting
    the raw lists for such a build would surface tests EWS never showed anyone.

    Both properties are required, not just the first: the single-run and repeat-failures schemes
    set first_run_failures_filtered for their own unrelated pre-existing check and have no second
    run to filter.
    """
    return 'first_run_failures_filtered' in properties and 'second_run_failures_filtered' in properties


def _failures_in_log(log_text: Optional[str]) -> list:
    """The failure names in one retry step's JSON log.

    Reproduces RunAPITests.parse_api_failures_from_string: the Timedout, Crashed and Failed names.
    """
    if not log_text:
        return []
    try:
        report = json.loads(log_text)
    except ValueError:
        return []
    if not isinstance(report, dict):
        return []
    names = set()
    for key in ('Timedout', 'Crashed', 'Failed'):
        for entry in report.get(key) or []:
            if isinstance(entry, dict) and entry.get('name') is not None:
                names.add(entry['name'])
    return sorted(names)


def _lists_from_retry_step_logs(
    client: buildbot.BuildbotClient,
    build_id: int,
    suite: suites.Suite,
) -> FailureLists:
    steps_by_name = {step['name']: step for step in client.steps(build_id)}
    collected = []
    for step_name in suite.retry_step_names:
        step = steps_by_name.get(step_name)
        if step is None:
            collected.append(None)
            continue
        log_text = client.log_text(step['stepid'], 'json')
        collected.append(_failures_in_log(log_text) if log_text else None)
    return FailureLists(*collected)


def _lists_from_properties(properties: dict, suite: suites.Suite) -> FailureLists:
    """The three failure lists under whichever retry scheme this build actually ran.

    The scheme is detected from the properties the build carries rather than a per-builder list, so
    a builder changing scheme needs no change here. Two schemes report something other than the
    classic three lists:

      repeat-failures  GTK and WPE builders rerun only the failures and report the ones that keep
                       failing, with and without the change.
      single-run       site-isolation, stress and WPT-simulator builders run once and check
                       pre-existing status against results.webkit.org instead of a clean-tree run,
                       so the single run stands in for its own confirmation.
    """
    if _has_results_db_filtered_lists(properties):
        return FailureLists(
            first_run=_list_property(properties, 'first_run_failures_filtered'),
            second_run=_list_property(properties, 'second_run_failures_filtered'),
            clean_tree=_list_property(properties, suite.clean_tree_property),
        )
    if suite.second_run_property in properties:
        return FailureLists(
            first_run=_list_property(properties, suite.first_run_property),
            second_run=_list_property(properties, suite.second_run_property),
            clean_tree=_list_property(properties, suite.clean_tree_property),
        )
    if 'with_change_repeat_failures_results_nonflaky_failures' in properties:
        return FailureLists(
            first_run=_list_property(properties, suite.first_run_property),
            second_run=_list_property(properties, 'with_change_repeat_failures_results_nonflaky_failures'),
            clean_tree=_list_property(properties, 'without_change_repeat_failures_results_nonflaky_failures'),
        )
    if suite.first_run_property in properties:
        single_run = _list_property(properties, suite.first_run_property)
        return FailureLists(
            first_run=single_run,
            second_run=single_run,
            clean_tree=_list_property(properties, 'results-db_first_run_pre_existing'),
        )
    return FailureLists.unknown()


def failure_lists(
    properties: dict,
    suite: suites.Suite,
    client: buildbot.BuildbotClient,
    build_id: int,
) -> FailureLists:
    if _has_results_db_filtered_lists(properties) or not suite.failures_from_logs:
        return _lists_from_properties(properties, suite)
    return _lists_from_retry_step_logs(client, build_id, suite)


def exceeded_failure_limit(properties: dict) -> bool:
    return (_flag_property(properties, 'first_results_exceed_failure_limit')
            or _flag_property(properties, 'second_results_exceed_failure_limit'))


def flakiness_query_ran(properties: dict) -> bool:
    return any(
        f'results-db_{run}_run_flaky{suffix}' in properties
        for _, run in FLAKINESS_RUNS
        for suffix in ('', '_unsupported', '_unknown')
    ) or any(f'{API_FLAKY_KEY}{suffix}' in properties for suffix in ('', '_unsupported', '_unknown'))


def _verdicts_from_map(
    properties: dict,
    run_number: int,
    flaky_key: str,
    unsupported_key: str,
    unknown_key: str,
) -> list:
    convictions = _map_property(properties, flaky_key) or {}
    without_evidence = set(_list_property(properties, unsupported_key) or [])
    query_failed = _list_property(properties, unknown_key) or []

    verdicts = []
    for test_name, rule in convictions.items():
        evidence: Optional[bool] = None
        if rule == config.BETWEEN_BUILDS:
            evidence = test_name not in without_evidence
        verdicts.append(FlakinessVerdict(run_number, test_name, rule, within_build_evidence=evidence))
    for test_name in query_failed:
        if test_name not in convictions:
            verdicts.append(FlakinessVerdict(run_number, test_name, None, query_failed=True))
    return verdicts


def flakiness_verdicts(properties: dict) -> list:
    """One verdict per test the classifier answered about, for each run that asked, plus api-tests'
    own single read.

    Both layout runs are kept. The rerun's answer is the one the author saw, but a first run that
    convicted a test the rerun did not is exactly the disagreement worth being able to see, and the
    prototype threw it away.
    """
    verdicts = []
    for run_number, run in FLAKINESS_RUNS:
        verdicts.extend(_verdicts_from_map(
            properties, run_number,
            f'results-db_{run}_run_flaky',
            f'results-db_{run}_run_flaky_unsupported',
            f'results-db_{run}_run_flaky_unknown',
        ))
    verdicts.extend(_verdicts_from_map(
        properties, API_FLAKY_RUN_NUMBER,
        API_FLAKY_KEY, f'{API_FLAKY_KEY}_unsupported', f'{API_FLAKY_KEY}_unknown',
    ))
    return verdicts


def _store(
    connection: sqlite3.Connection,
    columns: dict,
    verdicts: list,
    replacing: bool,
) -> None:
    build_id = columns['build_id']
    names = ', '.join(columns)
    placeholders = ', '.join('?' * len(columns))
    # escape_verdicts, flakiness_verdicts and build_classifications all cascade from build_verdicts
    # on delete, and a REPLACE is a delete: it would take an escape verdict costing a
    # results.webkit.org round trip with it, which nothing here recomputes. Update in place instead,
    # and let the two cheap tables be cleared deliberately below.
    updates = ', '.join(f'{name} = excluded.{name}' for name in columns if name != 'build_id')
    with connection:
        connection.execute(
            f'INSERT INTO build_verdicts ({names}) VALUES ({placeholders}) '
            f'ON CONFLICT (build_id) DO UPDATE SET {updates}',
            tuple(columns.values()),
        )
        if replacing:
            connection.execute('DELETE FROM flakiness_verdicts WHERE build_id = ?', (build_id,))
            connection.execute('DELETE FROM build_classifications WHERE build_id = ?', (build_id,))
        connection.executemany(
            '''INSERT OR REPLACE INTO flakiness_verdicts (
                build_id, run_number, test_name, rule, query_failed, within_build_evidence
            ) VALUES (?,?,?,?,?,?)''',
            [
                (build_id, verdict.run_number, verdict.test_name, verdict.rule,
                 int(verdict.query_failed),
                 None if verdict.within_build_evidence is None else int(verdict.within_build_evidence))
                for verdict in verdicts
            ],
        )
        connection.execute(
            'INSERT OR REPLACE INTO builds_ingested (builder_id, build_number, fetched_at) VALUES (?,?,?)',
            (columns['builder_id'], columns['build_number'], int(time.time())),
        )


def ingest_build(
    connection: sqlite3.Connection,
    client: buildbot.BuildbotClient,
    builder_name: str,
    builder_id: int,
    build: dict,
    force: bool = False,
    lists: Optional[FailureLists] = None,
) -> str:
    """Ingest one build. Returns 'ingested', 'reingested', 'already_present' or 'skipped'.

    `force` re-reads a build already in the database and drops its cached classification, which is
    how a build ingested before EWS started publishing the filtered failure lists gets corrected.

    `lists` is the answer a caller already fetched for this build, so a walk that scraped a page of
    logs in parallel does not scrape them again one at a time.
    """
    build_id = build['buildid']
    build_number = build['number']

    already_present = _already_stored(connection, builder_id, build_number)
    if already_present and not force:
        return 'already_present'
    if not build.get('complete'):
        return 'skipped'

    suite = suites.suite_for_builder(builder_name)
    if suite is None:
        return 'skipped'

    properties = build.get('properties') or {}
    if lists is None:
        lists = failure_lists(properties, suite, client, build_id)
    pull_request = _text_property(properties, 'github.number')

    columns = {
        'build_id': build_id,
        'builder': builder_name,
        'builder_id': builder_id,
        'build_number': build_number,
        'pr_id': int(pull_request) if pull_request else None,
        'pr_title': _text_property(properties, 'github.title'),
        'sha': _text_property(properties, 'github.head.sha'),
        'change_id': _text_property(properties, 'change_id'),
        'identifier': _text_property(properties, 'identifier'),
        'platform': platform_of(_text_property(properties, 'platform')),
        'style': _text_property(properties, 'configuration'),
        'flavor': flavor_of(properties, builder_name, suite),
        'suite': suite.name,
        'verdict': verdict_of(build.get('results')),
        'first_run_failures': _as_json(lists.first_run),
        'second_run_failures': _as_json(lists.second_run),
        'clean_tree_run_failures': _as_json(lists.clean_tree),
        'exceeded_failure_limit': int(exceeded_failure_limit(properties)),
        'flakiness_query_ran': int(flakiness_query_ran(properties)),
        'started_at': build.get('started_at'),
        'complete_at': build.get('complete_at'),
    }

    _store(connection, columns, flakiness_verdicts(properties), replacing=already_present)
    return 'reingested' if already_present else 'ingested'


def coverage_floor(connection: sqlite3.Connection, builder_name: str) -> Optional[int]:
    """The oldest timestamp a completed walk of this builder reached, or None if none ever has."""
    row = connection.execute(
        'SELECT walked_since FROM builder_coverage WHERE builder = ?', (builder_name,),
    ).fetchone()
    return row['walked_since'] if row else None


def _settles_the_walk(connection: sqlite3.Connection, builder_name: str,
                      since: Optional[int]) -> bool:
    """Whether a run of builds already held may end this walk.

    Below that run sits either history a previous walk fetched or history nobody has ever asked
    Buildbot for, and only the recorded reach tells the two apart: a queue holding its last fortnight
    would otherwise stop a 90-day walk two days in and report itself up to date. A walk given no
    window has no floor to reach, so for it the settled run is the only stopping rule there is.
    """
    if since is None:
        return True
    floor = coverage_floor(connection, builder_name)
    return floor is not None and floor <= since


def _record_coverage(connection: sqlite3.Connection, builder_name: str,
                     since: Optional[int]) -> None:
    """Remember how far back this walk went, keeping the deepest any walk has reached.

    A later narrow walk must not shrink the claim: the builds a 90-day walk stored are still stored
    once a 14-day one has run over the top of them.
    """
    reached = 0 if since is None else since
    floor = coverage_floor(connection, builder_name)
    with connection:
        connection.execute(
            'INSERT OR REPLACE INTO builder_coverage (builder, walked_since, walked_at) VALUES (?,?,?)',
            (builder_name, reached if floor is None else min(floor, reached), int(time.time())),
        )


def ingest_builder(
    connection: sqlite3.Connection,
    client: buildbot.BuildbotClient,
    builder_name: str,
    since: Optional[int] = None,
    limit: Optional[int] = None,
    force: bool = False,
    workers: int = LOG_FETCH_WORKERS,
) -> IngestReport:
    """Walk one builder's completed builds into the database.

    Nothing here escapes: listing the builder and paging its builds are HTTP calls of their own, and
    one builder losing its connection halfway must not end a walk over forty others.

    The walk proceeds a page at a time so the log scrapes a page needs can be fetched together, and
    stops once it has seen a page's worth of builds it already holds — but only within a window it
    has been walked over before, since a run of builds already held says nothing about the history
    underneath it. A walk that died, or one cut short by `limit`, reached nothing it can claim.
    """
    report = IngestReport()
    suite = suites.suite_for_builder(builder_name)
    if suite is None:
        return report

    try:
        builder_id = client.builder_id(builder_name)
        settles = _settles_the_walk(connection, builder_name, since)
        stored_in_a_row = 0
        for page in _pages(client.builds(builder_id, since=since, limit=limit)):
            scraped = _scraped_lists(
                client, suite, workers,
                [build for build in page
                 if _needs_log_scrape(connection, builder_id, build, suite, force)],
            )
            for build in page:
                try:
                    outcome = ingest_build(connection, client, builder_name, builder_id, build,
                                           force=force, lists=scraped.get(build['buildid']))
                except INGEST_ERRORS as error:
                    report.errors.append(IngestError.of(builder_name, build.get('number'), error))
                    continue
                report.record(outcome)
                stored_in_a_row = stored_in_a_row + 1 if outcome == 'already_present' else 0
                if stored_in_a_row >= SETTLED_RUN and not force and settles:
                    return report
    except INGEST_ERRORS as error:
        report.errors.append(IngestError.of(builder_name, None, error))
        report.record('builder_failed')
        return report

    if limit is None:
        _record_coverage(connection, builder_name, since)
    return report


def _pages(builds: Iterable[dict], size: int = buildbot.PAGE_LIMIT) -> Iterator[list]:
    """Group a build walk into pages, so the log scrapes a page needs can be fetched together.

    A walk that dies mid-page hands over what it read before the error reaches the caller, since a
    builder whose next page never arrives has still told us about the builds above it.
    """
    page: list = []
    remaining = iter(builds)
    while True:
        try:
            build = next(remaining)
        except StopIteration:
            break
        except Exception:
            if page:
                yield page
            raise
        page.append(build)
        if len(page) == size:
            yield page
            page = []
    if page:
        yield page


def _already_stored(connection: sqlite3.Connection, builder_id: int, build_number: int) -> bool:
    return connection.execute(
        'SELECT 1 FROM builds_ingested WHERE builder_id = ? AND build_number = ?',
        (builder_id, build_number),
    ).fetchone() is not None


def _needs_log_scrape(
    connection: sqlite3.Connection,
    builder_id: int,
    build: dict,
    suite: suites.Suite,
    force: bool,
) -> bool:
    if not suite.failures_from_logs or not build.get('complete'):
        return False
    if _has_results_db_filtered_lists(build.get('properties') or {}):
        return False
    return force or not _already_stored(connection, builder_id, build['number'])


def _scraped_lists(
    client: buildbot.BuildbotClient,
    suite: suites.Suite,
    workers: int,
    builds: list,
) -> dict:
    """Failure lists read from logs for the builds whose properties do not carry them.

    api-tests publishes no failure-list property, so each of its failing builds costs three serial
    round trips: a steps listing, a logs listing and the log itself. Fetching a page of them at once
    is where a refresh gets its time back. Nothing here touches sqlite, which is what keeps the
    connection single-threaded, and a build whose scrape raises is left out so `ingest_build`
    fetches it itself and reports its own error.
    """
    if not builds:
        return {}
    with futures.ThreadPoolExecutor(max_workers=min(workers, len(builds))) as pool:
        scraped = pool.map(lambda build: _scraped_without_raising(client, build, suite), builds)
        return {build['buildid']: lists
                for build, lists in zip(builds, scraped) if lists is not None}


def _scraped_without_raising(
    client: buildbot.BuildbotClient,
    build: dict,
    suite: suites.Suite,
) -> Optional[FailureLists]:
    try:
        return _lists_from_retry_step_logs(client, build['buildid'], suite)
    except INGEST_ERRORS:
        return None


def dashboard_builder_names(client: buildbot.BuildbotClient) -> list:
    """Every builder Buildbot currently exposes that this dashboard knows how to read."""
    return sorted(
        builder['name'] for builder in client.builders()
        if suites.suite_for_builder(builder['name']) is not None
    )
