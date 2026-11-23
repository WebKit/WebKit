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

"""The one-off rebuild that folds ESCAPED_RARELY into ESCAPED and adds the currency columns."""

from __future__ import annotations

import sqlite3

from ews_dashboard import config
from ews_dashboard.analysis import escapes
from scripts import migrate_escape_subcategories
from scripts.migrate_verdict_names import (live_table_sql, missing_columns, permitted_verdicts,
                                           schema_table_sql, verdict_counts)
from tests import fixtures

TEST = 'fast/a.html'
LANDED_AT = fixtures.DEFAULT_BUILD_TIME + 86400
WINDOW_ENDS_AT = LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS

RETIRED = migrate_escape_subcategories.RETIRED_VERDICT

# A constructed hypothetical: no committed schema.sql ever declared ESCAPED_RARELY, but the migration
# has to handle whatever a pre-existing live table turns out to carry.
PREVIOUS_TABLE_SQL = '''CREATE TABLE escape_verdicts (
    build_id        INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    test_name       TEXT    NOT NULL,

    verdict         TEXT    NOT NULL CHECK (verdict IN (
                        'ESCAPED', 'ESCAPED_RARELY', 'FAILS_ON_MAIN', 'CONTAINED',
                        'NO_RUNS', 'NO_BASELINE', 'TREE_DIVERGED'
                    )),

    runs_before     INTEGER NOT NULL DEFAULT 0,
    failed_before   INTEGER NOT NULL DEFAULT 0,
    runs_after      INTEGER NOT NULL DEFAULT 0,
    failed_after    INTEGER NOT NULL DEFAULT 0,

    window_ends_at  INTEGER NOT NULL,
    decided_at      INTEGER NOT NULL,

    PRIMARY KEY (build_id, test_name)
)'''

PREVIOUS_INDEX_SQL = 'CREATE INDEX index_escapes_verdict ON escape_verdicts(verdict)'


class TestMigrate(fixtures.DatabaseTest):
    """The rebuild itself, over a database still shaped the way it was before this change."""

    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(PREVIOUS_TABLE_SQL)
            self.connection.execute(PREVIOUS_INDEX_SQL)

    def _store_verdict(self, number: int, verdict: str, test_name: str = TEST,
                       runs_before: int = 152, failed_before: int = 0, runs_after: int = 96,
                       failed_after: int = 1) -> int:
        build_id = self.store_build(number, flaky={test_name: config.CLEAN_TREE}, pr_id=number,
                                    pr_title='A change that landed', sha='a' * 40)
        with self.connection:
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?)''',
                (build_id, test_name, verdict, runs_before, failed_before, runs_after,
                 failed_after, WINDOW_ENDS_AT, LANDED_AT),
            )
        return build_id

    def _counts(self) -> dict:
        return verdict_counts(self.connection)

    def _row(self, build_id: int) -> sqlite3.Row:
        return self.connection.execute(
            'SELECT * FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()

    def test_a_low_rate_escape_becomes_an_escape_with_its_counts_intact(self) -> None:
        """The counts are what the rate is now read from, so a row that loses them loses the
        subcategory the stored name used to carry."""
        build_id = self._store_verdict(1, RETIRED, runs_before=152, failed_before=0, runs_after=96,
                                       failed_after=1)

        migrate_escape_subcategories.migrate(self.connection)

        row = self._row(build_id)
        self.assertEqual(row['verdict'], escapes.ESCAPED)
        self.assertEqual(
            (row['runs_before'], row['failed_before'], row['runs_after'], row['failed_after']),
            (152, 0, 96, 1),
        )
        self.assertEqual((row['window_ends_at'], row['decided_at']), (WINDOW_ENDS_AT, LANDED_AT))
        self.assertEqual(escapes.rarity_for_counts(row['runs_after'], row['failed_after']),
                         escapes.RARE)

    def test_every_other_row_survives_untouched(self) -> None:
        self._store_verdict(1, RETIRED)
        self._store_verdict(2, escapes.ESCAPED, runs_after=96, failed_after=90)
        self._store_verdict(3, escapes.FAILS_ON_MAIN, runs_before=88, failed_before=6,
                            runs_after=99, failed_after=14)
        self._store_verdict(4, escapes.CONTAINED, runs_after=6, failed_after=0)
        self._store_verdict(5, escapes.TREE_DIVERGED, runs_before=0, runs_after=0, failed_after=0)
        before = self._counts()

        migrate_escape_subcategories.migrate(self.connection)

        self.assertEqual(sum(before.values()), sum(self._counts().values()))
        self.assertEqual(self._counts(), {escapes.CONTAINED: 1, escapes.ESCAPED: 2,
                                          escapes.FAILS_ON_MAIN: 1, escapes.TREE_DIVERGED: 1})

    def test_the_rebuilt_table_carries_the_currency_columns_unanswered(self) -> None:
        """Null is the answer for a row copied from before the check existed: nobody has asked main
        about it, which is not the same as main having stopped failing it."""
        build_id = self._store_verdict(1, RETIRED)

        migrate_escape_subcategories.migrate(self.connection)

        row = self._row(build_id)
        self.assertEqual((row['recent_runs'], row['recent_failed'], row['recent_checked_at']),
                         (None, None, None))
        self.assertEqual(escapes.currency_for_counts(row['recent_runs'], row['recent_failed'],
                                                     row['recent_checked_at']), escapes.UNCHECKED)

    def test_the_table_before_the_rebuild_has_nowhere_to_put_a_currency_answer(self) -> None:
        """Why a rebuild is needed at all: schema.sql alone never reaches a live table, so the first
        currency check would raise out of the assess pass."""
        self._store_verdict(1, escapes.ESCAPED, runs_after=96, failed_after=90)
        with self.assertRaises(sqlite3.OperationalError):
            self.connection.execute('UPDATE escape_verdicts SET recent_checked_at = 1')

    def test_the_rebuilt_table_no_longer_permits_the_retired_verdict(self) -> None:
        """The fold is not just cosmetic: a row stored under the retired name before the rebuild
        cannot come back once it is done, because the constraint the rebuilt table carries is the
        current one, read off the live table rather than assumed."""
        self._store_verdict(1, RETIRED)

        migrate_escape_subcategories.migrate(self.connection)

        self.assertNotIn(RETIRED, permitted_verdicts(live_table_sql(self.connection)))

    def test_the_index_the_dropped_table_had_comes_back(self) -> None:
        """Dropping a table takes its indexes with it, and every verdict count groups on this one."""
        self._store_verdict(1, RETIRED)

        migrate_escape_subcategories.migrate(self.connection)

        self.assertEqual(self.connection.execute(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' "
            "AND name = 'index_escapes_verdict' AND tbl_name = 'escape_verdicts'",
        ).fetchone()[0], 1)

    def test_a_second_run_finds_nothing_to_do(self) -> None:
        self._store_verdict(1, RETIRED)
        self.assertTrue(migrate_escape_subcategories.needs_migration(self.connection))

        migrate_escape_subcategories.migrate(self.connection)
        counts = self._counts()

        self.assertFalse(migrate_escape_subcategories.needs_migration(self.connection))
        self.assertEqual(migrate_escape_subcategories.retired_rows(self.connection), 0)
        self.assertEqual(missing_columns(self.connection), frozenset())
        self.assertEqual(self._counts(), counts)

    def test_migrate_refuses_to_silently_drop_a_column_schema_no_longer_declares(self) -> None:
        with self.connection:
            self.connection.execute('ALTER TABLE escape_verdicts ADD COLUMN retired_flag INTEGER')
        self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)
        self.assertEqual(migrate_escape_subcategories.extra_columns(self.connection),
                         frozenset({'retired_flag'}))

        self.assertTrue(migrate_escape_subcategories.needs_migration(self.connection))
        with self.assertRaises(RuntimeError) as context:
            migrate_escape_subcategories.migrate(self.connection)
        self.assertIn('retired_flag', str(context.exception))

    def test_foreign_keys_are_on_again_afterwards(self) -> None:
        """The rebuild turns them off, and a connection left that way would let later work store a
        verdict against a build that is not there."""
        self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)

        migrate_escape_subcategories.migrate(self.connection)

        self.assertEqual(self.connection.execute('PRAGMA foreign_keys').fetchone()[0], 1)

    def test_a_folded_row_is_counted_in_the_bucket_it_now_belongs_to(self) -> None:
        self._store_verdict(1, RETIRED)

        migrate_escape_subcategories.migrate(self.connection)

        tally = escapes.tally(self.connection, fixtures.DEFAULT_BUILD_TIME - 86400,
                              fixtures.DEFAULT_BUILD_TIME + 86400)
        self.assertEqual(tally.by_verdict[escapes.ESCAPED], 1)
        self.assertEqual((tally.decided, tally.unrecognised_total), (1, 0))

    def test_a_retired_row_is_unreadable_until_it_is_folded(self) -> None:
        """Left alone it counts in no bucket and in no rate, which is what a page reports as
        unrecognised."""
        self._store_verdict(1, RETIRED)

        tally = escapes.tally(self.connection, fixtures.DEFAULT_BUILD_TIME - 86400,
                              fixtures.DEFAULT_BUILD_TIME + 86400)

        self.assertEqual(tally.unrecognised, {RETIRED: 1})
        self.assertEqual(tally.by_verdict[escapes.ESCAPED], 0)


def _sql_without_columns(sql: str, columns: tuple) -> str:
    lines = sql.splitlines()
    kept = [line for line in lines
            if not any(line.strip().startswith(column) for column in columns)]
    return '\n'.join(kept)


class TestMissingColumns(fixtures.DatabaseTest):
    """The current constraint already matches schema.sql, so only the missing-column branch of
    `needs_migration` can be what fires here."""

    def setUp(self) -> None:
        super().setUp()
        current_sql = schema_table_sql()
        sql_without_new_columns = _sql_without_columns(
            current_sql, ('landed_at', 'recent_runs', 'recent_failed', 'recent_checked_at'))
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(sql_without_new_columns)

    def test_a_table_missing_the_newer_columns_still_needs_a_rebuild(self) -> None:
        """The rows, the constraint and the columns fail separately, and this table has nothing to
        rename and nowhere to record a currency answer."""
        self.assertEqual(
            permitted_verdicts(live_table_sql(self.connection)),
            permitted_verdicts(schema_table_sql()),
        )
        self.assertEqual(missing_columns(self.connection),
                         frozenset({'recent_runs', 'recent_failed', 'recent_checked_at',
                                    'landed_at'}))

        self.assertTrue(migrate_escape_subcategories.needs_migration(self.connection))

        migrate_escape_subcategories.migrate(self.connection)

        self.assertFalse(migrate_escape_subcategories.needs_migration(self.connection))


class TestFreshDatabase(fixtures.DatabaseTest):
    def test_a_database_created_from_the_current_schema_needs_no_migration(self) -> None:
        self.assertFalse(migrate_escape_subcategories.needs_migration(self.connection))

    def test_the_schema_permits_exactly_the_verdicts_the_code_can_produce(self) -> None:
        """schema.sql and VERDICTS have to agree, or a fresh database rejects a verdict the assess
        pass will hand it."""
        self.assertEqual(
            permitted_verdicts(schema_table_sql()),
            frozenset(escapes.VERDICTS),
        )

    def test_a_fresh_database_rejects_the_retired_verdict(self) -> None:
        """schema.sql has never declared this name, so a database built from it catches an attempt to
        store it without any rebuild having to run."""
        build_id = self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=1,
                                    pr_title='A change that landed', sha='a' * 40)
        with self.assertRaises(sqlite3.IntegrityError):
            self.connection.execute(
                '''INSERT INTO escape_verdicts (
                    build_id, test_name, verdict, window_ends_at, decided_at
                ) VALUES (?, ?, ?, ?, ?)''',
                (build_id, TEST, RETIRED, WINDOW_ENDS_AT, LANDED_AT),
            )
