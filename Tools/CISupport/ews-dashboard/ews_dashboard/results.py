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

"""Cache-aware reader for results.webkit.org's test history.

Two endpoints, for two different questions.

/api/results-summary/<suite>/<test> returns nine outcome percentages summing to 100 over roughly
the last 99 runs ending at `ref`. It is a sliding window, not history-up-to-a-commit: asking for an
older ref moves the window, it does not truncate it. So it answers "how reliable is this test
around here", and cannot answer "did this test start failing at commit X".

/api/results/<suite>/<test> returns the individual runs, each against the commit it tested, which is
what an escape check needs. Its bounds are the trap. `after_ref` and `before_ref` resolve a commit
first and answer 404 when results.webkit.org has never heard of it, and a 404 from this service
otherwise means "nothing recorded", so a landed commit that is not registered would read as a test
that never ran. `after_timestamp` and `before_timestamp` need no registration, so the bounds here
are commit timestamps. `branch` is the second trap: runs for main are partitioned under the literal
key 'default' (resultsdbpy `CommitContext.DEFAULT_BRANCH_KEY`), so `branch=main` answers with an
empty list rather than an error. Sending no branch at all is what selects main.

Every response is cached, including the absence of one. A configuration with no recorded history
answers 404, which is a real answer; re-asking it on every run is what made the prototype's
analysis pass take over an hour.
"""

from __future__ import annotations

import http.client
import json
import sqlite3
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import Counter
from concurrent import futures
from dataclasses import asdict, dataclass, replace
from typing import Iterable, Optional

from ews_dashboard import config

OUTCOMES = ('pass', 'fail', 'timeout', 'crash', 'image', 'audio', 'text', 'error', 'warning')

# A stored run carries one outcome, the worst of the run, spelled as resultsdbpy's Expectations
# names it. WARNING counts with PASS for the same reason `Answer.pass_rate` counts it there.
PASSING_OUTCOMES = ('PASS', 'WARNING')

# A run's uuid is its commit's timestamp times this, plus an ordinal separating commits that share a
# timestamp (resultsdbpy `Commit.UUID_MULTIPLIER`). Nothing else in the response says which commit a
# run tested, so this is how a run is placed before or after a landing.
UUID_MULTIPLIER = 100

HTTP_TIMEOUT_SECONDS = 30
RETRY_ATTEMPTS = 3
RETRY_BACKOFF_SECONDS = 1.5

CURRENT_TTL_SECONDS = 24 * 3600
NO_HISTORY_TTL_SECONDS = 7 * 24 * 3600

RUNS_TTL_SECONDS = 6 * 3600
# Bots report late: a window that only just closed keeps filling for about this long, so a cached
# answer for one is re-asked until the window is this far behind.
RUNS_SETTLING_SECONDS = 2 * 24 * 3600
# Per configuration, and the endpoint's own default. A window measured in days holds far fewer.
RUNS_PER_QUERY = 100

# The endpoint answers in about 1.6 seconds, so a few thousand lookups cost over an hour in series.
PREFETCH_WORKERS = 16

# Named by base class rather than one entry per failure: `ssl.SSLError` on a reset connection is an
# OSError that is none of ConnectionError, TimeoutError or URLError, so an enumeration let it past
# `_get` uncaught, and `_fetch_without_raising` only answers to HistoryUnavailable. Everything
# urlopen raises is an OSError or an HTTPException.
TRANSIENT_ERRORS = (
    OSError,
    http.client.HTTPException,
    json.JSONDecodeError,
)


class HistoryUnavailable(Exception):
    """results.webkit.org could not be reached or answered unusably. Distinct from a 404, which
    means the service is fine and has nothing recorded for that configuration."""


@dataclass(frozen=True)
class Configuration:
    suite: str
    platform: str
    style: str
    flavor: str = ''

    @classmethod
    def of_build(cls, build_row: sqlite3.Row) -> 'Configuration':
        return cls(
            suite=build_row['suite'],
            platform=build_row['platform'] or '',
            style=build_row['style'] or '',
            flavor=build_row['flavor'] or '',
        )

    def query_parameters(self) -> dict:
        parameters = {'platform': self.platform, 'style': self.style}
        if self.flavor:
            parameters['flavor'] = self.flavor
        return parameters


@dataclass(frozen=True)
class Query:
    test_name: str
    configuration: Configuration
    # '' asks about the tip of the tree. Anything else is a WebKit identifier or SHA.
    commit_ref: str = ''


@dataclass(frozen=True)
class RunQuery:
    """Every run of one test on main whose commit falls in a window of commit timestamps."""

    test_name: str
    configuration: Configuration
    after: int
    before: int


@dataclass(frozen=True)
class Run:
    """One run of one test, against the commit whose timestamp is `commit_at`.

    `expected` is what the platform's expectations allowed, so a test listed as a failure is
    `failed` and not `unexpected`.
    """

    actual: str
    expected: str
    started_at: int
    commit_at: int
    version_name: str

    @property
    def failed(self) -> bool:
        return self.actual not in PASSING_OUTCOMES

    @property
    def unexpected(self) -> bool:
        return self.failed and self.actual not in self.expected.split()


@dataclass(frozen=True)
class Answer:
    """A cache hit. `outcomes` is None when the hit records that upstream has no history."""

    outcomes: Optional[dict]

    @property
    def pass_rate(self) -> Optional[float]:
        """How often this test passes, or None when nothing is recorded for the configuration.

        A warning counts as a pass, because EWS's own pre-existing check compares
        `data.get('pass', 100) + data.get('warning', 0)` against the same threshold
        (results_db.py). Counting passes alone would call a warning-heavy test pre-existing where
        EWS blamed the author, which is the opposite of what this dashboard is measuring.
        """
        if self.outcomes is None:
            return None
        return (self.outcomes.get('pass') or 0.0) + (self.outcomes.get('warning') or 0.0)


def cached_answer(connection: sqlite3.Connection, query: Query) -> Optional[Answer]:
    """One test's history from the cache alone, or None when no refresh has stored one.

    Reaches no network, so a page can explain a classification without becoming a slow request.
    A miss on the commit falls back to the tip-of-tree row because `History._resolved` drops an
    unregistered commit before caching, and deciding whether a commit is registered is itself a
    request this must not make.
    """
    candidates = (query, replace(query, commit_ref='')) if query.commit_ref else (query,)
    for candidate in candidates:
        row = _cache_row(connection, candidate)
        if row is not None and not _expired(row, candidate):
            return _answer_of(row)
    return None


def _answer_of(row: sqlite3.Row) -> Answer:
    if not row['has_history']:
        return Answer(None)
    return Answer({outcome: row[f'{outcome}_pct'] for outcome in OUTCOMES})


def _cache_row(connection: sqlite3.Connection, query: Query) -> Optional[sqlite3.Row]:
    configuration = query.configuration
    return connection.execute(
        '''SELECT has_history, pass_pct, fail_pct, timeout_pct, crash_pct, image_pct,
                  audio_pct, text_pct, error_pct, warning_pct, fetched_at
           FROM results_summary_cache
           WHERE test_name = ? AND suite = ? AND platform = ? AND style = ? AND flavor = ?
             AND commit_ref = ?''',
        (query.test_name, configuration.suite, configuration.platform,
         configuration.style, configuration.flavor, query.commit_ref),
    ).fetchone()


def _expired(row: sqlite3.Row, query: Query) -> bool:
    age = int(time.time()) - row['fetched_at']
    if not row['has_history']:
        return age > NO_HISTORY_TTL_SECONDS
    if not query.commit_ref:
        return age > CURRENT_TTL_SECONDS
    return False


def cached_runs(connection: sqlite3.Connection, query: RunQuery) -> Optional[list]:
    """One test's runs over a window from the cache alone, or None when no refresh has stored them.

    An empty list is an answer — the test did not run on main in that window — and is distinct from
    the None this returns for a window nobody has asked about yet.
    """
    row = _runs_cache_row(connection, query)
    if row is None or _runs_expired(row, query):
        return None
    return _runs_of(row)


def _runs_of(row: sqlite3.Row) -> list:
    return [Run(**stored) for stored in json.loads(row['runs'])]


def _runs_cache_row(connection: sqlite3.Connection, query: RunQuery) -> Optional[sqlite3.Row]:
    configuration = query.configuration
    return connection.execute(
        '''SELECT runs, fetched_at FROM test_runs_cache
           WHERE test_name = ? AND suite = ? AND platform = ? AND style = ? AND flavor = ?
             AND after_at = ? AND before_at = ?''',
        (query.test_name, configuration.suite, configuration.platform, configuration.style,
         configuration.flavor, query.after, query.before),
    ).fetchone()


def _runs_expired(row: sqlite3.Row, query: RunQuery) -> bool:
    """Whether a window may still be filling. A window whose end is far enough in the past when it
    was fetched holds every run it will ever hold, so that answer never expires."""
    if row['fetched_at'] >= query.before + RUNS_SETTLING_SECONDS:
        return False
    return int(time.time()) - row['fetched_at'] > RUNS_TTL_SECONDS


def _commit_refs_in(queries: 'list[Query]') -> 'list[str]':
    return sorted({query.commit_ref for query in queries if query.commit_ref})


def _runs_in(answer: list) -> Iterable[Run]:
    """The runs across every configuration the response groups them by.

    A query names a platform, a style and a flavor, which still spans OS versions, models and
    architectures, so a response holds several groups. They are pooled the way the results-summary
    percentages pool them, and each run keeps the version it ran on so a reader can separate them.
    """
    for group in answer:
        if not isinstance(group, dict):
            continue
        configuration = group.get('configuration') or {}
        for run in group.get('results') or []:
            uuid = int(run.get('uuid') or 0)
            yield Run(
                # A run with no outcome recorded is a pass, which is how the results-summary
                # endpoint reads it too (`result.get('actual', 'PASS')` in test_controller.py).
                actual=run.get('actual') or 'PASS',
                expected=run.get('expected') or '',
                started_at=int(run.get('start_time') or 0),
                commit_at=uuid // UUID_MULTIPLIER,
                version_name=configuration.get('version_name') or '',
            )


class History:
    def __init__(self, connection: sqlite3.Connection,
                 base_url: str = config.RESULTS_URL) -> None:
        self.connection = connection
        self.base_url = base_url.rstrip('/')
        self.stats: Counter = Counter()
        self._registered_commits: dict = {}
        # Reentrant because the memo check records a statistic while already holding it.
        self._lock = threading.RLock()

    def pass_rate(self, query: Query) -> Optional[float]:
        return Answer(self.outcomes(query)).pass_rate

    def runs(self, query: RunQuery) -> list:
        """Every run of one test on main whose commit falls in the window, oldest commit first.

        Empty means the test did not run there, which upstream reports as a 404 for a test it has
        never heard of and as an empty list for one it has: neither is distinguishable from the
        other here, and neither is evidence of anything.
        """
        cached = cached_runs(self.connection, query)
        if cached is not None:
            self._record('runs_cache_hit')
            return cached
        self._record('runs_cache_miss')
        fetched = self._fetch_runs(query)
        self._write_runs_cache(query, fetched)
        return fetched

    def _fetch_runs(self, query: RunQuery) -> list:
        parameters = dict(
            query.configuration.query_parameters(),
            after_timestamp=query.after,
            before_timestamp=query.before,
            limit=RUNS_PER_QUERY,
        )
        path = (
            f'/api/results/{query.configuration.suite}/'
            f"{urllib.parse.quote(query.test_name, safe='/')}"
            f'?{urllib.parse.urlencode(parameters)}'
        )
        answer = self._get(path)
        if not isinstance(answer, list):
            return []
        return sorted(_runs_in(answer), key=lambda run: (run.commit_at, run.started_at))

    def _write_runs_cache(self, query: RunQuery, runs: list) -> None:
        configuration = query.configuration
        with self.connection:
            self.connection.execute(
                '''INSERT OR REPLACE INTO test_runs_cache (
                    test_name, suite, platform, style, flavor, after_at, before_at,
                    runs, fetched_at
                ) VALUES (?,?,?,?,?,?,?, ?,?)''',
                (
                    query.test_name, configuration.suite, configuration.platform,
                    configuration.style, configuration.flavor, query.after, query.before,
                    json.dumps([asdict(run) for run in runs]), int(time.time()),
                ),
            )

    def outcomes(self, query: Query) -> Optional[dict]:
        resolved = self._resolved(query)
        cached = self._read_cache(resolved)
        if cached is not None:
            self._record('cache_hit')
            return cached.outcomes
        self._record('cache_miss')
        outcomes = self._fetch(resolved)
        self._write_cache(resolved, outcomes)
        return outcomes

    def _resolved(self, query: Query) -> Query:
        """The query that will actually be sent.

        EWS drops the commit and asks about the tip of the tree when results.webkit.org does not
        know it (results_db.py `is_test_expected_to`), so this reproduces that. Resolving here
        rather than inside the fetch is what keeps a tip-of-tree answer out of a cache row keyed by
        a commit: such a row would look pinned, and `_expired` never expires a pinned row.
        """
        if not query.commit_ref or self.is_registered(query.commit_ref):
            return query
        self._record('commit_not_registered')
        return replace(query, commit_ref='')

    def prefetch(self, queries: Iterable[Query], workers: int = PREFETCH_WORKERS) -> None:
        """Warm the cache for many queries at once, so a later serial pass never waits on HTTP.

        Fetches run in a thread pool. Every sqlite call stays on the calling thread, which is what
        keeps the connection single-threaded. A query whose fetch fails is left uncached, so the
        serial pass retries it and reports its own outcome.

        Each answer is written as it arrives rather than after the pool drains, so an interrupted
        refresh keeps the thousands of lookups it already paid for.
        """
        requested = list(queries)
        with futures.ThreadPoolExecutor(max_workers=workers) as pool:
            # Registration first: resolving a query consults it, and the memo only helps once
            # filled. A day of builds shares an order of magnitude fewer base commits than tests.
            list(pool.map(self.is_registered, _commit_refs_in(requested)))
            pending = self._uncached(self._resolved(query) for query in requested)
            answers = pool.map(self._fetch_without_raising, pending)
            for query, (fetched, outcomes) in zip(pending, answers):
                if fetched:
                    self._write_cache(query, outcomes)

    def is_registered(self, commit_ref: str) -> bool:
        """Whether results.webkit.org knows this commit.

        EWS makes the same pre-check before asking for history, so an unregistered commit means a
        tip-of-tree lookup rather than a pointless 404.
        """
        with self._lock:
            if commit_ref in self._registered_commits:
                self._record('commit_memo_hit')
                return self._registered_commits[commit_ref]
        query = urllib.parse.urlencode({'ref': commit_ref})
        answer = self._get(f'/api/commits?{query}')
        registered = bool(answer) and not (isinstance(answer, dict) and answer.get('status') == 'error')
        with self._lock:
            self._registered_commits[commit_ref] = registered
        return registered

    def _uncached(self, queries: Iterable[Query]) -> list:
        pending = []
        seen = set()
        for query in queries:
            if query in seen:
                continue
            seen.add(query)
            if self._read_cache(query) is None:
                pending.append(query)
        return pending

    def _record(self, name: str) -> None:
        with self._lock:
            self.stats[name] += 1

    def _fetch(self, query: Query) -> Optional[dict]:
        parameters = query.configuration.query_parameters()
        if query.commit_ref:
            parameters['ref'] = query.commit_ref
        path = (
            f'/api/results-summary/{query.configuration.suite}/'
            f"{urllib.parse.quote(query.test_name, safe='/')}"
            f'?{urllib.parse.urlencode(parameters)}'
        )
        answer = self._get(path)
        if not isinstance(answer, dict):
            return None
        return {outcome: answer.get(outcome) for outcome in OUTCOMES}

    def _fetch_without_raising(self, query: Query) -> tuple:
        """(whether the answer is usable, the answer). Runs on a pool thread: no sqlite here."""
        try:
            return True, self._fetch(query)
        except HistoryUnavailable:
            self._record('unavailable')
            return False, None

    def _get(self, path: str) -> Optional[object]:
        """Parsed JSON, or None when upstream says it has nothing for this request.

        404 and 400 are both "nothing recorded here": a test that has never run in a configuration
        answers 404, and a configuration the service does not recognize answers 400.
        """
        url = f'{self.base_url}{path}'
        last_error: Optional[Exception] = None
        for attempt in range(RETRY_ATTEMPTS):
            try:
                request = urllib.request.Request(url, headers={'Accept': 'application/json'})
                with urllib.request.urlopen(request, timeout=HTTP_TIMEOUT_SECONDS) as response:
                    return json.loads(response.read())
            except urllib.error.HTTPError as error:
                if error.code in (400, 404):
                    self._record('no_history')
                    return None
                last_error = error
            except TRANSIENT_ERRORS as error:
                last_error = error
            if attempt + 1 < RETRY_ATTEMPTS:
                time.sleep(RETRY_BACKOFF_SECONDS * (attempt + 1))
        raise HistoryUnavailable(str(last_error))

    def _read_cache(self, query: Query) -> Optional[Answer]:
        """This exact query's cached answer. Unlike `cached_answer` it does not fall back to the tip
        of the tree, because `_resolved` has already decided which of the two this query is."""
        row = _cache_row(self.connection, query)
        if row is None or _expired(row, query):
            return None
        return _answer_of(row)

    def _write_cache(self, query: Query, outcomes: Optional[dict]) -> None:
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
                    *[(outcomes or {}).get(outcome) for outcome in OUTCOMES],
                    int(time.time()),
                ),
            )
