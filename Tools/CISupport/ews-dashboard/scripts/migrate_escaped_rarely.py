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

"""Re-decide every stored escape verdict under the rule where the baseline, not the rate, decides.

    python3 -m scripts.migrate_escaped_rarely
    python3 -m scripts.migrate_escaped_rarely --database other.db

Until now a conviction whose test failed on main after the landing in under ESCAPE_FAILURE_PCT of the
runs was called FAILS_ON_MAIN whatever the baseline said, so a test main had never failed before the
change landed and failed once after it was recorded as main's own failure. Those rows are the
population this check exists to find, and they are all stored under the wrong name.

Nothing has to be asked of results.webkit.org again: every row carries the runs and failures either
side of the landing, so `escapes.redecided` reaches the answer offline from the counts already there.

The table is rebuilt rather than updated in place, because db.initialize() creates the table with
`CREATE TABLE IF NOT EXISTS`, so an edit to schema.sql never reaches a database that already has one.

Rarity plays no part in the rename: a row this redecides into ESCAPED lands there whatever the failure
rate after the landing was, because nothing here stores a rate. `rarity_for_counts` reads it off
`runs_after` and `failed_after` wherever an escape is shown instead, so a rarity beside the counts can
never disagree with them.

Run once. A second run finds nothing to do and says so.
"""

from __future__ import annotations

import argparse
import os
import sqlite3
import sys

from ews_dashboard import config, db
from ews_dashboard.analysis import escapes
from scripts.migrate_verdict_names import (COPY_BATCH_ROWS, TABLE, extra_columns, live_table_sql,
                                           missing_columns, permitted_verdicts, schema_table_sql,
                                           verdict_counts)

TEMPORARY_TABLE = f'{TABLE}_redecided'

COUNT_COLUMNS = ('runs_before', 'failed_before', 'runs_after', 'failed_after')


def redecided_row(row: sqlite3.Row) -> str:
    """What one stored row's verdict should be called now.

    Delegates to `escapes.redecided` rather than restating the rule, so the migration and the assess
    pass cannot drift apart.
    """
    return escapes.redecided(row['verdict'], *(row[column] for column in COUNT_COLUMNS))


def stale_verdicts(connection: sqlite3.Connection) -> int:
    """How many stored rows the current rule would name differently."""
    return sum(1 for row in connection.execute(
        f'SELECT verdict, {", ".join(COUNT_COLUMNS)} FROM {TABLE}',
    ) if redecided_row(row) != row['verdict'])


def needs_migration(connection: sqlite3.Connection) -> bool:
    """Whether any of the three cases that call for a rebuild is still present.

    The rows, the constraint and the columns fail separately: a database with nothing left to
    re-decide can still reject the verdict the assess pass now produces, and either of those can
    already agree with schema.sql while the table itself still predates `landed_at` and the currency
    columns added since, or still carries a column schema.sql no longer declares, either of which
    makes a plain UPDATE impossible — the latter is a rebuild `migrate()` refuses to do silently.
    """
    if permitted_verdicts(live_table_sql(connection)) != permitted_verdicts(schema_table_sql()):
        return True
    if missing_columns(connection) or extra_columns(connection):
        return True
    return stale_verdicts(connection) > 0


def _companion_objects(connection: sqlite3.Connection) -> list:
    """The indexes and triggers on the table, which dropping it takes with it.

    `sql IS NULL` marks the index sqlite derives from the PRIMARY KEY; that one comes back with the
    CREATE TABLE and must not be replayed.
    """
    return [row['sql'] for row in connection.execute(
        '''SELECT sql FROM sqlite_master
           WHERE tbl_name = ? AND type IN ('index', 'trigger') AND sql IS NOT NULL''',
        (TABLE,),
    )]


def _copy_rows(connection: sqlite3.Connection, columns: list) -> int:
    quoted = ', '.join(f'"{column}"' for column in columns)
    placeholders = ', '.join('?' * len(columns))
    verdict_at = columns.index('verdict')
    cursor = connection.execute(f'SELECT {quoted} FROM {TABLE}')
    copied = 0
    while True:
        batch = cursor.fetchmany(COPY_BATCH_ROWS)
        if not batch:
            return copied
        rewritten = []
        for row in batch:
            values = list(row)
            values[verdict_at] = redecided_row(row)
            rewritten.append(values)
        connection.executemany(
            f'INSERT INTO {TEMPORARY_TABLE} ({quoted}) VALUES ({placeholders})', rewritten,
        )
        copied += len(rewritten)


def migrate(connection: sqlite3.Connection) -> int:
    """Rebuild the table under the current schema with every verdict decided again from its counts.

    Foreign keys go off for the duration, as sqlite's own table-rebuild recipe requires: the rows
    reference `build_verdicts`, and dropping the old table with them on would either refuse or
    cascade. `foreign_key_check` inside the transaction proves nothing was orphaned before anything
    is committed.
    """
    try:
        connection.execute('PRAGMA foreign_keys = OFF')
        connection.execute('BEGIN IMMEDIATE')
        companions = _companion_objects(connection)
        columns = [row['name'] for row in connection.execute(f'PRAGMA table_info({TABLE})')]
        extra = extra_columns(connection)
        if extra:
            raise RuntimeError(f'{TABLE} has {sorted(extra)}, which schema.sql no longer declares; '
                               'nothing was changed')
        connection.execute(f'DROP TABLE IF EXISTS {TEMPORARY_TABLE}')
        connection.execute(schema_table_sql(name=TEMPORARY_TABLE))
        copied = _copy_rows(connection, columns)
        connection.execute(f'DROP TABLE {TABLE}')
        connection.execute(f'ALTER TABLE {TEMPORARY_TABLE} RENAME TO {TABLE}')
        for statement in companions:
            connection.execute(statement)
        orphaned = connection.execute('PRAGMA foreign_key_check').fetchall()
        if orphaned:
            raise RuntimeError(f'{len(orphaned)} rows would be left orphaned; nothing was changed')
    except BaseException:
        if connection.in_transaction:
            connection.execute('ROLLBACK')
        raise
    else:
        connection.execute('COMMIT')
    finally:
        connection.execute('PRAGMA foreign_keys = ON')
    return copied


def _print_counts(label: str, counts: dict) -> None:
    print(f'  {label}')
    if not counts:
        print('    no rows')
        return
    width = max(len(verdict) for verdict in counts)
    for verdict, count in counts.items():
        unknown = '' if verdict in escapes.VERDICTS else ' (no bucket)'
        print(f'    {verdict:<{width}}  {count:>6}{unknown}')
    print(f'    {"total":<{width}}  {sum(counts.values()):>6}')


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
            print(f'{path} is already migrated; no verdict to decide again and no column to add')
            _print_counts('verdicts stored', verdict_counts(connection))
            return 0
        print(f'Re-deciding {TABLE} in {path}')
        _print_counts('before', verdict_counts(connection))
        stale = stale_verdicts(connection)
        added = sorted(missing_columns(connection))
        copied = migrate(connection)
        print(f'  copied {copied} rows under the current constraint, {stale} decided differently')
        print(f'  columns added: {", ".join(added) or "none"}')
        _print_counts('after', verdict_counts(connection))
    finally:
        connection.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
