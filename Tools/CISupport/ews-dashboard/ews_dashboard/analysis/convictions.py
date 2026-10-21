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

"""What the flakiness classifier convicted, by rule and by test.

Every count is over the answer that stood for each test — the rerun's where there is one, the first
run's otherwise, which is what latest_flakiness_verdicts selects. Convictions are counted per
(build, test), so one test convicted in twelve builds counts twelve times: what matters is how much
noise the rule absorbed, not how many distinct tests are unreliable.
"""

from __future__ import annotations

import sqlite3
from dataclasses import dataclass
from typing import Optional

from ews_dashboard import config, queues, results
from ews_dashboard.analysis import filters

WINDOW = 'build.started_at >= :since AND build.started_at < :until'


@dataclass(frozen=True)
class ConvictedTest:
    test_name: str
    rules: tuple
    convictions: int
    queues: int
    last_seen: int
    builder: str
    builder_id: int
    build_number: int
    pr_id: Optional[int]
    configuration: results.Configuration


@dataclass(frozen=True)
class TestConviction:
    """One conviction of a single test, for that test's own drilldown."""

    rule: str
    builder: str
    builder_id: int
    build_number: int
    pr_id: Optional[int]
    started_at: int
    configuration: results.Configuration


@dataclass(frozen=True)
class Convictions:
    """One capped set of a rule's convicted tests, and whether it is the whole of them, so a response
    describes what it just sent rather than presenting a cut set as the whole of it.

    `total` is the count of every row the query matched whether or not the cap kept it, so
    `truncated` can tell a reader the set was cut without the caller having to compare it itself.
    """

    tests: list
    total: int
    limit: int

    @property
    def truncated(self) -> bool:
        return self.total > len(self.tests)


@dataclass(frozen=True)
class TestConvictions:
    """One capped set of a single test's own convictions, and whether it is the whole of them, so
    the drilldown can say when a cap cut its list the same way `Convictions` already does for the
    convicted-tests table.

    `total` is the count of every conviction the query matched whether or not the cap kept it, so
    `truncated` can tell a reader the set was cut without the caller having to compare it itself.
    """

    convictions: tuple
    total: int
    limit: int

    @property
    def truncated(self) -> bool:
        return self.total > len(self.convictions)


@dataclass(frozen=True)
class QueueActivity:
    builder: str
    builds_queried: int
    convictions: int
    query_failures: int


def _filters(suite: Optional[str], builders: tuple = ()) -> tuple:
    """Extra WHERE clauses for a query that has build_verdicts aliased as `build`, and their
    parameters. Returned together so a caller cannot bind one without the other."""
    conditions, parameters = '', {}
    if suite is not None:
        conditions += ' AND build.suite = :suite'
        parameters['suite'] = suite
    fragment, builder_parameters = queues.builder_filter(builders)
    if fragment:
        conditions += f' AND {fragment}'
        parameters.update(builder_parameters)
    return conditions, parameters


def _window_parameters(since: int, until: int, suite: Optional[str], builders: tuple = ()) -> tuple:
    conditions, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until})
    return conditions, parameters


def by_rule(connection: sqlite3.Connection, since: int, until: int, suite: Optional[str] = None,
            builders: tuple = ()) -> dict:
    """Convictions per rule, including rules that never fired, so a zero shows up as a zero."""
    conditions, parameters = _window_parameters(since, until, suite, builders)
    counted = {
        row['rule']: row['convictions']
        for row in connection.execute(
            f'''SELECT verdict.rule, COUNT(*) AS convictions
                FROM latest_flakiness_verdicts AS verdict
                JOIN build_verdicts AS build USING (build_id)
                WHERE verdict.rule IS NOT NULL AND {WINDOW}{conditions}
                GROUP BY verdict.rule''',
            parameters,
        )
    }
    return {rule: counted.get(rule, 0) for rule in config.FLAKINESS_RULES}


def builds_queried(connection: sqlite3.Connection, since: int, until: int,
                   suite: Optional[str] = None, builders: tuple = ()) -> int:
    conditions, parameters = _window_parameters(since, until, suite, builders)
    return connection.execute(
        f'''SELECT COUNT(*) FROM build_verdicts AS build
            WHERE build.flakiness_query_ran = 1 AND {WINDOW}{conditions}''',
        parameters,
    ).fetchone()[0]


def query_failures(connection: sqlite3.Connection, since: int, until: int,
                   suite: Optional[str] = None, builders: tuple = ()) -> int:
    """Tests the classifier asked about and got no answer for — the read path's own error rate."""
    conditions, parameters = _window_parameters(since, until, suite, builders)
    return connection.execute(
        f'''SELECT COUNT(*)
            FROM latest_flakiness_verdicts AS verdict
            JOIN build_verdicts AS build USING (build_id)
            WHERE verdict.query_failed = 1 AND {WINDOW}{conditions}''',
        parameters,
    ).fetchone()[0]


def convicted_tests(
    connection: sqlite3.Connection,
    since: int,
    until: int,
    suite: Optional[str] = None,
    builders: tuple = (),
    limit: int = 1000,
    conditions: tuple = (),
    sort_keys: tuple = (),
) -> 'Convictions':
    """The tests convicted, one row per test, in the order asked for, each with a build to link to,
    capped at `limit` rows so the query's cost has a ceiling whatever day-window or filter a reader
    picks.

    `conditions` and `sort_keys` come from `filters`, which owns every column name, operator and
    expression a request can reach: nothing a reader typed is spelled into this SQL, only bound
    to it.

    The builder, build number and configuration columns are bare in an aggregate query alongside
    MAX(started_at), which sqlite documents as taking their values from the row that produced the
    maximum — so they describe the most recent conviction rather than an arbitrary one.
    """
    scoping, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until, 'limit': limit})
    where, having, bound = filters.clause(conditions)
    parameters.update(bound)
    narrowing = f' AND {where}' if where else ''
    grouping = f' HAVING {having}' if having else ''
    selection = (f'''FROM latest_flakiness_verdicts AS verdict
                JOIN build_verdicts AS build USING (build_id)
                WHERE verdict.rule IS NOT NULL AND {WINDOW}{scoping}{narrowing}
                GROUP BY verdict.test_name{grouping}''')
    # Counted through a subquery whether or not there is a HAVING, because this is what tells a
    # reader the cap cut the set, not what clamps anything: `Convictions.truncated` compares it
    # against the rows actually returned below.
    total = connection.execute(
        f'SELECT COUNT(*) FROM (SELECT verdict.test_name {selection})', parameters,
    ).fetchone()[0]
    rows = connection.execute(
        # SQLite takes bare columns beside a single MAX() from the row that produced it, so builder,
        # build_number and pr_id all describe the last-seen build.
        f'''SELECT verdict.test_name,
                   GROUP_CONCAT(DISTINCT verdict.rule) AS rules,
                   COUNT(*) AS convictions,
                   COUNT(DISTINCT build.builder) AS queues,
                   MAX(build.started_at) AS last_seen,
                   build.builder, build.builder_id, build.build_number, build.pr_id,
                   build.suite, build.platform, build.style, build.flavor
            {selection}
            ORDER BY {filters.order_by(filters.TESTS, sort_keys)}
            LIMIT :limit''',
        parameters,
    ).fetchall()
    return Convictions(
        tests=[
            ConvictedTest(
                test_name=row['test_name'],
                rules=tuple(sorted(row['rules'].split(','))) if row['rules'] else (),
                convictions=row['convictions'],
                queues=row['queues'],
                last_seen=row['last_seen'],
                builder=row['builder'],
                builder_id=row['builder_id'],
                build_number=row['build_number'],
                pr_id=row['pr_id'],
                configuration=results.Configuration.of_build(row),
            )
            for row in rows
        ],
        total=total,
        limit=limit,
    )


def test_convictions(
    connection: sqlite3.Connection,
    since: int,
    until: int,
    test_name: str,
    suite: Optional[str] = None,
    builders: tuple = (),
    limit: int = 200,
) -> 'TestConvictions':
    """Every conviction of one test in the window, newest first, for that test's own drilldown,
    capped at `limit` so a test convicted very often still hands back a bounded page rather than
    every conviction it has ever drawn, alongside the total the cap cut it from and whether it did.

    `test_name` is bound, never spelled into the SQL. The total is counted through the same WHERE
    as the capped rows, so the two cannot disagree about what was cut.
    """
    conditions, parameters = _window_parameters(since, until, suite, builders)
    parameters.update({'test_name': test_name, 'limit': limit})
    scoping = f'''FROM latest_flakiness_verdicts AS verdict
                JOIN build_verdicts AS build USING (build_id)
                WHERE verdict.rule IS NOT NULL AND verdict.test_name = :test_name AND {WINDOW}{conditions}'''
    total = connection.execute(f'SELECT COUNT(*) {scoping}', parameters).fetchone()[0]
    rows = connection.execute(
        f'''SELECT verdict.rule, build.builder, build.builder_id, build.build_number, build.pr_id,
                   build.started_at, build.suite, build.platform, build.style, build.flavor
            {scoping}
            ORDER BY build.started_at DESC, build.build_id DESC
            LIMIT :limit''',
        parameters,
    ).fetchall()
    return TestConvictions(
        convictions=tuple(
            TestConviction(
                rule=row['rule'],
                builder=row['builder'],
                builder_id=row['builder_id'],
                build_number=row['build_number'],
                pr_id=row['pr_id'],
                started_at=row['started_at'],
                configuration=results.Configuration.of_build(row),
            )
            for row in rows
        ),
        total=total,
        limit=limit,
    )


def _counts_by_builder(connection: sqlite3.Connection, sql: str, parameters: dict) -> dict:
    return {row['builder']: row['total'] for row in connection.execute(sql, parameters)}


def queue_activity(connection: sqlite3.Connection, since: int, until: int,
                   suite: Optional[str] = None) -> list:
    """Per queue: how often it asked, how often it convicted, how often the query failed.

    Takes no builder/group selection: this is the window's whole set of active queues, which is
    what the queue dropdown's tree is built from, so a reader narrowing to one queue never makes
    the others disappear from the list they could pick instead.

    Only RunWebKitTests and ReRunWebKitTests set the properties behind builds_queried (steps.py).
    They ask only when a run had failures, the pull request targets main, and the limit held.
    So a zero can mean the queue's builds passed, not that it skipped the read path.
    An api-tests queue is always zero: its steps set no flakiness property at all.
    """
    conditions, parameters = _filters(suite)
    parameters.update({'since': since, 'until': until})
    queried = _counts_by_builder(
        connection,
        f'''SELECT build.builder, SUM(build.flakiness_query_ran) AS total
            FROM build_verdicts AS build WHERE {WINDOW}{conditions} GROUP BY build.builder''',
        parameters,
    )
    convicted = _counts_by_builder(
        connection,
        f'''SELECT build.builder, COUNT(*) AS total
            FROM latest_flakiness_verdicts AS verdict
            JOIN build_verdicts AS build USING (build_id)
            WHERE verdict.rule IS NOT NULL AND {WINDOW}{conditions}
            GROUP BY build.builder''',
        parameters,
    )
    failed = _counts_by_builder(
        connection,
        f'''SELECT build.builder, COUNT(*) AS total
            FROM latest_flakiness_verdicts AS verdict
            JOIN build_verdicts AS build USING (build_id)
            WHERE verdict.query_failed = 1 AND {WINDOW}{conditions}
            GROUP BY build.builder''',
        parameters,
    )
    activity = [
        QueueActivity(
            builder=builder,
            builds_queried=queried.get(builder) or 0,
            convictions=convicted.get(builder) or 0,
            query_failures=failed.get(builder) or 0,
        )
        for builder in sorted(set(queried) | set(convicted) | set(failed))
    ]
    return sorted(activity, key=lambda queue: (-queue.builds_queried, queue.builder))
