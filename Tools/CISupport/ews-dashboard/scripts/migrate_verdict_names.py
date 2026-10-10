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

"""Fold the two escape verdict names 3761a9a retired into the one that replaced them.

    python3 -m scripts.migrate_verdict_names
    python3 -m scripts.migrate_verdict_names --database other.db

FLAKY_ON_MAIN and ALREADY_FAILING were merged into FAILS_ON_MAIN. A database created before that
change still holds the rows decided under the old names, and still has the CHECK constraint that
permits them and rejects the name that replaced them: schema.sql was updated, but the live table
survived on `CREATE TABLE IF NOT EXISTS` and was never rebuilt. The constraint therefore has to be
replaced before the names can be rewritten, which is why this copies the table rather than running
an UPDATE, which the old constraint would reject.

The runs either side of the landing are copied across untouched. They are the evidence that main
failed the test without the change, and the reason these rows are migrated rather than dropped.

Run once. A second run finds nothing to do and says so.
"""

from __future__ import annotations

import argparse
import os
import re
import sqlite3
import sys
from typing import Optional

from ews_dashboard import config, db
from ews_dashboard.analysis import escapes

TABLE = 'escape_verdicts'
TEMPORARY_TABLE = f'{TABLE}_migrated'

COPY_BATCH_ROWS = 500

# The names 3761a9a retired, and what each became. Both meant main failing the test without the
# change in it, which is the one thing FAILS_ON_MAIN says.
RETIRED_VERDICTS = {
    'FLAKY_ON_MAIN': escapes.FAILS_ON_MAIN,
    'ALREADY_FAILING': escapes.FAILS_ON_MAIN,
}


def mapped_verdict(verdict: str) -> str:
    """What a stored verdict name should be called now."""
    return RETIRED_VERDICTS.get(verdict, verdict)


def permitted_verdicts(table_sql: str) -> frozenset:
    """The verdict names a `CREATE TABLE` statement's CHECK constraint allows.

    Read out of the statement rather than assumed, because the whole reason for this migration is a
    live table whose constraint and schema.sql's had silently drifted apart.
    """
    match = re.search(r'verdict\s+IN\s*\((.*?)\)', table_sql, re.DOTALL | re.IGNORECASE)
    if not match:
        return frozenset()
    return frozenset(re.findall(r"'([^']+)'", match.group(1)))


def schema_table_sql(table: str = TABLE, name: Optional[str] = None) -> str:
    """The `CREATE TABLE` statement schema.sql declares, optionally under a different name.

    Taken from the file so the rebuilt table carries whatever the current schema permits; retyping
    the constraint list here would be the same mistake that let the live table fall behind.
    """
    with open(db.SCHEMA_PATH) as schema_file:
        schema = schema_file.read()
    match = re.search(
        rf'^CREATE TABLE (?:IF NOT EXISTS )?{table}\s*\((.*?)^\);',
        schema, re.DOTALL | re.MULTILINE,
    )
    if not match:
        raise LookupError(f'{db.SCHEMA_PATH} declares no table {table}')
    return f'CREATE TABLE {name or table} ({match.group(1)})'


def live_table_sql(connection: sqlite3.Connection, table: str = TABLE) -> str:
    row = connection.execute(
        "SELECT sql FROM sqlite_master WHERE type = 'table' AND name = ?", (table,),
    ).fetchone()
    if row is None:
        raise LookupError(f'no table {table} in this database')
    return row['sql']


def verdict_counts(connection: sqlite3.Connection) -> dict:
    return {
        row['verdict']: row['convictions']
        for row in connection.execute(
            f'SELECT verdict, COUNT(*) AS convictions FROM {TABLE} GROUP BY verdict '
            'ORDER BY verdict',
        )
    }


def _columns(table_sql: str, table: str) -> frozenset:
    """The column names a `CREATE TABLE` statement declares, as sqlite reads them.

    Created in a scratch database and read back through PRAGMA rather than parsed here: the verdict
    names inside the CHECK constraint sit on their own lines and any line-wise reading of the
    statement counts them as columns. The foreign key's target is not resolved at CREATE time, so the
    scratch database needs nothing else in it.
    """
    scratch = sqlite3.connect(':memory:')
    try:
        scratch.execute(table_sql)
        return frozenset(row[1] for row in scratch.execute(f'PRAGMA table_info({table})'))
    finally:
        scratch.close()


def missing_columns(connection: sqlite3.Connection) -> frozenset:
    """Columns schema.sql declares that the live table does not have."""
    live = frozenset(row['name'] for row in connection.execute(f'PRAGMA table_info({TABLE})'))
    return _columns(schema_table_sql(name='probe'), table='probe') - live


def extra_columns(connection: sqlite3.Connection) -> frozenset:
    """Columns the live table has that schema.sql no longer declares.

    `_copy_rows` only names the live table's own columns, so one of these left in place would abort
    the INSERT into the rebuilt table mid-transaction rather than being dropped along with the rest.
    """
    live = frozenset(row['name'] for row in connection.execute(f'PRAGMA table_info({TABLE})'))
    return live - _columns(schema_table_sql(name='probe'), table='probe')


def needs_migration(connection: sqlite3.Connection) -> bool:
    """Whether any of the three cases that call for a rebuild is still present.

    The constraint, the rows and the columns fail separately: a database can have no row left under a
    retired name and still reject the name that replaced it, and either of those can already agree
    with schema.sql while the table itself still predates a column added since, or still carries a
    column schema.sql no longer declares, either of which makes a plain UPDATE impossible — the latter
    is a rebuild `migrate()` refuses to do silently.
    """
    if permitted_verdicts(live_table_sql(connection)) != permitted_verdicts(schema_table_sql()):
        return True
    if missing_columns(connection) or extra_columns(connection):
        return True
    return any(verdict in RETIRED_VERDICTS for verdict in verdict_counts(connection))


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
            values[verdict_at] = mapped_verdict(values[verdict_at])
            rewritten.append(values)
        connection.executemany(
            f'INSERT INTO {TEMPORARY_TABLE} ({quoted}) VALUES ({placeholders})', rewritten,
        )
        copied += len(rewritten)


def migrate(connection: sqlite3.Connection) -> int:
    """Rebuild the table under the current schema with the retired names rewritten.

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
        retired = ' (retired)' if verdict in RETIRED_VERDICTS else ''
        print(f'    {verdict:<{width}}  {count:>6}{retired}')
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
            print(f'{path} is already migrated; no verdict names to rewrite and no column to add')
            _print_counts('verdicts stored', verdict_counts(connection))
            return 0
        print(f'Migrating {TABLE} in {path}')
        _print_counts('before', verdict_counts(connection))
        added = sorted(missing_columns(connection))
        copied = migrate(connection)
        print(f'  copied {copied} rows under the current constraint')
        print(f'  columns added: {", ".join(added) or "none"}')
        _print_counts('after', verdict_counts(connection))
    finally:
        connection.close()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
