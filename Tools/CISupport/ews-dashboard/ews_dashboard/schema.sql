-- SQLite schema for the EWS flakiness dashboard.
--
-- Two rules shape it. Buildbot's own arrays are stored as JSON-encoded TEXT verbatim, so a
-- reader can always see what the bot reported. Anything the web layer filters, groups or
-- counts by is exploded into its own row at ingest time, so no page parses JSON to answer a
-- question.

PRAGMA foreign_keys = ON;
-- WAL, not the default rollback journal, because a scheduled refresh writes while the web app is
-- serving: a rollback-journal commit needs an exclusive lock, so every page request would wait out
-- whichever transaction the refresh is in. The mode persists in the file once set here.
PRAGMA journal_mode = WAL;


CREATE TABLE IF NOT EXISTS build_verdicts (
    build_id                    INTEGER PRIMARY KEY,
    builder                     TEXT    NOT NULL,
    builder_id                  INTEGER NOT NULL,
    build_number                INTEGER NOT NULL,

    pr_id                       INTEGER,
    -- The pull request's title, which is the only thing a build and the commit it landed as have in
    -- common: a landed message names its bug and its radar, never its pull request.
    pr_title                    TEXT,
    sha                         TEXT,
    change_id                   TEXT,
    -- WebKit commit identifier of the main commit the pull request was rebased onto, e.g.
    -- '314546@main'. This is the "before the patch" point every history lookup is asked about.
    identifier                  TEXT,

    platform                    TEXT,
    style                       TEXT,
    flavor                      TEXT,
    suite                       TEXT    NOT NULL,

    verdict                     TEXT    NOT NULL CHECK (verdict IN (
                                    'SUCCESS', 'WARNINGS', 'FAILURE', 'SKIPPED',
                                    'EXCEPTION', 'RETRY', 'CANCELLED', 'UNKNOWN'
                                )),

    first_run_failures          TEXT,
    second_run_failures         TEXT,
    clean_tree_run_failures     TEXT,

    -- The bot's own crash-storm signal: first_results_exceed_failure_limit or its second-run
    -- counterpart, set by steps.py when a run bails out at EXIT_AFTER_FAILURES.
    exceeded_failure_limit      INTEGER NOT NULL DEFAULT 0,

    -- Whether this build consulted results.webkit.org for flakiness at all. A build that asked
    -- and convicted nothing leaves no flakiness_verdicts rows, so without this flag it is
    -- indistinguishable from a build that never asked.
    flakiness_query_ran         INTEGER NOT NULL DEFAULT 0,

    started_at                  INTEGER,
    complete_at                 INTEGER
);

CREATE INDEX IF NOT EXISTS index_verdicts_pull_request ON build_verdicts(pr_id);
CREATE INDEX IF NOT EXISTS index_verdicts_verdict_time ON build_verdicts(verdict, started_at);
CREATE INDEX IF NOT EXISTS index_verdicts_suite_time   ON build_verdicts(suite, started_at);
CREATE INDEX IF NOT EXISTS index_verdicts_builder_time ON build_verdicts(builder, started_at);
CREATE INDEX IF NOT EXISTS index_verdicts_queried      ON build_verdicts(flakiness_query_ran, started_at);


-- One row per test the EWS flakiness classifier was asked about and answered for, exploded from
-- the results-db_{first,second}_run_flaky* build properties.
--
-- `rule` is the READ-side verdict RunWebKitTests reached — CleanTree, DirtyTree or BetweenBuilds
-- — and is not the write-side flaky_type stored in Cassandra. The two vocabularies differ.
CREATE TABLE IF NOT EXISTS flakiness_verdicts (
    build_id                 INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    -- 1 for the first test run, 2 for the rerun with the change still applied. The rerun
    -- re-queries the same tests, so its answer is the one the author saw; latest_flakiness_verdicts
    -- selects it.
    run_number               INTEGER NOT NULL CHECK (run_number IN (1, 2)),
    test_name                TEXT    NOT NULL,

    rule                     TEXT    CHECK (rule IN ('CleanTree', 'DirtyTree', 'BetweenBuilds')),
    query_failed             INTEGER NOT NULL DEFAULT 0,
    -- 0 only for a BetweenBuilds conviction the bot itself flagged as having no within-build
    -- evidence; NULL for every other rule, which has nothing to say about it.
    within_build_evidence    INTEGER,

    PRIMARY KEY (build_id, run_number, test_name)
);

CREATE INDEX IF NOT EXISTS index_flakiness_rule ON flakiness_verdicts(rule);
CREATE INDEX IF NOT EXISTS index_flakiness_test ON flakiness_verdicts(test_name);


-- The answer that stood for each test: the highest run number that said anything about it. Scoped
-- per test rather than per build, because the rerun only asks about the tests still failing, so a
-- build-wide MAX would drop a first-run conviction the rerun never revisited.
CREATE VIEW IF NOT EXISTS latest_flakiness_verdicts AS
SELECT verdict.*
FROM flakiness_verdicts AS verdict
WHERE verdict.run_number = (
    SELECT MAX(later.run_number)
    FROM flakiness_verdicts AS later
    WHERE later.build_id = verdict.build_id AND later.test_name = verdict.test_name
);


CREATE TABLE IF NOT EXISTS builds_ingested (
    builder_id    INTEGER NOT NULL,
    build_number  INTEGER NOT NULL,
    fetched_at    INTEGER NOT NULL,

    PRIMARY KEY (builder_id, build_number)
);


-- How far back a walk of each builder has actually reached. Without this a run of builds already
-- held reads as "caught up" whatever window the walk was given, so widening the window could never
-- backfill: a busy queue holding its last fortnight stopped a 90-day walk inside the first day.
CREATE TABLE IF NOT EXISTS builder_coverage (
    builder       TEXT    PRIMARY KEY,
    -- The oldest timestamp a walk that ran to completion was asked for, 0 for an unbounded one.
    walked_since  INTEGER NOT NULL,
    walked_at     INTEGER NOT NULL
);


-- Which commit on main each pull request landed as, worked out by webkit_checkout from the pull
-- request's title. Cached because the search reads commit messages, and because most of these rows
-- are asked for again on the next refresh.
CREATE TABLE IF NOT EXISTS landings (
    pr_id         INTEGER PRIMARY KEY,
    status        TEXT    NOT NULL CHECK (status IN ('landed', 'not_landed', 'ambiguous')),
    -- How many commits on main the title matched: 1 for landed, more for ambiguous, 0 for neither.
    matches       INTEGER NOT NULL DEFAULT 0,
    commit_hash   TEXT,
    -- WebKit identifier of the landed commit, from its Canonical link.
    identifier    TEXT,
    landed_at     INTEGER,
    subject       TEXT,
    -- The main this answer was looked for in. A pull request that had not landed when it was first
    -- asked about is the ordinary case, not a dead end, so 'not_landed' is re-asked once main moves.
    branch_head   TEXT    NOT NULL,
    resolved_at   INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS index_landings_status ON landings(status);


-- Cached responses from results.webkit.org's results-summary endpoint, which returns nine
-- outcome percentages summing to 100.
--
-- Invalidation, by row kind:
--   commit_ref = ''  and has_history = 1  tip-of-tree lookup; expires (CURRENT_TTL_SECONDS)
--   commit_ref = ref and has_history = 1  a window ending at a fixed commit; never expires
--   has_history = 0                       upstream has no history for this configuration, which
--                                         is an answer worth keeping, but history can appear
--                                         later, so it expires (NO_HISTORY_TTL_SECONDS)
CREATE TABLE IF NOT EXISTS results_summary_cache (
    test_name     TEXT NOT NULL,
    suite         TEXT NOT NULL,
    platform      TEXT NOT NULL,
    style         TEXT NOT NULL,
    flavor        TEXT NOT NULL DEFAULT '',
    commit_ref    TEXT NOT NULL DEFAULT '',

    has_history   INTEGER NOT NULL DEFAULT 1,

    pass_pct      INTEGER,
    fail_pct      INTEGER,
    timeout_pct   INTEGER,
    crash_pct     INTEGER,
    image_pct     INTEGER,
    audio_pct     INTEGER,
    text_pct      INTEGER,
    error_pct     INTEGER,
    warning_pct   INTEGER,

    fetched_at    INTEGER NOT NULL,

    PRIMARY KEY (test_name, suite, platform, style, flavor, commit_ref)
);


-- Cached responses from results.webkit.org's per-run endpoint: every run of one test on main whose
-- commit timestamp falls in a window. Stored as the JSON list the reader parses out of the
-- response, because a page never groups or counts by anything inside a single run.
--
-- An empty list is an answer — the test did not run on main in that window — and is what a test
-- upstream has never heard of leaves behind as well.
--
-- Invalidation: a window whose end was already RUNS_SETTLING_SECONDS in the past when it was
-- fetched holds every run it will ever hold and never expires. A window nearer than that keeps
-- filling as bots report, so it expires after RUNS_TTL_SECONDS.
CREATE TABLE IF NOT EXISTS test_runs_cache (
    test_name     TEXT NOT NULL,
    suite         TEXT NOT NULL,
    platform      TEXT NOT NULL,
    style         TEXT NOT NULL,
    flavor        TEXT NOT NULL DEFAULT '',

    -- Commit timestamps, not run times: the endpoint's bounds are the commit a run tested.
    after_at      INTEGER NOT NULL,
    before_at     INTEGER NOT NULL,

    runs          TEXT NOT NULL,
    fetched_at    INTEGER NOT NULL,

    PRIMARY KEY (test_name, suite, platform, style, flavor, after_at, before_at)
);


-- One row per convicted test whose pull request has a landed commit: what main did with that test
-- either side of the landing, and what that says about the conviction.
--
-- Only tests that could be asked about are here. A conviction whose pull request has not landed, or
-- whose title matched several commits, is left out and recounted from `landings` on every pass,
-- since a pull request that had not landed yet is the ordinary case rather than an answer.
--
-- Invalidation: `window_ends_at` is when the watched window closes, so a row decided before that
-- window had settled is decided again. One decided after it holds.
--
-- `landed_at` and `window_ends_at` are two different facts and both are stored: when the commit
-- landed, and the end of the window these counts were taken over. The gap between them is
-- ESCAPE_WINDOW_DAYS, which is configuration, so deriving either from the other would move the
-- landing time of every already-stored row the day that setting changes — a page would print a date
-- that never happened beside counts taken over the window that was really counted. `landed_at` is
-- nullable only so it can be added to a live table by ALTER TABLE; every write path fills it, and a
-- row left NULL is one `scripts/migrate_verdict_landed_at.py` found no landing for, which the page
-- says rather than guesses.
--
-- How strongly a test escaped is not stored: it is read off `runs_after` and `failed_after` by
-- `escapes.rarity_for_counts`, so it cannot disagree with the counts a page prints beside it.
--
-- The `recent_` columns are a separate question — is main failing this test now — answered over a
-- fresh window ending at `recent_checked_at`, and only ever for an ESCAPED row. All three are NULL
-- together until the first check, because a row nothing has asked about is not a row main has
-- recovered on, and storing a boolean instead would have made the two indistinguishable. For the
-- same reason `recent_runs = 0` is kept rather than collapsed: main running the test no times in the
-- window is not a recovery either, and only the count can tell the two apart.
CREATE TABLE IF NOT EXISTS escape_verdicts (
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

    landed_at         INTEGER,
    window_ends_at    INTEGER NOT NULL,
    decided_at        INTEGER NOT NULL,

    PRIMARY KEY (build_id, test_name)
);

CREATE INDEX IF NOT EXISTS index_escapes_verdict ON escape_verdicts(verdict);


-- Cached per-build false-positive classification, keyed by the threshold it was computed under.
--
-- Safe to share across every overlapping window the trend asks for, because a build's
-- classification depends only on its own failure lists and the history at classification time.
--
-- Invalidation: re-ingesting a build deletes its rows, and a row that had to give up on any test
-- expires after UNDETERMINED_TTL_SECONDS, since the history it lacked may exist by now. A row
-- that classified every test keeps its answer.
CREATE TABLE IF NOT EXISTS build_classifications (
    build_id                     INTEGER NOT NULL REFERENCES build_verdicts(build_id) ON DELETE CASCADE,
    threshold_pct                INTEGER NOT NULL,

    -- NULL means the build surfaced no tests to the author at all.
    bucket                       TEXT CHECK (bucket IN (
                                     'CLEAN', 'PARTIAL_FP', 'FALSE_RED', 'UNDETERMINED'
                                 )),
    surfaced_total               INTEGER NOT NULL,
    surfaced_pre_existing        INTEGER NOT NULL,
    surfaced_real                INTEGER NOT NULL,
    surfaced_undetermined        INTEGER NOT NULL,

    classified_at                INTEGER NOT NULL,

    PRIMARY KEY (build_id, threshold_pct)
);


-- One row per refresh, so every page can say how old its numbers are. `error` is set when a run
-- died, so a run with neither a finish nor an error is one still going.
CREATE TABLE IF NOT EXISTS refresh_runs (
    started_at       INTEGER PRIMARY KEY,
    finished_at      INTEGER,
    builders_walked  INTEGER NOT NULL DEFAULT 0,
    builds_ingested  INTEGER NOT NULL DEFAULT 0,
    builds_failed    INTEGER NOT NULL DEFAULT 0,
    error            TEXT
);


CREATE TABLE IF NOT EXISTS schema_migrations (
    migration_index  INTEGER PRIMARY KEY,
    applied_at       INTEGER NOT NULL
);
