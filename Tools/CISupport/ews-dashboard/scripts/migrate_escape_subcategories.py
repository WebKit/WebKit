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

"""Give the table `landed_at` and its currency columns, and fold in ESCAPED_RARELY should a row ever
carry it.

    python3 -m scripts.migrate_escape_subcategories
    python3 -m scripts.migrate_escape_subcategories --database other.db

The rebuild adds `landed_at`, plus the currency columns `recent_runs`, `recent_failed` and
`recent_checked_at`, which hold whether main is still failing an escaped test. They arrive null,
which is what says nobody has asked yet: the
assess pass fills them in for the escapes alone.

ESCAPED_RARELY never reached a committed schema.sql — no CHECK constraint this codebase has ever
declared has permitted it — so the fold below has nothing to do against a real database. It stays
in because it costs nothing to leave in and because ESCAPED_RARELY and ESCAPED were always the same
answer to the one question a verdict here answers: whether main failed the test after the landing
without having failed it before. The rate is read off `runs_after` and `failed_after` wherever an
escape is shown, so a rarity beside the counts can never disagree with them.

The table is rebuilt rather than altered because db.initialize() creates it with `CREATE TABLE IF NOT
EXISTS`, so an edit to schema.sql never reaches a database that already has one — neither the current
CHECK constraint nor the three new columns.

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

TEMPORARY_TABLE = f'{TABLE}_subcategorised'

RETIRED_VERDICT = 'ESCAPED_RARELY'


def mapped_verdict(verdict: str) -> str:
    """What a stored verdict name should be called now."""
    return escapes.ESCAPED if verdict == RETIRED_VERDICT else verdict


def retired_rows(connection: sqlite3.Connection) -> int:
    return verdict_counts(connection).get(RETIRED_VERDICT, 0)


def needs_migration(connection: sqlite3.Connection) -> bool:
    """Whether any of the three cases that call for a rebuild is still present.

    The constraint, the rows and the columns fail separately: a database with no ESCAPED_RARELY row
    left can still permit the name, and one that permits exactly the right names can still be missing
    `landed_at` or the currency columns, or still carry a column schema.sql no longer declares, either
    of which makes a plain UPDATE impossible — the latter is a rebuild `migrate()` refuses to do
    silently.
    """
    if permitted_verdicts(live_table_sql(connection)) != permitted_verdicts(schema_table_sql()):
        return True
    if missing_columns(connection) or extra_columns(connection):
        return True
    return retired_rows(connection) > 0


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
    """Copy the live table's own columns across, renaming the retired verdict on the way.

    Only the columns the live table has are named, so the ones this migration adds take their
    declared default of null: a row copied from before the currency check existed has not been
    checked, and that is exactly what null says.
    """
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
            values[verdict_at] = mapped_verdict(values[verdict_at])
            rewritten.append(values)
        connection.executemany(
            f'INSERT INTO {TEMPORARY_TABLE} ({quoted}) VALUES ({placeholders})', rewritten,
        )
        copied += len(rewritten)


def migrate(connection: sqlite3.Connection) -> int:
    """Rebuild the table under the current schema with the retired verdict folded into ESCAPED.

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
            print(f'{path} is already migrated; no verdict to fold and no column to add')
            _print_counts('verdicts stored', verdict_counts(connection))
            return 0
        print(f'Rebuilding {TABLE} in {path}')
        _print_counts('before', verdict_counts(connection))
        retired, added = retired_rows(connection), sorted(missing_columns(connection))
        copied = migrate(connection)
        print(f'  copied {copied} rows, {retired} of them from {RETIRED_VERDICT}')
        print(f'  columns added: {", ".join(added) or "none"}')
        _print_counts('after', verdict_counts(connection))
    finally:
        connection.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
