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

"""The one-off backfill that gives every stored escape verdict the landing time it was decided
on."""

from __future__ import annotations

import sqlite3

from ews_dashboard import config
from ews_dashboard.analysis import escapes
from scripts import migrate_verdict_landed_at
from tests import fixtures

TEST = 'fast/a.html'
LANDED_AT = fixtures.DEFAULT_BUILD_TIME + 86400
WINDOW_ENDS_AT = LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS

# The table as it stood when only the window's end was stored and the landing was back-derived from
# it against whatever ESCAPE_WINDOW_DAYS happened to say on the day the page was rendered.
PREVIOUS_TABLE_SQL = '''CREATE TABLE escape_verdicts (
    build_id          INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    test_name         TEXT    NOT NULL,

    verdict           TEXT    NOT NULL CHECK (verdict IN (
                          'ESCAPED', 'FAILS_ON_MAIN', 'CONTAINED',
                          'NO_RUNS', 'NO_BASELINE', 'TREE_DIVERGED'
                      )),

    runs_before       INTEGER NOT NULL DEFAULT 0,
    failed_before     INTEGER NOT NULL DEFAULT 0,
    runs_after        INTEGER NOT NULL DEFAULT 0,
    failed_after      INTEGER NOT NULL DEFAULT 0,

    recent_runs       INTEGER,
    recent_failed     INTEGER,
    recent_checked_at INTEGER,

    window_ends_at    INTEGER NOT NULL,
    decided_at        INTEGER NOT NULL,

    PRIMARY KEY (build_id, test_name)
)'''

PREVIOUS_INDEX_SQL = 'CREATE INDEX index_escapes_verdict ON escape_verdicts(verdict)'


class TestMigrate(fixtures.DatabaseTest):
    """The backfill over a database whose table has no landing time at all."""

    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(PREVIOUS_TABLE_SQL)
            self.connection.execute(PREVIOUS_INDEX_SQL)

    def _store_verdict(self, number: int, verdict: str = escapes.ESCAPED,
                       landed_at: int = LANDED_AT, landing_status: str = 'landed',
                       test_name: str = TEST) -> int:
        build_id = self.store_build(number, flaky={test_name: config.CLEAN_TREE}, pr_id=number,
                                    pr_title='A change that landed', sha='a' * 40)
        self.store_landing(number, status=landing_status, landed_at=landed_at)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?)''',
                (build_id, test_name, verdict, 152, 0, 96, 1,
                 landed_at + escapes.ESCAPE_WINDOW_SECONDS, landed_at),
            )
        return build_id

    def _row(self, build_id: int) -> sqlite3.Row:
        return self.connection.execute(
            'SELECT * FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()

    def test_the_column_is_added_and_filled_from_the_landing(self) -> None:
        build_id = self._store_verdict(1)
        self.assertTrue(migrate_verdict_landed_at.column_missing(self.connection))

        self.assertEqual(migrate_verdict_landed_at.migrate(self.connection), (1, 0))

        self.assertEqual(self._row(build_id)['landed_at'], LANDED_AT)

    def test_the_backfilled_time_is_the_landing_and_not_the_derived_one(self) -> None:
        """The bug this exists for: the derived value was the window's end less whatever the setting
        says now, and the two agree only while the setting has not changed."""
        build_id = self._store_verdict(1)

        migrate_verdict_landed_at.migrate(self.connection)

        row = self._row(build_id)
        self.assertEqual(row['landed_at'], LANDED_AT)
        self.assertEqual(row['window_ends_at'], WINDOW_ENDS_AT)

    def test_every_other_column_is_left_as_it_was(self) -> None:
        build_id = self._store_verdict(1)

        migrate_verdict_landed_at.migrate(self.connection)

        row = self._row(build_id)
        self.assertEqual(
            (row['verdict'], row['runs_before'], row['failed_before'], row['runs_after'],
             row['failed_after']),
            (escapes.ESCAPED, 152, 0, 96, 1),
        )
        self.assertEqual((row['recent_runs'], row['recent_failed'], row['recent_checked_at']),
                         (None, None, None))

    def test_a_second_run_finds_nothing_to_do(self) -> None:
        self._store_verdict(1)
        self.assertTrue(migrate_verdict_landed_at.needs_migration(self.connection))

        migrate_verdict_landed_at.migrate(self.connection)

        self.assertFalse(migrate_verdict_landed_at.needs_migration(self.connection))
        self.assertEqual(migrate_verdict_landed_at.migrate(self.connection), (0, 0))

    def test_a_row_with_no_landing_to_join_is_left_null_and_counted(self) -> None:
        """A guess would be a date that never happened, which is the failure this change removes, so
        the row keeps its counts and reports no time."""
        filled = self._store_verdict(1)
        unjoinable = self._store_verdict(2, landing_status='not_landed')

        self.assertEqual(migrate_verdict_landed_at.migrate(self.connection), (1, 1))

        self.assertEqual(self._row(filled)['landed_at'], LANDED_AT)
        self.assertIsNone(self._row(unjoinable)['landed_at'])

    def test_a_row_that_can_never_be_backfilled_does_not_ask_to_be_migrated_again(self) -> None:
        """Otherwise every run would rewrite the whole table for a landing that is not coming
        back."""
        self._store_verdict(1, landing_status='not_landed')

        migrate_verdict_landed_at.migrate(self.connection)

        self.assertEqual(migrate_verdict_landed_at.unfilled_rows(self.connection), 1)
        self.assertFalse(migrate_verdict_landed_at.needs_migration(self.connection))

    def test_the_table_before_the_backfill_has_nowhere_to_put_a_landing_time(self) -> None:
        """Why the column has to be added at all: schema.sql alone never reaches a live table, so
        the first store of a verdict would raise out of the assess pass."""
        self._store_verdict(1)
        with self.assertRaises(sqlite3.OperationalError):
            self.connection.execute('UPDATE escape_verdicts SET landed_at = 1')


class TestColumnSetGuard(fixtures.DatabaseTest):
    """The half of the guard the two earlier migration scripts get wrong.

    They gate on a CHECK-constraint diff plus their rows, so on a database whose constraint already
    matches schema.sql they are permanent no-ops however much work is left. The column set is a
    separate failure from the row contents and both have to be asked about.
    """

    def _store_verdict(self, number: int, landed_at: int = LANDED_AT) -> int:
        build_id = self.store_build(number, flaky={TEST: config.CLEAN_TREE}, pr_id=number,
                                    pr_title='A change that landed', sha='a' * 40)
        self.store_landing(number, landed_at=landed_at)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, landed_at, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?,?)''',
                (build_id, TEST, escapes.ESCAPED, 152, 0, 96, 1, landed_at,
                 landed_at + escapes.ESCAPE_WINDOW_SECONDS, landed_at),
            )
        return build_id

    def test_a_table_with_the_column_but_unfilled_rows_still_migrates(self) -> None:
        """The current schema's constraint, the current schema's columns, and nothing filled in: a
        guard that only compares constraints reports this database as done."""
        build_id = self._store_verdict(1)
        with self.connection:
            self.connection.execute('UPDATE escape_verdicts SET landed_at = NULL')

        self.assertFalse(migrate_verdict_landed_at.column_missing(self.connection))
        self.assertTrue(migrate_verdict_landed_at.needs_migration(self.connection))
        self.assertEqual(migrate_verdict_landed_at.migrate(self.connection), (1, 0))
        self.assertEqual(
            self.connection.execute('SELECT landed_at FROM escape_verdicts WHERE build_id = ?',
                                    (build_id,)).fetchone()['landed_at'],
            LANDED_AT,
        )

    def test_a_table_missing_the_column_still_migrates_with_nothing_to_fill(self) -> None:
        """The mirror case: no row to backfill, so a row-only guard says there is nothing to do, and
        the next assess pass raises on a column that is not there."""
        with self.connection:
            self.connection.execute('ALTER TABLE escape_verdicts DROP COLUMN landed_at')

        self.assertTrue(migrate_verdict_landed_at.column_missing(self.connection))
        self.assertEqual(migrate_verdict_landed_at.unfilled_rows(self.connection), 0)
        self.assertTrue(migrate_verdict_landed_at.needs_migration(self.connection))

        migrate_verdict_landed_at.migrate(self.connection)

        self.assertFalse(migrate_verdict_landed_at.column_missing(self.connection))


class TestFreshDatabase(fixtures.DatabaseTest):
    def test_a_database_created_from_the_current_schema_needs_no_migration(self) -> None:
        self.assertFalse(migrate_verdict_landed_at.needs_migration(self.connection))

    def test_the_column_the_schema_declares_is_nullable_so_it_can_be_added_in_place(self) -> None:
        """A NOT NULL column cannot be added to a live table without a default, and a default here
        would be a landing time nothing landed at."""
        declared = [row for row in self.connection.execute('PRAGMA table_info(escape_verdicts)')
                    if row['name'] == migrate_verdict_landed_at.COLUMN]
        self.assertEqual(len(declared), 1)
        self.assertEqual((declared[0]['notnull'], declared[0]['dflt_value']), (0, None))
