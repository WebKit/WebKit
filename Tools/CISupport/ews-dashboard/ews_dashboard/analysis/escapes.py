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

"""What main did with a test after the change EWS told an author was not to blame for it.

Every other number in this dashboard is a floor, because main does not contain the change and a test
that is reliable on main and genuinely broken by the change cannot be told apart from one that flaked
during the build. Once the change lands, it can: the test now runs on main with the change in it, so
a conviction that was wrong shows up as main failing the test it excused.

One convicted test in one build, whose pull request landed as a known commit on main, falls in one
bucket:

  ESCAPED          main never failed it before the landing and failed it unexpectedly after: the
                   conviction excused a real regression. Two further things are read off the counts
                   rather than stored beside them — whether the share of failing runs after the
                   landing is what a strong escape needs or thinner than that, and whether main is
                   still failing the test now
  FAILS_ON_MAIN    main failed it before the landing too, at any rate, so the failure is not this
                   change's and the conviction was corroborated
  CONTAINED        no unexpected failure on main after the landing
  NO_RUNS          nothing ran the test on main in the window after the landing
  NO_BASELINE      it failed after the landing, but nothing ran before it, so a regression cannot be
                   told from a failure main already had
  TREE_DIVERGED    a later build of the same pull request, started before the landing, tested a
                   different head, so what landed is not what this conviction was made on. A build
                   EWS started after the landing is not divergence: it cannot have superseded the
                   tree that was already on main

Whether main is still failing an escaped test cannot be read from the window either side of the
landing however wide it is, so the assess pass asks a second, fresh question over the last
CURRENCY_DAYS days — of the escapes alone, at most once a day each — and stores the runs and failures
it got back. That answer has four states and never fewer: still failing, recovered, not run lately
when main ran the test no times in the window, and unchecked when nothing has asked yet. Both of the
last two are absences of evidence rather than recoveries, which is why the runs are stored and no
boolean is.

Only an unexpected failure counts. A test main already lists as failing is failing to order, and
counting it would convict every rule of an escape it had nothing to do with.

What this cannot do: it sees only pull requests whose landing `webkit_checkout` could pin down, only
the window either side of the landing, and only tests some bot on main actually runs in the same
configuration. ESCAPED is therefore a floor as well, and the buckets that answer nothing are counted
and shown rather than dropped.
"""

from __future__ import annotations

import math
import sqlite3
import time
from collections import Counter
from dataclasses import dataclass, field
from typing import Optional

from ews_dashboard import config, queues, results, webkit_checkout

ESCAPED = 'ESCAPED'
FAILS_ON_MAIN = 'FAILS_ON_MAIN'
CONTAINED = 'CONTAINED'
NO_RUNS = 'NO_RUNS'
NO_BASELINE = 'NO_BASELINE'
TREE_DIVERGED = 'TREE_DIVERGED'

VERDICTS = (ESCAPED, FAILS_ON_MAIN, CONTAINED, NO_RUNS, NO_BASELINE, TREE_DIVERGED)

# How hard an escaped test failed after the landing. Derived from the stored counts on every read
# rather than stored alongside them, so it can never contradict the numbers beside it.
STRONG = 'strong'
RARE = 'rare'

# What main is doing with an escaped test now, and a partition of the escapes: exactly one of these
# holds for any row. Neither UNCHECKED nor NOT_RUN_LATELY is another way of saying recovered — nobody
# asked, and nobody ran it, are two different absences of evidence, and neither is good news.
STILL_FAILING = 'still_failing'
RECOVERED = 'recovered'
NOT_RUN_LATELY = 'not_run_lately'
UNCHECKED = 'unchecked'

# What answers nothing about the conviction, so it belongs in no rate. An escape on few failures is
# not here: the question was answered, and only the evidence behind the answer is thin.
UNDECIDED_VERDICTS = (NO_RUNS, NO_BASELINE, TREE_DIVERGED)

VERDICT_DESCRIPTIONS = {
    ESCAPED: f'Main had not failed this in the {config.ESCAPE_WINDOW_DAYS} days before the landing '
             f'and did fail it in the {config.ESCAPE_WINDOW_DAYS} days after, so the conviction '
             'excused something main was not failing before. At least '
             f'{config.ESCAPE_FAILURE_PCT}% of those runs failing makes it a strong escape; below '
             'that the escape rests on few failures.',
    FAILS_ON_MAIN: f'Main was already failing this in the {config.ESCAPE_WINDOW_DAYS} days before '
                   'the change landed, so the failure is not this change\'s and the build was told '
                   'the truth.',
    CONTAINED: f'Main did not fail this in the {config.ESCAPE_WINDOW_DAYS} days after the landing.',
    NO_RUNS: f'No bot ran this on main in the {config.ESCAPE_WINDOW_DAYS} days after the change '
             'landed.',
    NO_BASELINE: f'It failed on main in the {config.ESCAPE_WINDOW_DAYS} days after the change '
                 f'landed, but nothing ran it in the {config.ESCAPE_WINDOW_DAYS} days before, so a '
                 'regression cannot be told from a failure main already had.',
    TREE_DIVERGED: 'This conviction was made on a version of the pull request that a later build '
                   'superseded before the landing, so the code that landed is not the code it was '
                   'made on and main cannot grade it.',
}

# The pull requests a conviction cannot even be looked for on, counted from `landings` rather than
# stored, since a pull request that has not landed yet is the ordinary case and not an answer.
NOT_LANDED = 'not_landed'
AMBIGUOUS = 'ambiguous'
UNRESOLVED = 'unresolved'
UNAVAILABLE = 'unavailable'

ESCAPE_WINDOW_SECONDS = config.ESCAPE_WINDOW_DAYS * 86400
CURRENCY_WINDOW_SECONDS = config.CURRENCY_DAYS * 86400

# One escape is worth reading about individually, so the list is long before it is cut.
ESCAPES_LISTED = 200

WINDOW = 'build.started_at >= :since AND build.started_at < :until'


def rarity_for_counts(runs_after: int, failed_after: int) -> Optional[str]:
    """Whether an escape's failures after the landing are the share a strong escape needs.

    None when nothing ran after the landing, which no escape reaches: `verdict_for_counts` sends a
    conviction with no run after the landing to NO_RUNS before the baseline is ever consulted, so
    every ESCAPED row is either STRONG or RARE and the two of them are the whole of the bucket.
    """
    if not runs_after:
        return None
    if 100.0 * failed_after / runs_after >= config.ESCAPE_FAILURE_PCT:
        return STRONG
    return RARE


WILSON_Z = 1.6449


def strength_for_counts(runs_after: int, failed_after: int) -> Optional[float]:
    """The lower end of the 90% Wilson score interval on `failed_after / runs_after`, as a fraction.

    None when nothing ran after the landing, for the reason `rarity_for_counts` returns None there:
    a rate over zero runs is not thin evidence, it is no evidence. This ranks escapes against each
    other; it does not decide STRONG or RARE above, which stays the raw rate against
    `ESCAPE_FAILURE_PCT`.
    """
    if not runs_after:
        return None
    n = float(runs_after)
    p = failed_after / n
    denominator = 1 + WILSON_Z * WILSON_Z / n
    centre = p + WILSON_Z * WILSON_Z / (2 * n)
    margin = WILSON_Z * math.sqrt(p * (1 - p) / n + WILSON_Z * WILSON_Z / (4 * n * n))
    return max(0.0, (centre - margin) / denominator)


def currency_for_counts(recent_runs: Optional[int], recent_failed: Optional[int],
                        recent_checked_at: Optional[int]) -> str:
    """What main is doing with the test now, from the last currency check.

    Four states, and one of them holds for every escape. Both absences of evidence are named as
    themselves: an escape nobody has asked about is UNCHECKED, and one main ran nothing about in the
    window is NOT_RUN_LATELY, because a recovery main was never in a position to demonstrate would
    read as reassurance a reader has no grounds for.
    """
    if recent_checked_at is None or recent_runs is None:
        return UNCHECKED
    if not recent_runs:
        return NOT_RUN_LATELY
    return STILL_FAILING if recent_failed else RECOVERED


def damage_for_counts(recent_runs: Optional[int], recent_failed: Optional[int]) -> Optional[float]:
    """`recent_failed / recent_runs` from the last currency check, as a fraction.

    None when `recent_runs` is None or zero, or when `recent_failed` is None: a row nobody has asked
    about, or asked about and got no runs back, has no rate to report, and 0% would claim an answer
    the data does not carry.
    """
    if not recent_runs or recent_failed is None:
        return None
    return recent_failed / recent_runs


def _filters(suite: Optional[str], builders: tuple = ()) -> tuple:
    """Extra WHERE clauses for a query with build_verdicts aliased as `build`, and their parameters,
    returned together so a caller cannot bind one without the other."""
    conditions, parameters = '', {}
    if suite is not None:
        conditions += ' AND build.suite = :suite'
        parameters['suite'] = suite
    fragment, builder_parameters = queues.builder_filter(builders)
    if fragment:
        conditions += f' AND {fragment}'
        parameters.update(builder_parameters)
    return conditions, parameters


@dataclass(frozen=True)
class Conviction:
    """One conviction main answered, and everything a reader needs to go and check the answer.

    `heads` and `builds` count the pull request's builds rather than this one's, which is what says
    how far the code that landed had moved from the code convicted here.

    `landed_at` and `window_ends_at` both come off the row rather than one being derived from the
    other: the gap between them is ESCAPE_WINDOW_DAYS, and a stored row's landing time must not move
    when that setting does. `landed_at` is None for a row whose landing the database no longer holds,
    and a page says so rather than printing a date.

    The `recent_` fields are the last currency check, and are None together when none has run.
    """

    test_name: str
    rule: str
    verdict: str
    build_id: int
    builder: str
    builder_id: int
    build_number: int
    pr_id: Optional[int]
    configuration: results.Configuration
    runs_before: int
    failed_before: int
    runs_after: int
    failed_after: int
    landed_at: Optional[int]
    window_ends_at: int
    tested_sha: Optional[str]
    newest_sha: Optional[str]
    heads: int
    builds: int
    recent_runs: Optional[int] = None
    recent_failed: Optional[int] = None
    recent_checked_at: Optional[int] = None

    @property
    def rarity(self) -> Optional[str]:
        return rarity_for_counts(self.runs_after, self.failed_after)

    @property
    def currency(self) -> str:
        return currency_for_counts(self.recent_runs, self.recent_failed, self.recent_checked_at)

    @property
    def strength(self) -> Optional[float]:
        return strength_for_counts(self.runs_after, self.failed_after)

    @property
    def damage(self) -> Optional[float]:
        return damage_for_counts(self.recent_runs, self.recent_failed)


@dataclass(frozen=True)
class Subcategories:
    """How a window's escapes split, twice over.

    Two partitions of the one number, not six buckets: what main is doing with the test now, and how
    hard it failed after the landing. Counted here rather than in a template so both totals are the
    escape count and a page cannot print a split that does not add up.
    """

    still_failing: int = 0
    recovered: int = 0
    not_run_lately: int = 0
    unchecked: int = 0
    strong: int = 0
    rare: int = 0

    @property
    def total(self) -> int:
        return self.still_failing + self.recovered + self.not_run_lately + self.unchecked

    @property
    def rate_total(self) -> int:
        return self.strong + self.rare


@dataclass(frozen=True)
class Tally:
    """What a window's convictions came to: what main answered, and what it could not be asked.

    `asked` is `decided` plus `undecided` plus `unrecognised_total`. That last term is normally zero
    and exists so it cannot quietly stop being: a verdict name this code has retired still has rows
    stored under it, and counting only the names in `VERDICTS` once took those convictions out of
    every total on the page without saying so.
    """

    by_verdict: dict
    unaskable: dict
    unrecognised: dict = field(default_factory=dict)

    @property
    def asked(self) -> int:
        """Every conviction main was asked about, answered or not, which is what the buckets divide
        up."""
        return sum(self.by_verdict.values()) + self.unrecognised_total

    @property
    def decided(self) -> int:
        """Convictions main gave an answer about, which is the only honest denominator here."""
        return sum(count for verdict, count in self.by_verdict.items()
                   if verdict not in UNDECIDED_VERDICTS)

    @property
    def escaped(self) -> int:
        return self.by_verdict.get(ESCAPED, 0)

    @property
    def escape_rate_pct(self) -> Optional[float]:
        if not self.decided:
            return None
        return round(100.0 * self.escaped / self.decided, 1)

    @property
    def undecided(self) -> int:
        return sum(self.by_verdict.get(verdict, 0) for verdict in UNDECIDED_VERDICTS)

    @property
    def unrecognised_total(self) -> int:
        """Convictions stored under a verdict name this code has no bucket for.

        Not folded into `undecided`, which names three specific reasons main answered nothing: these
        were answered, by a version of the dashboard that has since renamed the answer, and the fix
        is `scripts/migrate_verdict_names.py` rather than another run.
        """
        return sum(self.unrecognised.values())

    @property
    def unasked(self) -> int:
        return sum(self.unaskable.values())


@dataclass(frozen=True)
class Candidate:
    """One convicted test whose pull request landed, and the two things that bound the check.

    `newest_sha` is the head of the newest build of the same pull request that EWS started at or
    before the landing, which is what decides whether the conviction was made on the code that
    landed. A build started after the landing tested something main already had, so it cannot have
    superseded the tree this conviction was made on and is left out.
    """

    build_id: int
    test_name: str
    rule: str
    configuration: results.Configuration
    pr_id: int
    landed_at: int
    tested_sha: Optional[str]
    newest_sha: Optional[str]

    @property
    def window_ends_at(self) -> int:
        return self.landed_at + ESCAPE_WINDOW_SECONDS

    @property
    def diverged(self) -> bool:
        return (self.tested_sha is not None and self.newest_sha is not None
                and self.tested_sha != self.newest_sha)


@dataclass(frozen=True)
class Verdict:
    verdict: str
    runs_before: int = 0
    failed_before: int = 0
    runs_after: int = 0
    failed_after: int = 0


CANDIDATE_SQL = f'''
    SELECT verdict.test_name, verdict.rule, build.build_id, build.pr_id, build.sha AS tested_sha,
           build.suite, build.platform, build.style, build.flavor, landing.landed_at,
           (SELECT newer.sha FROM build_verdicts AS newer
             WHERE newer.pr_id = build.pr_id AND newer.sha IS NOT NULL
               AND newer.started_at <= landing.landed_at
             ORDER BY newer.started_at DESC, newer.build_id DESC LIMIT 1) AS newest_sha
    FROM latest_flakiness_verdicts AS verdict
    JOIN build_verdicts AS build USING (build_id)
    JOIN landings AS landing ON landing.pr_id = build.pr_id
    WHERE verdict.rule IS NOT NULL
      AND landing.status = '{webkit_checkout.LANDED}' AND landing.landed_at IS NOT NULL
      AND build.started_at >= :since AND build.started_at < :until
    ORDER BY landing.landed_at, build.build_id, verdict.test_name
'''


def candidates(connection: sqlite3.Connection, since: int, until: int) -> 'list[Candidate]':
    """Every convicted test in the window that main can be asked about, oldest landing first."""
    return [
        Candidate(
            build_id=row['build_id'],
            test_name=row['test_name'],
            rule=row['rule'],
            configuration=results.Configuration.of_build(row),
            pr_id=row['pr_id'],
            landed_at=row['landed_at'],
            tested_sha=row['tested_sha'],
            newest_sha=row['newest_sha'],
        )
        for row in connection.execute(CANDIDATE_SQL, {'since': since, 'until': until})
    ]


def verdict_for_counts(runs_before: int, failed_before: int, runs_after: int,
                       failed_after: int) -> str:
    """Which bucket the runs either side of the landing put the conviction in.

    The baseline decides whose failure it is and nothing else does: a test main was already failing,
    at any rate at all, is main's, and a test main had never failed before the landing escaped
    whether it then failed most of the runs or one of them. How hard it failed is `rarity_for_counts`,
    read off these same counts wherever an escape is shown rather than stored as a verdict of its own.
    """
    if not runs_after:
        return NO_RUNS
    if not failed_after:
        # A clean window after the landing needs no baseline: nothing failed, so nothing escaped.
        return CONTAINED
    if not runs_before:
        return NO_BASELINE
    if failed_before:
        return FAILS_ON_MAIN
    return ESCAPED


def redecided(verdict: str, runs_before: int, failed_before: int, runs_after: int,
              failed_after: int) -> str:
    """What a verdict already stored with these counts would be decided as now.

    TREE_DIVERGED is reached before any run is asked for and stores no counts, so it is left as it
    is: putting its zeroes through the rule would rewrite it to NO_RUNS.
    """
    if verdict == TREE_DIVERGED:
        return verdict
    return verdict_for_counts(runs_before, failed_before, runs_after, failed_after)


def decide(runs_before: list, runs_after: list) -> Verdict:
    """Which bucket the runs put the conviction in.

    Pure, and the whole of the judgement: everything else here fetches, stores or counts.
    """
    counts = dict(runs_before=len(runs_before),
                  failed_before=len([run for run in runs_before if run.unexpected]),
                  runs_after=len(runs_after),
                  failed_after=len([run for run in runs_after if run.unexpected]))
    return Verdict(verdict_for_counts(**counts), **counts)


def _runs_in(history: results.History, candidate: Candidate, after: int, before: int) -> list:
    return history.runs(results.RunQuery(candidate.test_name, candidate.configuration,
                                         after=after, before=before))


def _baseline_runs(history: results.History, candidate: Candidate) -> list:
    """What main did with the test before the landing.

    Filtered on the commit rather than left to the query's bounds, because whether the endpoint's
    `after_timestamp` and `before_timestamp` include their endpoints is not documented, and a run of
    the landing commit itself would otherwise count as the baseline it is compared against.
    """
    runs = _runs_in(history, candidate, candidate.landed_at - ESCAPE_WINDOW_SECONDS,
                    candidate.landed_at)
    return [run for run in runs if run.commit_at < candidate.landed_at]


def _watched_runs(history: results.History, candidate: Candidate) -> list:
    runs = _runs_in(history, candidate, candidate.landed_at, candidate.window_ends_at)
    return [run for run in runs if run.commit_at >= candidate.landed_at]


def assess_one(history: results.History, candidate: Candidate) -> Verdict:
    """One conviction's verdict, asking main about the test either side of the landing."""
    if candidate.diverged:
        return Verdict(TREE_DIVERGED)
    return decide(_baseline_runs(history, candidate), _watched_runs(history, candidate))


def _stored(connection: sqlite3.Connection, candidate: Candidate) -> Optional[Verdict]:
    """The verdict already reached for this conviction, or None when it has to be reached again.

    A verdict decided while the window it watched was still filling is not kept: the runs that would
    turn CONTAINED into ESCAPED arrive after the last commit in the window, not with it.
    """
    row = connection.execute(
        '''SELECT verdict, runs_before, failed_before, runs_after, failed_after,
                  window_ends_at, decided_at
           FROM escape_verdicts WHERE build_id = ? AND test_name = ?''',
        (candidate.build_id, candidate.test_name),
    ).fetchone()
    if row is None:
        return None
    if row['decided_at'] < row['window_ends_at'] + results.RUNS_SETTLING_SECONDS:
        return None
    return Verdict(
        verdict=row['verdict'],
        runs_before=row['runs_before'],
        failed_before=row['failed_before'],
        runs_after=row['runs_after'],
        failed_after=row['failed_after'],
    )


def _store(connection: sqlite3.Connection, candidate: Candidate, verdict: Verdict) -> None:
    """Store the verdict, dropping any currency answer with it.

    REPLACE deletes the row before inserting, so the `recent_` columns go back to null here. That is
    what a re-decided verdict deserves — the answer was about the old verdict — and the currency check
    that follows in the same pass fills them again when the verdict is still an escape.
    """
    with connection:
        connection.execute(
            '''INSERT OR REPLACE INTO escape_verdicts (
                build_id, test_name, verdict, runs_before, failed_before, runs_after,
                failed_after, landed_at, window_ends_at, decided_at
            ) VALUES (?,?,?,?,?,?,?,?,?,?)''',
            (candidate.build_id, candidate.test_name, verdict.verdict, verdict.runs_before,
             verdict.failed_before, verdict.runs_after, verdict.failed_after,
             candidate.landed_at, candidate.window_ends_at, int(time.time())),
        )


def _currency_due(connection: sqlite3.Connection, candidate: Candidate, now: int) -> bool:
    """Whether this escape's currency answer is missing or older than the TTL."""
    row = connection.execute(
        'SELECT recent_checked_at FROM escape_verdicts WHERE build_id = ? AND test_name = ?',
        (candidate.build_id, candidate.test_name),
    ).fetchone()
    if row is None or row['recent_checked_at'] is None:
        return True
    return row['recent_checked_at'] <= now - config.CURRENCY_TTL_SECONDS


def _store_currency(connection: sqlite3.Connection, candidate: Candidate, runs: int, failed: int,
                    checked_at: int) -> None:
    with connection:
        connection.execute(
            '''UPDATE escape_verdicts
               SET recent_runs = ?, recent_failed = ?, recent_checked_at = ?
               WHERE build_id = ? AND test_name = ?''',
            (runs, failed, checked_at, candidate.build_id, candidate.test_name),
        )


def check_currency(connection: sqlite3.Connection, history: results.History, candidate: Candidate,
                   now: int) -> bool:
    """Ask main whether it is still failing an escaped test, over a fresh window ending now.

    Returns whether an answer was stored. An outage leaves the columns as they were: the page names
    such a verdict unchecked, which is the truth, and the next pass asks again.
    """
    if not _currency_due(connection, candidate, now):
        return False
    try:
        runs = _runs_in(history, candidate, now - CURRENCY_WINDOW_SECONDS, now)
    except results.HistoryUnavailable:
        return False
    _store_currency(connection, candidate, len(runs),
                    len([run for run in runs if run.unexpected]), now)
    return True


def assess(connection: sqlite3.Connection, history: results.History, since: int,
           until: int) -> Counter:
    """Decide every convicted test in the window that main can be asked about, and store the answers.

    Returns a count per verdict, plus `unavailable` for the convictions results.webkit.org could not
    be reached about. Nothing is stored for those, so the next pass asks again.

    Only the escapes are asked whether main is still failing them, and each of those at most once a
    day: there are tens of escapes against thousands of convictions, and a currency query per
    conviction would be a second full pass over results.webkit.org.
    """
    outcomes: Counter = Counter()
    now = int(time.time())
    for candidate in candidates(connection, since, until):
        verdict = _stored(connection, candidate)
        if verdict is None:
            try:
                verdict = assess_one(history, candidate)
            except results.HistoryUnavailable:
                outcomes[UNAVAILABLE] += 1
                continue
            _store(connection, candidate, verdict)
        outcomes[verdict.verdict] += 1
        if verdict.verdict == ESCAPED:
            check_currency(connection, history, candidate, now)
    return outcomes


def unaskable(connection: sqlite3.Connection, since: int, until: int, suite: Optional[str] = None,
              builders: tuple = ()) -> dict:
    """Convictions in the window that main cannot be asked about, by why not.

    Read from the database alone and never stored, because every one of these is a pull request that
    may land, or be resolved, before the next pass.
    """
    conditions, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until, 'unresolved': UNRESOLVED,
                       'landed': webkit_checkout.LANDED})
    counted = {
        row['status']: row['convictions']
        for row in connection.execute(
            f'''SELECT COALESCE(landing.status, :unresolved) AS status, COUNT(*) AS convictions
                FROM latest_flakiness_verdicts AS verdict
                JOIN build_verdicts AS build USING (build_id)
                LEFT JOIN landings AS landing ON landing.pr_id = build.pr_id
                WHERE verdict.rule IS NOT NULL AND {WINDOW}{conditions}
                  AND (landing.status IS NULL OR landing.status != :landed)
                GROUP BY status''',
            parameters,
        )
    }
    return {reason: counted.get(reason, 0) for reason in (NOT_LANDED, AMBIGUOUS, UNRESOLVED)}


def _counted_verdicts(connection: sqlite3.Connection, since: int, until: int,
                      suite: Optional[str], builders: tuple = ()) -> tuple:
    """Stored verdicts split into the buckets this code knows and the names it does not.

    Both halves come off one query, because a name no longer in `VERDICTS` still has rows and
    counting only the known ones is how a retired verdict once dropped out of every total silently.
    """
    conditions, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until})
    counted = {
        row['verdict']: row['convictions']
        for row in connection.execute(
            f'''SELECT outcome.verdict, COUNT(*) AS convictions
                FROM escape_verdicts AS outcome
                JOIN build_verdicts AS build USING (build_id)
                WHERE {WINDOW}{conditions}
                GROUP BY outcome.verdict''',
            parameters,
        )
    }
    known = {verdict: counted.get(verdict, 0) for verdict in VERDICTS}
    unrecognised = {verdict: count for verdict, count in counted.items() if verdict not in VERDICTS}
    return known, unrecognised


def by_verdict(connection: sqlite3.Connection, since: int, until: int, suite: Optional[str] = None,
               builders: tuple = ()) -> dict:
    """Stored verdicts per bucket, including buckets nothing reached, so a zero reads as a zero.

    Only the buckets this code knows: `tally` is what reports the rest.
    """
    return _counted_verdicts(connection, since, until, suite, builders)[0]


def tally(connection: sqlite3.Connection, since: int, until: int, suite: Optional[str] = None,
          builders: tuple = ()) -> 'Tally':
    known, unrecognised = _counted_verdicts(connection, since, until, suite, builders)
    return Tally(
        by_verdict=known,
        unaskable=unaskable(connection, since, until, suite=suite, builders=builders),
        unrecognised=unrecognised,
    )


def escape_subcategories(connection: sqlite3.Connection, since: int, until: int,
                         suite: Optional[str] = None,
                         builders: tuple = ()) -> Subcategories:
    """How the window's escapes split by what main is doing now and by how hard they failed.

    Counted in Python off the stored counts, through the same two functions the sentences use, so a
    bucket on the page cannot disagree with the sentence a reader opens under it.
    """
    conditions, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until, 'verdict': ESCAPED})
    counted: Counter = Counter()
    for row in connection.execute(
            f'''SELECT outcome.runs_after, outcome.failed_after, outcome.recent_runs,
                       outcome.recent_failed, outcome.recent_checked_at
                FROM escape_verdicts AS outcome
                JOIN build_verdicts AS build USING (build_id)
                WHERE outcome.verdict = :verdict AND {WINDOW}{conditions}''',
            parameters,
    ):
        counted[currency_for_counts(row['recent_runs'], row['recent_failed'],
                                    row['recent_checked_at'])] += 1
        rarity = rarity_for_counts(row['runs_after'], row['failed_after'])
        if rarity is not None:
            counted[rarity] += 1
    return Subcategories(still_failing=counted[STILL_FAILING], recovered=counted[RECOVERED],
                         not_run_lately=counted[NOT_RUN_LATELY], unchecked=counted[UNCHECKED],
                         strong=counted[STRONG], rare=counted[RARE])


def convictions(connection: sqlite3.Connection, since: int, until: int, verdict: str,
                suite: Optional[str] = None, builders: tuple = (),
                limit: int = ESCAPES_LISTED) -> 'list[Conviction]':
    """The individual convictions behind one verdict's count, newest landing first."""
    conditions, parameters = _filters(suite, builders)
    parameters.update({'since': since, 'until': until, 'verdict': verdict, 'limit': limit})
    return [
        Conviction(
            test_name=row['test_name'],
            rule=row['rule'],
            verdict=row['verdict'],
            build_id=row['build_id'],
            builder=row['builder'],
            builder_id=row['builder_id'],
            build_number=row['build_number'],
            pr_id=row['pr_id'],
            configuration=results.Configuration.of_build(row),
            runs_before=row['runs_before'],
            failed_before=row['failed_before'],
            runs_after=row['runs_after'],
            failed_after=row['failed_after'],
            landed_at=row['landed_at'],
            window_ends_at=row['window_ends_at'],
            tested_sha=row['tested_sha'],
            newest_sha=row['newest_sha'],
            heads=row['heads'],
            builds=row['builds'],
            recent_runs=row['recent_runs'],
            recent_failed=row['recent_failed'],
            recent_checked_at=row['recent_checked_at'],
        )
        for row in connection.execute(
            f'''SELECT outcome.*, verdict.rule, build.builder, build.builder_id,
                       build.build_number, build.pr_id, build.suite, build.platform,
                       build.style, build.flavor, build.sha AS tested_sha,
                       (SELECT newer.sha FROM build_verdicts AS newer
                         WHERE newer.pr_id = build.pr_id AND newer.sha IS NOT NULL
                           AND outcome.landed_at IS NOT NULL AND newer.started_at <= outcome.landed_at
                         ORDER BY newer.started_at DESC, newer.build_id DESC LIMIT 1) AS newest_sha,
                       (SELECT COUNT(DISTINCT other.sha) FROM build_verdicts AS other
                         WHERE other.pr_id = build.pr_id AND other.sha IS NOT NULL) AS heads,
                       (SELECT COUNT(*) FROM build_verdicts AS other
                         WHERE other.pr_id = build.pr_id) AS builds
                FROM escape_verdicts AS outcome
                JOIN build_verdicts AS build USING (build_id)
                JOIN latest_flakiness_verdicts AS verdict
                  ON verdict.build_id = outcome.build_id AND verdict.test_name = outcome.test_name
                WHERE outcome.verdict = :verdict AND {WINDOW}{conditions}
                ORDER BY outcome.window_ends_at DESC, outcome.build_id DESC, outcome.test_name
                LIMIT :limit''',
            parameters,
        )
    ]


@dataclass(frozen=True)
class Part:
    """One run of a verdict's sentence, and whether a page should emphasise it.

    The counts are what a reader is looking for in the prose, and they carry a test name beside
    them, so the emphasis travels as data and the template is what turns it into markup.
    """

    text: str
    emphasis: bool = False


def _emphasised(text: str) -> Part:
    return Part(text, emphasis=True)


def _diverged_sentence(conviction: Conviction) -> 'tuple[Part, ...]':
    """What the heads say, with each piece dropped rather than rendered when it was never stored: a
    build ingested before `github.head.sha` was recorded has no head to name, and a row with no
    `landed_at` has no sha to name it landed as."""
    convicted = (f'Convicted on head {conviction.tested_sha[:8]}' if conviction.tested_sha
                 else 'Convicted on a head this build did not record')
    subject = f'PR {conviction.pr_id}' if conviction.pr_id is not None else 'the pull request'
    landed = f' and landed as {conviction.newest_sha[:8]}' if conviction.newest_sha else ''
    return (
        Part(f'{convicted}, but {subject} was built '),
        _emphasised(f'{conviction.builds} times'),
        Part(' across '),
        _emphasised(f'{conviction.heads} heads'),
        Part(f'{landed}.'),
    )


def _currency_clause(conviction: Conviction) -> 'tuple[Part, ...]':
    """What main is doing with the test now, or nothing at all when nobody has asked.

    An unchecked escape gets no clause rather than a hedged one: a sentence that mentions the last
    week at all implies main was asked about it.
    """
    state = conviction.currency
    if state == STILL_FAILING:
        return (
            Part(' Main is still failing it, '),
            _emphasised(f'{conviction.recent_failed} of {conviction.recent_runs}'),
            Part(f' runs in the last {config.CURRENCY_DAYS} days.'),
        )
    if state == RECOVERED:
        return (
            Part(' Main has stopped failing it: none of its '),
            _emphasised(f'{conviction.recent_runs} runs'),
            Part(f' in the last {config.CURRENCY_DAYS} days did.'),
        )
    if state == NOT_RUN_LATELY:
        return (
            Part(f' Main has not run it in the last {config.CURRENCY_DAYS} days, so whether the '
                 'failure is still there is unmeasured.'),
        )
    return ()


def _escaped_sentence(conviction: Conviction) -> 'tuple[Part, ...]':
    """The counts behind the escape, how hard it failed, and what main is doing with it now."""
    if conviction.rarity == RARE:
        rate = Part(f', below the {config.ESCAPE_FAILURE_PCT}% a strong escape needs, so the '
                    'escape rests on few failures,')
    else:
        rate = Part(f', at or above the {config.ESCAPE_FAILURE_PCT}% a strong escape needs,')
    return (
        Part('Main failed it '),
        _emphasised(f'{conviction.failed_after} of {conviction.runs_after}'),
        Part(' runs after the landing'),
        rate,
        Part(' having never failed it in the '),
        _emphasised(str(conviction.runs_before)),
        Part(' runs before.'),
    ) + _currency_clause(conviction)


def sentence(conviction: Conviction) -> 'tuple[Part, ...]':
    """Why this conviction reached the verdict it did, in the counts main was asked for."""
    if conviction.verdict == FAILS_ON_MAIN:
        return (
            Part('Main failed it '),
            _emphasised(f'{conviction.failed_after} of {conviction.runs_after}'),
            Part(' runs '),
            _emphasised('after'),
            Part(' the landing vs. '),
            _emphasised(f'{conviction.failed_before} of {conviction.runs_before}'),
            Part(' before — main\'s failure, not this change\'s.'),
        )
    if conviction.verdict == CONTAINED:
        return (
            Part('Main ran it '),
            _emphasised(f'{conviction.runs_after} times'),
            Part(' after the landing and never failed it.'),
        )
    if conviction.verdict == NO_RUNS:
        return (Part(f'No bot ran it on main in the {config.ESCAPE_WINDOW_DAYS} days after the '
                     'landing, so there is nothing to compare against.'),)
    if conviction.verdict == NO_BASELINE:
        return (
            Part('Main failed it '),
            _emphasised(f'{conviction.failed_after} of {conviction.runs_after}'),
            Part(' runs after the landing, but nothing ran it in the '
                 f'{config.ESCAPE_WINDOW_DAYS} days before, so a regression cannot be told from a '
                 'failure main already had.'),
        )
    if conviction.verdict == TREE_DIVERGED:
        return _diverged_sentence(conviction)
    return _escaped_sentence(conviction)
