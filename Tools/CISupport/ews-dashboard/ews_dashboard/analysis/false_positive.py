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

"""How often EWS blames an author for a failure that was not theirs.

A build's author-visible failures are (first run ∩ rerun) − clean tree: the set behind "Found N new
test failures". Each of those tests is asked about on main at the commit the change was rebased
onto, and lands in one of three groups:

  pre-existing   main already fails it at or below the pass-rate threshold, so the change is not
                 the cause and the author was blamed for noise
  real           main passes it reliably, so the change is the likely cause
  undetermined   nothing is recorded for that test in that configuration

The build then falls into one bucket:

  CLEAN         every author-visible test is real
  PARTIAL_FP    at least one real and at least one pre-existing
  FALSE_RED     every author-visible test is pre-existing
  UNDETERMINED  nothing classifiable
  (no bucket)   the build surfaced nothing to the author at all, so it has no bearing on the rate

What this cannot do: main does not contain the change, so a test that is reliable on main and
genuinely broken by the change is indistinguishable from one that is reliable on main and flaked
once during this build. The rate is therefore a floor on blame noise, not a verdict on any single
build.
"""

from __future__ import annotations

import json
import sqlite3
import time
from dataclasses import dataclass, field
from typing import Callable, Iterator, Optional

from ews_dashboard import config, queues, results

# A build classified while its tests had no recorded history may be classifiable now, so those rows
# expire. A build that classified every test keeps its answer forever.
UNDETERMINED_TTL_SECONDS = 7 * 24 * 3600

CLEAN = 'CLEAN'
PARTIAL_FP = 'PARTIAL_FP'
FALSE_RED = 'FALSE_RED'
UNDETERMINED = 'UNDETERMINED'

PRE_EXISTING = 'pre_existing'
REAL = 'real'
NO_HISTORY = 'no_history'
UNQUERIED = 'unqueried'

VERDICT_DESCRIPTIONS = {
    REAL: f'Main passed this test more than {config.PRE_EXISTING_THRESHOLD_PCT}% of the time at the '
          'commit the change was rebased onto, so the change is the likely cause.',
    PRE_EXISTING: f'Main passed this test {config.PRE_EXISTING_THRESHOLD_PCT}% of the time or less '
                  'at the commit the change was rebased onto, so the author was blamed for noise.',
    NO_HISTORY: 'results.webkit.org has no runs recorded for this test in this configuration, so '
                'there is no pass rate to compare against the threshold.',
    UNQUERIED: 'Nobody has looked this one up.',
}

# The two states below are not stored buckets — `formatting.NO_SURFACED` and `formatting.UNCLASSIFIED`
# — but a reader sees them wherever a bucket appears, so they need the same gloss. Spelled out as
# literals rather than imported, since `formatting` imports this module and not the other way round.
BUCKET_DESCRIPTIONS = {
    CLEAN: 'Every author-visible failure is real, so none of them is noise.',
    PARTIAL_FP: 'The author-visible failures are a mix of real and pre-existing, so the build '
                'blamed some noise alongside a real failure.',
    FALSE_RED: 'Every author-visible failure is pre-existing, so the whole build blamed noise.',
    UNDETERMINED: 'The build surfaced failures but none of them could be classified against main, '
                  'whether for missing history or because the build itself cannot be trusted.',
    'no_surfaced': 'The build showed its author no failures at all, so it has no bucket and no '
                   'bearing on the rate.',
    'unclassified': 'No refresh has reached this build yet, so nothing has been classified.',
}

TRUNCATED_LISTS = 'truncated_lists'
NO_BASE_COMMIT = 'no_base_commit'
TOO_MANY_SURFACED = 'too_many_surfaced'

REASON_DESCRIPTIONS = {
    TRUNCATED_LISTS: 'the run hit the bot\'s failure ceiling, so its failure lists stop at an '
                     'arbitrary point',
    NO_BASE_COMMIT: 'the build recorded no base identifier, so any lookup would answer about the '
                    'tip of the tree instead',
    TOO_MANY_SURFACED: f'more than {config.MAX_CLASSIFIABLE_SURFACED_TESTS} tests were surfaced, '
                       'more than a change plausibly breaks, which is a broken checkout or a crash '
                       'storm rather than a change under test',
}


@dataclass(frozen=True)
class Classification:
    """One build's outcome. `bucket` is None when the build surfaced nothing to the author."""

    bucket: Optional[str]
    surfaced_total: int = 0
    surfaced_pre_existing: int = 0
    surfaced_real: int = 0
    surfaced_undetermined: int = 0


# Returns None for a build nobody has classified yet, which is not the same as a build with nothing
# to classify.
Classifier = Callable[[sqlite3.Row], Optional[Classification]]


@dataclass
class Counts:
    clean: int = 0
    partial_fp: int = 0
    false_red: int = 0
    undetermined: int = 0
    no_surfaced: int = 0
    unclassified: int = 0
    surfaced_total: int = 0
    surfaced_pre_existing: int = 0
    surfaced_real: int = 0
    surfaced_undetermined: int = 0

    def record(self, classification: Optional[Classification]) -> None:
        if classification is None:
            self.unclassified += 1
            return
        if classification.bucket is None:
            self.no_surfaced += 1
            return
        self.surfaced_total += classification.surfaced_total
        self.surfaced_pre_existing += classification.surfaced_pre_existing
        self.surfaced_real += classification.surfaced_real
        self.surfaced_undetermined += classification.surfaced_undetermined
        if classification.bucket == CLEAN:
            self.clean += 1
        elif classification.bucket == PARTIAL_FP:
            self.partial_fp += 1
        elif classification.bucket == FALSE_RED:
            self.false_red += 1
        else:
            self.undetermined += 1

    def merge(self, other: 'Counts') -> None:
        self.clean += other.clean
        self.partial_fp += other.partial_fp
        self.false_red += other.false_red
        self.undetermined += other.undetermined
        self.no_surfaced += other.no_surfaced
        self.unclassified += other.unclassified
        self.surfaced_total += other.surfaced_total
        self.surfaced_pre_existing += other.surfaced_pre_existing
        self.surfaced_real += other.surfaced_real
        self.surfaced_undetermined += other.surfaced_undetermined

    @property
    def classifiable(self) -> int:
        """Builds the rate is over. Every rate below is None rather than 0.0 when this is zero,
        because a percentage over no builds reads as a perfect score."""
        return self.clean + self.partial_fp + self.false_red

    @property
    def author_fp_rate_pct(self) -> Optional[float]:
        return self._share(self.partial_fp + self.false_red, self.classifiable)

    @property
    def false_red_rate_pct(self) -> Optional[float]:
        return self._share(self.false_red, self.classifiable)

    @property
    def blame_noise_rate_pct(self) -> Optional[float]:
        return self._share(self.surfaced_pre_existing, self.surfaced_total)

    @staticmethod
    def _share(part: int, whole: int) -> Optional[float]:
        if not whole:
            return None
        return round(100.0 * part / whole, 1)


@dataclass
class Partition:
    real: list = field(default_factory=list)
    pre_existing: list = field(default_factory=list)
    undetermined: list = field(default_factory=list)

    def bucket(self) -> str:
        if self.real and self.pre_existing:
            return PARTIAL_FP
        if self.real:
            return CLEAN
        if self.pre_existing:
            return FALSE_RED
        return UNDETERMINED


def surfaced_tests(build_row: sqlite3.Row) -> list:
    """The tests the author was shown: what failed twice with the change and not without it."""
    def tests_in(column: str) -> set:
        raw = build_row[column]
        if not raw:
            return set()
        try:
            return set(json.loads(raw) or [])
        except ValueError:
            return set()

    return sorted((tests_in('first_run_failures') & tests_in('second_run_failures'))
                  - tests_in('clean_tree_run_failures'))


def base_commit_of(build_row: sqlite3.Row) -> Optional[str]:
    """The commit the change was rebased onto, as results.webkit.org knows it.

    Only the identifier qualifies. `sha` and `change_id` are the pull request's own head commit,
    which main does not contain and results.webkit.org has never recorded, so asking about either
    would silently answer about the tip of the tree instead.
    """
    return build_row['identifier'] or None


def undetermined_reason(build_row: sqlite3.Row) -> Optional[str]:
    """Why this build's history cannot be believed, whatever it surfaced, or None when it can.

    `REASON_DESCRIPTIONS` carries the sentence each one deserves on a page, so a build reads as
    unanswerable for a stated reason rather than as an unexplained gap.
    """
    if build_row['exceeded_failure_limit']:
        return TRUNCATED_LISTS
    if base_commit_of(build_row) is None:
        return NO_BASE_COMMIT
    if len(surfaced_tests(build_row)) > config.MAX_CLASSIFIABLE_SURFACED_TESTS:
        return TOO_MANY_SURFACED
    return None


def undeterminable(build_row: sqlite3.Row) -> bool:
    return undetermined_reason(build_row) is not None


def classifiable_tests(build_row: sqlite3.Row) -> Optional[list]:
    """The tests worth asking results.webkit.org about, or None when the build needs no lookup.

    `pending_queries` warms exactly these, and `classify` asks about exactly these. They must agree
    or the refresh warms the wrong cache and the serial pass pays 1.6 seconds a test to discover it.
    """
    if undeterminable(build_row):
        return None
    return surfaced_tests(build_row) or None


def partition(
    history: results.History,
    test_names: list,
    configuration: results.Configuration,
    commit_ref: str,
    threshold_pct: int,
) -> Partition:
    partitioned = Partition()
    for test_name in test_names:
        query = results.Query(test_name, configuration, commit_ref)
        try:
            pass_rate = history.pass_rate(query)
        except results.HistoryUnavailable:
            partitioned.undetermined.append(test_name)
            continue
        if pass_rate is None:
            partitioned.undetermined.append(test_name)
        elif pass_rate <= threshold_pct:
            partitioned.pre_existing.append(test_name)
        else:
            partitioned.real.append(test_name)
    return partitioned


def _cached_classification(
    connection: sqlite3.Connection, build_id: int, threshold_pct: int,
) -> Optional[Classification]:
    row = connection.execute(
        '''SELECT bucket, surfaced_total, surfaced_pre_existing, surfaced_real,
                  surfaced_undetermined, classified_at
           FROM build_classifications
           WHERE build_id = ? AND threshold_pct = ?''',
        (build_id, threshold_pct),
    ).fetchone()
    if row is None:
        return None
    if row['surfaced_undetermined'] and int(time.time()) - row['classified_at'] > UNDETERMINED_TTL_SECONDS:
        return None
    return Classification(
        bucket=row['bucket'],
        surfaced_total=row['surfaced_total'],
        surfaced_pre_existing=row['surfaced_pre_existing'],
        surfaced_real=row['surfaced_real'],
        surfaced_undetermined=row['surfaced_undetermined'],
    )


def _cache_classification(
    connection: sqlite3.Connection, build_id: int, threshold_pct: int,
    classification: Classification,
) -> None:
    with connection:
        connection.execute(
            '''INSERT OR REPLACE INTO build_classifications (
                build_id, threshold_pct, bucket, surfaced_total, surfaced_pre_existing,
                surfaced_real, surfaced_undetermined, classified_at
            ) VALUES (?,?,?,?,?,?,?,?)''',
            (build_id, threshold_pct, classification.bucket, classification.surfaced_total,
             classification.surfaced_pre_existing, classification.surfaced_real,
             classification.surfaced_undetermined, int(time.time())),
        )


def classify(
    connection: sqlite3.Connection,
    history: results.History,
    build_row: sqlite3.Row,
    threshold_pct: int = config.PRE_EXISTING_THRESHOLD_PCT,
) -> Classification:
    """One build's bucket, cached by (build, threshold).

    A build `classifiable_tests` refuses is UNDETERMINED without a single lookup, except when it
    surfaced nothing at all: that build showed the author no failures, so there is no bucket to put
    it in and it belongs in no denominator.
    """
    cached = _cached_classification(connection, build_row['build_id'], threshold_pct)
    if cached is not None:
        return cached

    surfaced = surfaced_tests(build_row)
    classifiable = classifiable_tests(build_row)
    if not surfaced:
        classification = Classification(None)
    elif classifiable is None:
        classification = Classification(
            bucket=UNDETERMINED,
            surfaced_total=len(surfaced),
            surfaced_pre_existing=0,
            surfaced_real=0,
            surfaced_undetermined=len(surfaced),
        )
    else:
        partitioned = partition(
            history, classifiable, results.Configuration.of_build(build_row),
            base_commit_of(build_row) or '', threshold_pct,
        )
        classification = Classification(
            bucket=partitioned.bucket(),
            surfaced_total=len(surfaced),
            surfaced_pre_existing=len(partitioned.pre_existing),
            surfaced_real=len(partitioned.real),
            surfaced_undetermined=len(partitioned.undetermined),
        )

    _cache_classification(connection, build_row['build_id'], threshold_pct, classification)
    return classification


@dataclass(frozen=True)
class SurfacedTest:
    """One test a build showed its author, and the answer that put it where it is."""

    name: str
    verdict: str
    pass_rate: Optional[float]


def explain(
    connection: sqlite3.Connection,
    build_row: sqlite3.Row,
    threshold_pct: int = config.PRE_EXISTING_THRESHOLD_PCT,
) -> list:
    """Every test this build showed its author, and why each one landed where it did.

    The same partition `classify` made, per test instead of as counts, read from the cache a refresh
    filled and never from the network. A test whose answer is not cached reads UNQUERIED rather than
    as having no history: the first sends a reader to the refresh, the second to results.webkit.org.
    """
    configuration = results.Configuration.of_build(build_row)
    commit_ref = base_commit_of(build_row) or ''
    answerable = undetermined_reason(build_row) is None
    explained = []
    for test_name in surfaced_tests(build_row):
        answer = results.cached_answer(
            connection, results.Query(test_name, configuration, commit_ref),
        ) if answerable else None
        explained.append(SurfacedTest(
            name=test_name,
            verdict=_verdict(answer, threshold_pct),
            pass_rate=None if answer is None else answer.pass_rate,
        ))
    return explained


def _verdict(answer: Optional[results.Answer], threshold_pct: int) -> str:
    if answer is None:
        return UNQUERIED
    if answer.pass_rate is None:
        return NO_HISTORY
    return PRE_EXISTING if answer.pass_rate <= threshold_pct else REAL


BUILD_COLUMNS = '''build_id, builder, builder_id, build_number, pr_id, sha, change_id, identifier,
                   platform, style, flavor, suite, exceeded_failure_limit,
                   first_run_failures, second_run_failures, clean_tree_run_failures, started_at'''


def _failing_where(since: int, until: int, suite: Optional[str],
                   builders: tuple = ()) -> tuple:
    """The WHERE clause every failing-build query shares, and its parameters, returned together so a
    caller cannot bind one without the other."""
    conditions = ["verdict = 'FAILURE'", 'started_at >= :since', 'started_at < :until']
    parameters = {'since': since, 'until': until}
    if suite is not None:
        conditions.append('suite = :suite')
        parameters['suite'] = suite
    fragment, builder_parameters = queues.builder_filter(builders, column='builder')
    if fragment:
        conditions.append(fragment)
        parameters.update(builder_parameters)
    return ' AND '.join(conditions), parameters


def failing_builds(
    connection: sqlite3.Connection,
    since: int,
    until: int,
    suite: Optional[str] = None,
    builders: tuple = (),
    limit: Optional[int] = None,
) -> list:
    """Failing builds, newest first. Unlimited by default because the refresh classifies all of
    them; a page passes a limit and reports the rest with `failing_build_count`."""
    where, parameters = _failing_where(since, until, suite, builders)
    clause = ''
    if limit is not None:
        clause = ' LIMIT :limit'
        parameters['limit'] = limit
    return connection.execute(
        f'''SELECT {BUILD_COLUMNS}
            FROM build_verdicts
            WHERE {where}
            ORDER BY started_at DESC, build_id DESC{clause}''',
        parameters,
    ).fetchall()


def failing_build_count(
    connection: sqlite3.Connection,
    since: int,
    until: int,
    suite: Optional[str] = None,
    builders: tuple = (),
) -> int:
    where, parameters = _failing_where(since, until, suite, builders)
    return connection.execute(
        f'SELECT COUNT(*) FROM build_verdicts WHERE {where}', parameters,
    ).fetchone()[0]


def failing_build(connection: sqlite3.Connection, build_id: int) -> Optional[sqlite3.Row]:
    """One failing build by id, so a page can open a build that falls outside the page it listed."""
    return connection.execute(
        f"SELECT {BUILD_COLUMNS} FROM build_verdicts WHERE build_id = ? AND verdict = 'FAILURE'",
        (build_id,),
    ).fetchone()


def pending_queries(
    connection: sqlite3.Connection,
    build_rows: list,
    threshold_pct: int = config.PRE_EXISTING_THRESHOLD_PCT,
) -> Iterator[results.Query]:
    """Every history lookup the given builds still need, for History.prefetch to warm in parallel.

    Classifying in series costs about 1.6 seconds per uncached test.
    """
    for build_row in build_rows:
        if _cached_classification(connection, build_row['build_id'], threshold_pct) is not None:
            continue
        classifiable = classifiable_tests(build_row)
        if classifiable is None:
            continue
        configuration = results.Configuration.of_build(build_row)
        commit_ref = base_commit_of(build_row) or ''
        for test_name in classifiable:
            yield results.Query(test_name, configuration, commit_ref)


def cached_classifier(
    connection: sqlite3.Connection,
    threshold_pct: int = config.PRE_EXISTING_THRESHOLD_PCT,
) -> Classifier:
    """Reads classifications and never makes one. What the web layer gets, so a page cannot reach
    the network on a request; a build nobody has classified yet is reported as unclassified."""
    def classifier(build_row: sqlite3.Row) -> Optional[Classification]:
        return _cached_classification(connection, build_row['build_id'], threshold_pct)
    return classifier


def live_classifier(
    connection: sqlite3.Connection,
    history: results.History,
    threshold_pct: int = config.PRE_EXISTING_THRESHOLD_PCT,
) -> Classifier:
    """Classifies what it has to, asking results.webkit.org. For refresh, never for a request."""
    def classifier(build_row: sqlite3.Row) -> Optional[Classification]:
        return classify(connection, history, build_row, threshold_pct)
    return classifier


def rate(
    connection: sqlite3.Connection,
    classifier: Classifier,
    since: int,
    until: int,
    suite: Optional[str] = None,
    builders: tuple = (),
) -> Counts:
    counts = Counts()
    for build_row in failing_builds(connection, since, until, suite=suite, builders=builders):
        counts.record(classifier(build_row))
    return counts
