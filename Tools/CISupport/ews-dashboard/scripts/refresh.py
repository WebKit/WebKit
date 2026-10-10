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

"""Bring the database up to date: ingest builds, then classify what they surfaced.

This is the only thing in the repository that talks to the network, so it is also the only thing
that can be slow. Classification asks results.webkit.org about one test in one configuration at one
commit, which takes about 1.6 seconds, so every lookup a window needs is collected first and warmed
in parallel; the classify pass afterwards reads the cache and does no I/O of its own.

Run it from cron or by hand. The web app never runs it, and a page served while it is halfway
through shows the builds it has already finished plus a count of the ones it has not.

The default window is the widest one a page offers, because a verdict exists only for what this
script assessed while the convictions it could not be asked about are recounted per request over the
whole window a reader picked: a narrower refresh than the widest choice makes the widest choice look
worse than it is purely because nothing ever looked.
"""

from __future__ import annotations

import argparse
import datetime
import sqlite3
import sys
import time
from typing import Optional

from ews_dashboard import buildbot, config, db, ingest, landings, results, webkit_checkout
from ews_dashboard.analysis import escapes, false_positive, trend

# The widest of the web app's WINDOW_CHOICES, so a default refresh assesses everything a page can be
# asked to show.
DEFAULT_DAYS = 90


def _window(days: int) -> tuple:
    until = trend.day_bounds(trend.today())[1]
    return until - days * 86400, until


def _begin_run(connection: sqlite3.Connection, started_at: int) -> None:
    with connection:
        connection.execute('INSERT OR REPLACE INTO refresh_runs (started_at) VALUES (?)',
                           (started_at,))


def _finish_run(connection: sqlite3.Connection, started_at: int, report: ingest.IngestReport,
                builders_walked: int) -> None:
    with connection:
        connection.execute(
            '''UPDATE refresh_runs
               SET finished_at = ?, builders_walked = ?, builds_ingested = ?, builds_failed = ?
               WHERE started_at = ?''',
            (int(time.time()), builders_walked,
             report.outcomes['ingested'] + report.outcomes['reingested'], report.failed, started_at),
        )


def _fail_run(connection: sqlite3.Connection, started_at: int, error: BaseException,
              report: ingest.IngestReport) -> None:
    """Record why a run died, so the pages can say the numbers stopped moving on purpose.

    `finished_at` stays null: a failed run did not finish, and every freshness answer treats it as
    the stale run it is. `builds_ingested` and `builds_failed` still record whatever `report`
    accumulated before the error, so a run that died halfway reports what it did manage to store and
    fail rather than nothing.
    """
    with connection:
        connection.execute(
            'UPDATE refresh_runs SET error = ?, builds_ingested = ?, builds_failed = ? WHERE started_at = ?',
            (f'{type(error).__name__}: {error}',
             report.outcomes['ingested'] + report.outcomes['reingested'], report.failed, started_at),
        )


def ingest_builds(connection: sqlite3.Connection, client: buildbot.BuildbotClient, since: int,
                  builder: Optional[str], force: bool, report: ingest.IngestReport) -> int:
    names = [builder] if builder else ingest.dashboard_builder_names(client)
    for name in names:
        print(f'  {name}', flush=True)
        report.add(ingest.ingest_builder(connection, client, name, since=since, force=force))
    return len(names)


def classify_builds(connection: sqlite3.Connection, history: results.History,
                    since: int, until: int) -> false_positive.Counts:
    builds = false_positive.failing_builds(connection, since, until)
    history.prefetch(false_positive.pending_queries(connection, builds))
    return false_positive.rate(
        connection, false_positive.live_classifier(connection, history), since, until,
    )


def check_escapes(connection: sqlite3.Connection, history: results.History,
                  since: int, until: int, checkout_path: str) -> None:
    """Find where each convicted pull request landed, then ask main what it did with the test.

    A checkout that cannot be read stops this pass and nothing else: the builds are already ingested
    and classified by now, and the convictions it could not decide read as undecided on the page
    rather than as convictions main agreed with.
    """
    try:
        print('Resolving landings')
        resolved = landings.resolve(connection, webkit_checkout.Checkout(checkout_path),
                                    since, until)
        print(f'  {resolved}')
        print('Asking main what it did with each convicted test')
        print(f'  {dict(escapes.assess(connection, history, since, until))}')
        print(f'  could not be asked: {escapes.unaskable(connection, since, until)}')
    except webkit_checkout.CheckoutUnavailable as error:
        print(f'escape detection skipped: {error}', file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--days', type=int, default=DEFAULT_DAYS,
                        help=f'how far back to walk builds (default {DEFAULT_DAYS})')
    parser.add_argument('--builder', help='one builder rather than every builder EWS exposes')
    parser.add_argument('--force', action='store_true',
                        help='re-read builds already stored, discarding their classifications')
    parser.add_argument('--skip-ingest', action='store_true',
                        help='classify what is already stored without touching Buildbot')
    parser.add_argument('--skip-escapes', action='store_true',
                        help='skip the landing search and the check of what main did afterwards')
    parsed = parser.parse_args()

    since, until = _window(parsed.days)
    db.initialize()
    connection = db.connect()
    started_at = int(time.time())
    _begin_run(connection, started_at)
    report = ingest.IngestReport()
    try:
        builders_walked = _refresh(connection, parsed, since, until, report)
    except BaseException as error:
        _fail_run(connection, started_at, error, report)
        connection.close()
        print(f'refresh failed: {type(error).__name__}: {error}', file=sys.stderr)
        raise
    _finish_run(connection, started_at, report, builders_walked)
    connection.close()
    return 0


def _refresh(connection: sqlite3.Connection, parsed: argparse.Namespace,
             since: int, until: int, report: ingest.IngestReport) -> int:
    builders_walked = 0
    if not parsed.skip_ingest:
        walked_from = datetime.datetime.fromtimestamp(since, datetime.timezone.utc).date()
        print(f'Ingesting builds since {walked_from.isoformat()}')
        builders_walked = ingest_builds(
            connection, buildbot.BuildbotClient(), since, parsed.builder, parsed.force, report,
        )
        for error in report.errors[:10]:
            print(f'  {error}', file=sys.stderr)
        print(f'  {dict(report.outcomes)}, {report.failed} failed')

    print('Classifying author-visible failures')
    history = results.History(connection)
    counts = classify_builds(connection, history, since, until)
    print(f'  {counts.classifiable} classifiable builds, '
          f'{counts.author_fp_rate_pct}% blamed an author for noise')

    checkout_path = config.checkout_path()
    if parsed.skip_escapes or not checkout_path:
        print(f'Skipping escape detection; point {config.CHECKOUT_PATH_VARIABLE} at a WebKit '
              'checkout to enable it')
    else:
        check_escapes(connection, history, since, until, checkout_path)

    print(f'  results.webkit.org: {dict(history.stats)}')
    return builders_walked


if __name__ == '__main__':
    sys.exit(main())
