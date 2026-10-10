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

"""The one-off migration that folds the retired escape verdict names into FAILS_ON_MAIN."""

from __future__ import annotations

import sqlite3

from ews_dashboard import config
from ews_dashboard.analysis import escapes
from scripts import migrate_verdict_names
from tests import fixtures

TEST = 'fast/a.html'
LANDED_AT = fixtures.DEFAULT_BUILD_TIME + 86400
WINDOW_ENDS_AT = LANDED_AT + escapes.ESCAPE_WINDOW_SECONDS

# The table as it stood before 3761a9a: it permits the two retired names and has no FAILS_ON_MAIN,
# which is why the rows cannot simply be updated in place.
RETIRED_TABLE_SQL = '''CREATE TABLE escape_verdicts (
    build_id        INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    test_name       TEXT    NOT NULL,

    verdict         TEXT    NOT NULL CHECK (verdict IN (
                        'ESCAPED', 'FLAKY_ON_MAIN', 'CONTAINED', 'ALREADY_FAILING',
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

RETIRED_INDEX_SQL = 'CREATE INDEX index_escapes_verdict ON escape_verdicts(verdict)'


class TestMappedVerdict(fixtures.DatabaseTest):
    """Which stored names the migration rewrites, and which it must leave alone."""

    def test_both_retired_names_become_the_verdict_that_replaced_them(self) -> None:
        self.assertEqual(
            [migrate_verdict_names.mapped_verdict(name)
             for name in ('FLAKY_ON_MAIN', 'ALREADY_FAILING')],
            [escapes.FAILS_ON_MAIN, escapes.FAILS_ON_MAIN],
        )

    def test_every_current_verdict_is_left_as_it_is(self) -> None:
        """A migration that rewrote a live name would lose the answer main actually gave."""
        for verdict in escapes.VERDICTS:
            self.assertEqual(migrate_verdict_names.mapped_verdict(verdict), verdict)

    def test_no_retired_name_survives_as_a_current_verdict(self) -> None:
        """Nothing may be both retired and current, or the mapping would rename a live answer."""
        self.assertEqual(
            set(migrate_verdict_names.RETIRED_VERDICTS) & set(escapes.VERDICTS), set(),
        )

    def test_every_retired_name_maps_onto_a_current_verdict(self) -> None:
        for replacement in migrate_verdict_names.RETIRED_VERDICTS.values():
            self.assertIn(replacement, escapes.VERDICTS)


class TestPermittedVerdicts(fixtures.DatabaseTest):
    """The constraint list is read out of the SQL rather than assumed, since the drift between the
    live table's list and schema.sql's is the bug being migrated."""

    def test_the_names_are_read_out_of_the_check_constraint(self) -> None:
        self.assertEqual(
            migrate_verdict_names.permitted_verdicts(RETIRED_TABLE_SQL),
            frozenset({'ESCAPED', 'FLAKY_ON_MAIN', 'CONTAINED', 'ALREADY_FAILING',
                       'NO_RUNS', 'NO_BASELINE', 'TREE_DIVERGED'}),
        )

    def test_the_schema_permits_exactly_the_verdicts_the_code_can_produce(self) -> None:
        """schema.sql and VERDICTS have to agree, or a fresh database rejects a verdict the assess
        pass will hand it."""
        self.assertEqual(
            migrate_verdict_names.permitted_verdicts(migrate_verdict_names.schema_table_sql()),
            frozenset(escapes.VERDICTS),
        )


class TestMigrate(fixtures.DatabaseTest):
    """The rebuild itself, over a database still shaped the way it was before 3761a9a."""

    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(RETIRED_TABLE_SQL)
            self.connection.execute(RETIRED_INDEX_SQL)

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
        return migrate_verdict_names.verdict_counts(self.connection)

    def test_the_retired_names_are_folded_into_one_and_nothing_is_lost(self) -> None:
        self._store_verdict(1, 'FLAKY_ON_MAIN')
        self._store_verdict(2, 'ALREADY_FAILING')
        self._store_verdict(3, escapes.CONTAINED)
        self._store_verdict(4, escapes.TREE_DIVERGED)
        before = self._counts()

        migrate_verdict_names.migrate(self.connection)

        self.assertEqual(sum(before.values()), sum(self._counts().values()))
        self.assertEqual(self._counts(), {escapes.CONTAINED: 1, escapes.FAILS_ON_MAIN: 2,
                                          escapes.TREE_DIVERGED: 1})

    def test_the_runs_either_side_of_the_landing_survive_the_copy(self) -> None:
        """These counts are the evidence main failed the test without the change, and the reason the
        rows are migrated rather than dropped."""
        build_id = self._store_verdict(1, 'ALREADY_FAILING', runs_before=96, failed_before=90,
                                       runs_after=99, failed_after=93)

        migrate_verdict_names.migrate(self.connection)

        row = self.connection.execute(
            'SELECT * FROM escape_verdicts WHERE build_id = ?', (build_id,),
        ).fetchone()
        self.assertEqual(row['verdict'], escapes.FAILS_ON_MAIN)
        self.assertEqual(
            (row['runs_before'], row['failed_before'], row['runs_after'], row['failed_after']),
            (96, 90, 99, 93),
        )
        self.assertEqual((row['window_ends_at'], row['decided_at']), (WINDOW_ENDS_AT, LANDED_AT))
        self.assertEqual(row['test_name'], TEST)

    def test_the_rebuilt_table_accepts_the_name_that_replaced_them(self) -> None:
        self._store_verdict(1, escapes.CONTAINED)
        migrate_verdict_names.migrate(self.connection)

        self._store_verdict(2, escapes.FAILS_ON_MAIN)

        self.assertEqual(self._counts()[escapes.FAILS_ON_MAIN], 1)

    def test_the_rebuilt_table_rejects_a_retired_name(self) -> None:
        """The constraint is what stops the old names coming back on the next refresh."""
        self._store_verdict(1, escapes.CONTAINED)
        migrate_verdict_names.migrate(self.connection)

        with self.assertRaises(sqlite3.IntegrityError):
            self._store_verdict(2, 'FLAKY_ON_MAIN')

    def test_the_index_the_dropped_table_had_comes_back(self) -> None:
        """Dropping a table takes its indexes with it, and every verdict count groups on this one."""
        self._store_verdict(1, 'FLAKY_ON_MAIN')

        migrate_verdict_names.migrate(self.connection)

        self.assertEqual(self.connection.execute(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'index' "
            "AND name = 'index_escapes_verdict' AND tbl_name = 'escape_verdicts'",
        ).fetchone()[0], 1)

    def test_a_second_run_finds_nothing_to_do(self) -> None:
        self._store_verdict(1, 'FLAKY_ON_MAIN')
        self.assertTrue(migrate_verdict_names.needs_migration(self.connection))

        migrate_verdict_names.migrate(self.connection)

        self.assertFalse(migrate_verdict_names.needs_migration(self.connection))

    def test_a_table_with_no_retired_rows_still_needs_its_constraint_replaced(self) -> None:
        """The rows and the constraint fail separately: this table has nothing to rewrite and still
        rejects the verdict the assess pass now produces."""
        self._store_verdict(1, escapes.CONTAINED)
        self.assertEqual(self._counts(), {escapes.CONTAINED: 1})

        self.assertTrue(migrate_verdict_names.needs_migration(self.connection))

    def test_foreign_keys_are_on_again_afterwards(self) -> None:
        """The rebuild turns them off, and a connection left that way would let later work store a
        verdict against a build that is not there."""
        self._store_verdict(1, 'FLAKY_ON_MAIN')

        migrate_verdict_names.migrate(self.connection)

        self.assertEqual(self.connection.execute('PRAGMA foreign_keys').fetchone()[0], 1)

    def test_a_migrated_verdict_is_counted_in_the_tally_it_had_vanished_from(self) -> None:
        """The point of the migration: these convictions are back in the denominator the escape rate
        is taken over, and the tally says so before it as well as after."""
        self._store_verdict(1, 'FLAKY_ON_MAIN')
        self._store_verdict(2, 'ALREADY_FAILING')

        before = self._tally()
        self.assertEqual(before.unrecognised, {'FLAKY_ON_MAIN': 1, 'ALREADY_FAILING': 1})
        self.assertEqual((before.asked, before.decided), (2, 0))

        migrate_verdict_names.migrate(self.connection)

        after = self._tally()
        self.assertEqual((after.asked, after.decided, after.unrecognised_total), (2, 2, 0))
        self.assertEqual(after.by_verdict[escapes.FAILS_ON_MAIN], 2)

    def _tally(self) -> escapes.Tally:
        return escapes.tally(self.connection, fixtures.DEFAULT_BUILD_TIME - 86400,
                             fixtures.DEFAULT_BUILD_TIME + 86400)


class TestFreshDatabase(fixtures.DatabaseTest):
    def test_a_database_created_from_the_current_schema_needs_no_migration(self) -> None:
        self.assertFalse(migrate_verdict_names.needs_migration(self.connection))


# The table as schema.sql declared it before `landed_at` and the currency columns were added: the
# constraint already matches today's, and no row is stored under a retired name.
CURRENT_CONSTRAINT_TABLE_SQL = '''CREATE TABLE escape_verdicts (
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


class TestMissingColumns(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(CURRENT_CONSTRAINT_TABLE_SQL)

    def test_a_table_missing_the_newer_columns_still_needs_a_rebuild(self) -> None:
        """The constraint already matches and nothing is stored under a retired name, but the table
        predates `landed_at` and the currency columns, so a rebuild is still outstanding."""
        self.assertEqual(
            migrate_verdict_names.permitted_verdicts(
                migrate_verdict_names.live_table_sql(self.connection)),
            migrate_verdict_names.permitted_verdicts(migrate_verdict_names.schema_table_sql()),
        )

        self.assertTrue(migrate_verdict_names.needs_migration(self.connection))

        migrate_verdict_names.migrate(self.connection)

        self.assertFalse(migrate_verdict_names.needs_migration(self.connection))


# The current schema's columns, plus one schema.sql no longer declares: a live table can carry a
# column a schema edit dropped, and a migration must never discard that column's data silently.
EXTRA_COLUMN_TABLE_SQL = '''CREATE TABLE escape_verdicts (
    build_id        INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    test_name       TEXT    NOT NULL,

    verdict         TEXT    NOT NULL CHECK (verdict IN (
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

    landed_at       INTEGER,
    window_ends_at  INTEGER NOT NULL,
    decided_at      INTEGER NOT NULL,

    retired_flag    INTEGER,

    PRIMARY KEY (build_id, test_name)
)'''


class TestExtraColumns(fixtures.DatabaseTest):
    def setUp(self) -> None:
        super().setUp()
        with self.connection:
            self.connection.execute('DROP TABLE escape_verdicts')
            self.connection.execute(EXTRA_COLUMN_TABLE_SQL)

    def test_a_table_with_a_column_schema_no_longer_declares_still_needs_a_rebuild(self) -> None:
        self.assertEqual(migrate_verdict_names.extra_columns(self.connection),
                         frozenset({'retired_flag'}))
        self.assertTrue(migrate_verdict_names.needs_migration(self.connection))

    def test_migrate_refuses_to_silently_drop_the_extra_column(self) -> None:
        with self.assertRaises(RuntimeError) as context:
            migrate_verdict_names.migrate(self.connection)
        self.assertIn('retired_flag', str(context.exception))
