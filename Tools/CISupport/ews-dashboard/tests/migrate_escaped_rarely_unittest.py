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

"""The one-off migration that decides every stored escape verdict again from the counts it kept."""

from __future__ import annotations

import sqlite3

from ews_dashboard import config
from ews_dashboard.analysis import escapes
from scripts import migrate_escaped_rarely
from tests import fixtures

TEST = 'fast/a.html'
LANDED_AT = fixtures.DEFAULT_BUILD_TIME + 86400
WINDOW_ENDS_AT = LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS

# The table as it stood before the baseline decided the verdict: the names the old rule could reach,
# and none of the currency columns a later change added, which is why the rows are copied rather than
# updated in place.
PREVIOUS_TABLE_SQL = '''CREATE TABLE escape_verdicts (
    build_id        INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    test_name       TEXT    NOT NULL,

    verdict         TEXT    NOT NULL CHECK (verdict IN (
                        'ESCAPED', 'FAILS_ON_MAIN', 'CONTAINED',
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


class TestRedecidedRow(fixtures.DatabaseTest):
    """What one stored row is renamed to, taken from `escapes` rather than restated here."""

    def setUp(self) -> None:
        super().setUp()
        self.build_id = self.store_build(1, flaky={TEST: config.CLEAN_TREE}, pr_id=1,
                                         pr_title='A change that landed', sha='a' * 40)

    def _row(self, verdict: str, runs_before: int, failed_before: int, runs_after: int,
             failed_after: int) -> sqlite3.Row:
        with self.connection:
            self.connection.execute(
                '''INSERT OR REPLACE INTO escape_verdicts (
                    build_id, test_name, verdict, runs_before, failed_before, runs_after,
                    failed_after, window_ends_at, decided_at
                ) VALUES (?,?,?,?,?,?,?,?,?)''',
                (self.build_id, TEST, verdict, runs_before, failed_before, runs_after, failed_after,
                 WINDOW_ENDS_AT, LANDED_AT),
            )
        return self.connection.execute('SELECT * FROM escape_verdicts').fetchone()

    def test_a_clean_baseline_with_one_failure_after_becomes_an_escape(self) -> None:
        self.assertEqual(
            migrate_escaped_rarely.redecided_row(
                self._row(escapes.FAILS_ON_MAIN, 152, 0, 96, 1)),
            escapes.ESCAPED,
        )

    def test_a_baseline_main_was_failing_stays_main_s(self) -> None:
        self.assertEqual(
            migrate_escaped_rarely.redecided_row(self._row(escapes.FAILS_ON_MAIN, 88, 6, 99, 14)),
            escapes.FAILS_ON_MAIN,
        )

    def test_a_diverged_row_is_left_alone(self) -> None:
        self.assertEqual(
            migrate_escaped_rarely.redecided_row(self._row(escapes.TREE_DIVERGED, 0, 0, 0, 0)),
            escapes.TREE_DIVERGED,
        )


class TestMigrate(fixtures.DatabaseTest):
    """The rebuild itself, over a database shaped the way it was before the baseline decided."""

    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(PREVIOUS_TABLE_SQL)
            self.connection.execute(PREVIOUS_INDEX_SQL)

    def _store_verdict(self, number: int, verdict: str, test_name: str = TEST,
                       runs_before: int = 88, failed_before: int = 6, runs_after: int = 99,
                       failed_after: int = 14) -> int:
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
        return migrate_escaped_rarely.verdict_counts(self.connection)

    def test_a_stale_verdict_over_a_clean_baseline_is_rewritten(self) -> None:
        """The whole point: 0 of 152 before and 1 of 96 after was stored as main's own failure and is
        an escape on thin evidence."""
        build_id = self._store_verdict(1, escapes.FAILS_ON_MAIN, runs_before=152, failed_before=0,
                                       runs_after=96, failed_after=1)

        migrate_escaped_rarely.migrate(self.connection)

        row = self.connection.execute(
            'SELECT * FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()
        self.assertEqual(row['verdict'], escapes.ESCAPED)
        self.assertEqual(escapes.rarity_for_counts(row['runs_after'], row['failed_after']),
                         escapes.RARE)
        self.assertEqual(
            (row['runs_before'], row['failed_before'], row['runs_after'], row['failed_after']),
            (152, 0, 96, 1),
        )
        self.assertEqual((row['window_ends_at'], row['decided_at']), (WINDOW_ENDS_AT, LANDED_AT))

    def test_nothing_is_lost_and_only_the_stale_rows_move(self) -> None:
        self._store_verdict(1, escapes.FAILS_ON_MAIN, runs_before=152, failed_before=0,
                            runs_after=96, failed_after=1)
        self._store_verdict(2, escapes.FAILS_ON_MAIN)
        self._store_verdict(3, escapes.CONTAINED, runs_after=6, failed_after=0)
        self._store_verdict(4, escapes.TREE_DIVERGED, runs_before=0, failed_before=0, runs_after=0,
                            failed_after=0)
        before = self._counts()

        migrate_escaped_rarely.migrate(self.connection)

        self.assertEqual(sum(before.values()), sum(self._counts().values()))
        self.assertEqual(self._counts(), {escapes.CONTAINED: 1, escapes.FAILS_ON_MAIN: 1,
                                          escapes.TREE_DIVERGED: 1, escapes.ESCAPED: 1})

    def test_a_diverged_row_survives_the_recompute_that_would_call_it_no_runs(self) -> None:
        build_id = self._store_verdict(1, escapes.TREE_DIVERGED, runs_before=0, failed_before=0,
                                       runs_after=0, failed_after=0)

        migrate_escaped_rarely.migrate(self.connection)

        self.assertEqual(self.connection.execute(
            'SELECT verdict FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()['verdict'], escapes.TREE_DIVERGED)

    def test_the_rebuilt_table_carries_the_columns_the_current_schema_has(self) -> None:
        """The rebuild copies into a table created from schema.sql, so it picks up every column added
        since, unanswered, rather than only renaming verdicts."""
        build_id = self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)

        migrate_escaped_rarely.migrate(self.connection)

        row = self.connection.execute(
            'SELECT * FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()
        self.assertEqual((row['recent_runs'], row['recent_failed'], row['recent_checked_at']),
                         (None, None, None))

    def test_the_index_the_dropped_table_had_comes_back(self) -> None:
        """Dropping a table takes its indexes with it, and every verdict count groups on this one."""
        self._store_verdict(1, escapes.FAILS_ON_MAIN, runs_before=152, failed_before=0,
                            runs_after=96, failed_after=1)

        migrate_escaped_rarely.migrate(self.connection)

        self.assertEqual(self.connection.execute(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' "
            "AND name = 'index_escapes_verdict' AND tbl_name = 'escape_verdicts'",
        ).fetchone()[0], 1)

    def test_a_second_run_finds_nothing_to_do(self) -> None:
        self._store_verdict(1, escapes.FAILS_ON_MAIN, runs_before=152, failed_before=0,
                            runs_after=96, failed_after=1)
        self.assertTrue(migrate_escaped_rarely.needs_migration(self.connection))

        migrate_escaped_rarely.migrate(self.connection)

        self.assertFalse(migrate_escaped_rarely.needs_migration(self.connection))
        counts = self._counts()

        self.assertEqual(migrate_escaped_rarely.stale_verdicts(self.connection), 0)
        self.assertEqual(self._counts(), counts)

    def test_a_table_missing_the_newer_columns_still_needs_a_rebuild(self) -> None:
        """The rows and the columns fail separately: a verdict that already agrees with the current
        rule still leaves the table without `landed_at` and the currency columns until the rebuild
        this script also runs brings them in."""
        self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)
        self.assertEqual(migrate_escaped_rarely.stale_verdicts(self.connection), 0)

        self.assertTrue(migrate_escaped_rarely.needs_migration(self.connection))

        migrate_escaped_rarely.migrate(self.connection)

        self.assertFalse(migrate_escaped_rarely.needs_migration(self.connection))

    def test_migrate_refuses_to_silently_drop_a_column_schema_no_longer_declares(self) -> None:
        with self.connection:
            self.connection.execute('ALTER TABLE escape_verdicts ADD COLUMN retired_flag INTEGER')
        self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)
        self.assertEqual(migrate_escaped_rarely.extra_columns(self.connection),
                         frozenset({'retired_flag'}))

        self.assertTrue(migrate_escaped_rarely.needs_migration(self.connection))
        with self.assertRaises(RuntimeError) as context:
            migrate_escaped_rarely.migrate(self.connection)
        self.assertIn('retired_flag', str(context.exception))

    def test_foreign_keys_are_on_again_afterwards(self) -> None:
        """The rebuild turns them off, and a connection left that way would let later work store a
        verdict against a build that is not there."""
        self._store_verdict(1, escapes.CONTAINED, runs_after=6, failed_after=0)

        migrate_escaped_rarely.migrate(self.connection)

        self.assertEqual(self.connection.execute('PRAGMA foreign_keys').fetchone()[0], 1)

    def test_a_rewritten_verdict_is_counted_in_the_bucket_it_now_belongs_to(self) -> None:
        self._store_verdict(1, escapes.FAILS_ON_MAIN, runs_before=152, failed_before=0,
                            runs_after=96, failed_after=1)

        migrate_escaped_rarely.migrate(self.connection)

        tally = escapes.tally(self.connection, fixtures.DEFAULT_BUILD_TIME - 86400,
                              fixtures.DEFAULT_BUILD_TIME + 86400)
        self.assertEqual(tally.by_verdict[escapes.ESCAPED], 1)
        self.assertEqual((tally.decided, tally.unrecognised_total), (1, 0))


class TestFreshDatabase(fixtures.DatabaseTest):
    def test_a_database_created_from_the_current_schema_needs_no_migration(self) -> None:
        self.assertFalse(migrate_escaped_rarely.needs_migration(self.connection))

    def test_the_schema_permits_exactly_the_verdicts_the_code_can_produce(self) -> None:
        """schema.sql and VERDICTS have to agree, or a fresh database rejects a verdict the assess
        pass will hand it."""
        self.assertEqual(
            migrate_escaped_rarely.permitted_verdicts(migrate_escaped_rarely.schema_table_sql()),
            frozenset(escapes.VERDICTS),
        )
