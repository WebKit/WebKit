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

"""Give every stored escape verdict the landing time it was decided about.

    python3 -m scripts.migrate_verdict_landed_at
    python3 -m scripts.migrate_verdict_landed_at --database other.db

The table stored `window_ends_at` and nothing else about the landing, so the page back-derived the
landing time as `window_ends_at - ESCAPE_WINDOW_DAYS * 86400`. That reads the current setting
against a row counted under whatever the setting was then: changing ESCAPE_WINDOW_DAYS moved the
landing date of every already-stored row, while its runs either side stayed attached to the window
really counted.
Both facts are now stored, and this fills the new column in from `landings` for the rows already
there.

The column is nullable, so a plain ALTER TABLE reaches a live table and no rebuild is needed. A row
whose pull request has no landed row left in `landings` keeps a NULL rather than a guess, and the
page says the landing time is unknown; the count of those is printed, because a large one means the
landings cache was cleared rather than that this migration failed.

Guarded on the column set as well as on the rows: a database that has the column but unfilled rows,
and one whose rows were filled by a rebuild before the column existed here, are different failures,
and gating on either alone makes the script a no-op on a database that still needs it.

Run once. A second run finds nothing to do and says so.
"""

from __future__ import annotations

import argparse
import os
import sqlite3
import sys

from ews_dashboard import config, db, webkit_checkout
from scripts.migrate_verdict_names import TABLE, missing_columns

COLUMN = 'landed_at'

# The same join the candidates query makes: a verdict names a build, a build names a pull request,
# and only a pull request that landed has a landing time to take.
BACKFILL_SQL = f'''
    UPDATE {TABLE} SET {COLUMN} = (
        SELECT landing.landed_at
        FROM build_verdicts AS build
        JOIN landings AS landing ON landing.pr_id = build.pr_id
        WHERE build.build_id = {TABLE}.build_id
          AND landing.status = ? AND landing.landed_at IS NOT NULL
    )
    WHERE {COLUMN} IS NULL
'''


def column_missing(connection: sqlite3.Connection) -> bool:
    return COLUMN in missing_columns(connection)


def unfilled_rows(connection: sqlite3.Connection) -> int:
    """Stored verdicts with no landing time, which is every row until this has run."""
    if column_missing(connection):
        return connection.execute(f'SELECT COUNT(*) FROM {TABLE}').fetchone()[0]
    return connection.execute(
        f'SELECT COUNT(*) FROM {TABLE} WHERE {COLUMN} IS NULL',
    ).fetchone()[0]


def backfillable_rows(connection: sqlite3.Connection) -> int:
    """Rows with no landing time that a landing can still be found for.

    Counted separately from `unfilled_rows` so a database whose remaining NULLs are all unjoinable
    reports itself as migrated instead of being rewritten on every run.
    """
    if column_missing(connection):
        return 0
    return connection.execute(
        f'''SELECT COUNT(*) FROM {TABLE} AS outcome
            JOIN build_verdicts AS build USING (build_id)
            JOIN landings AS landing ON landing.pr_id = build.pr_id
            WHERE outcome.{COLUMN} IS NULL
              AND landing.status = ? AND landing.landed_at IS NOT NULL''',
        (webkit_checkout.LANDED,),
    ).fetchone()[0]


def needs_migration(connection: sqlite3.Connection) -> bool:
    """Whether either half of the problem is still present.

    The column and the rows fail separately: a table whose constraint matches schema.sql can
    still be missing a column, and a table that has the column can still hold rows nothing filled it
    in for.
    """
    if column_missing(connection):
        return True
    return backfillable_rows(connection) > 0


def add_column(connection: sqlite3.Connection) -> None:
    with connection:
        connection.execute(f'ALTER TABLE {TABLE} ADD COLUMN {COLUMN} INTEGER')


def backfill(connection: sqlite3.Connection) -> None:
    """Fill the landing time in from `landings` wherever one can be found.

    A row the join misses is set to NULL again, which is what it already was, so the statement's
    rowcount says nothing about how many took a time; the caller counts the NULLs either side.
    """
    with connection:
        connection.execute(BACKFILL_SQL, (webkit_checkout.LANDED,))


def migrate(connection: sqlite3.Connection) -> 'tuple[int, int]':
    """Add the column if it is absent and backfill it, returning the rows filled and the rows left.

    The two numbers are reported rather than summed: rows left over are pull requests `landings` no
    longer holds an answer for, which is a gap in the cache and not a failure here.
    """
    if column_missing(connection):
        add_column(connection)
    unfilled_before = unfilled_rows(connection)
    backfill(connection)
    left = unfilled_rows(connection)
    return unfilled_before - left, left


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--database', default=None)
    arguments = parser.parse_args()

    path = arguments.database or config.database_path()
    if not os.path.exists(path):
        print(f'no database at {path}', file=sys.stderr)
        return 1

    connection = db.connect(path)
    try:
        if not needs_migration(connection):
            print(f'{path} is already migrated; the column is there and every landing found has been filled in')
            print(f'  rows with no landing time: {unfilled_rows(connection)}')
            return 0
        print(f'Backfilling {TABLE}.{COLUMN} in {path}')
        added = column_missing(connection)
        filled, left = migrate(connection)
        print(f'  column added: {"yes" if added else "no, it was already there"}')
        print(f'  rows backfilled from landings: {filled}')
        print(f'  rows left with no landing time: {left}')
    finally:
        connection.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
