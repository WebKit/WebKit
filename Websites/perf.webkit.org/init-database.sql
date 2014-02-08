DROP TABLE run_iterations CASCADE;
DROP TABLE test_runs CASCADE;
DROP TABLE test_configurations CASCADE;
DROP TYPE test_configuration_type CASCADE;
DROP TABLE aggregators CASCADE;
DROP TABLE build_revisions CASCADE;
DROP TABLE builds CASCADE;
DROP TABLE builders CASCADE;
DROP TABLE repositories CASCADE;
DROP TABLE platforms CASCADE;
DROP TABLE test_metrics CASCADE;
DROP TABLE tests CASCADE;
DROP TABLE jobs CASCADE;
DROP TABLE reports CASCADE;
DROP TABLE tracker_repositories CASCADE;
DROP TABLE bug_trackers CASCADE;

CREATE TABLE platforms (
    platform_id serial PRIMARY KEY,
    platform_name varchar(64) NOT NULL,
    platform_hidden boolean NOT NULL DEFAULT FALSE);

CREATE TABLE repositories (
    repository_id serial PRIMARY KEY,
    repository_name varchar(64) NOT NULL,
    repository_url varchar(1024),
    repository_blame_url varchar(1024));

CREATE TABLE bug_trackers (
    tracker_id serial PRIMARY KEY,
    tracker_name varchar(64) NOT NULL,
    tracker_new_bug_url varchar(1024));

CREATE TABLE tracker_repositories (
    tracrepo_tracker integer NOT NULL REFERENCES bug_trackers ON DELETE CASCADE,
    tracrepo_repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE);

CREATE TABLE builders (
    builder_id serial PRIMARY KEY,
    builder_name varchar(64) NOT NULL UNIQUE,
    builder_password_hash character(64) NOT NULL,
    builder_build_url varchar(1024));

CREATE TABLE builds (
    build_id serial PRIMARY KEY,
    build_builder integer REFERENCES builders ON DELETE CASCADE,
    build_number integer NOT NULL,
    build_time timestamp NOT NULL,
    build_latest_revision timestamp,
    CONSTRAINT builder_build_time_tuple_must_be_unique UNIQUE(build_builder, build_number, build_time));
CREATE INDEX build_builder_index ON builds(build_builder);

CREATE TABLE build_revisions (
    revision_build integer NOT NULL REFERENCES builds ON DELETE CASCADE,
    revision_repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE,
    revision_value varchar(64) NOT NULL,
    revision_time timestamp,
    PRIMARY KEY (revision_repository, revision_build));
CREATE INDEX revision_build_index ON build_revisions(revision_build);
CREATE INDEX revision_repository_index ON build_revisions(revision_repository);

CREATE TABLE aggregators (
    aggregator_id serial PRIMARY KEY,
    aggregator_name varchar(64),
    aggregator_definition text);

CREATE TABLE tests (
    test_id serial PRIMARY KEY,
    test_name varchar(255) NOT NULL,
    test_parent integer REFERENCES tests ON DELETE CASCADE,
    test_url varchar(1024) DEFAULT NULL,
    CONSTRAINT parent_test_must_be_unique UNIQUE(test_parent, test_name));

CREATE TABLE test_metrics (
    metric_id serial PRIMARY KEY,
    metric_test integer NOT NULL REFERENCES tests ON DELETE CASCADE,
    metric_name varchar(64) NOT NULL,
    metric_aggregator integer REFERENCES aggregators ON DELETE CASCADE);

CREATE TYPE test_configuration_type as ENUM ('current', 'baseline', 'target');
CREATE TABLE test_configurations (
    config_id serial PRIMARY KEY,
    config_metric integer NOT NULL REFERENCES test_metrics ON DELETE CASCADE,
    config_platform integer NOT NULL REFERENCES platforms ON DELETE CASCADE,
    config_type test_configuration_type NOT NULL,
    config_is_in_dashboard boolean NOT NULL DEFAULT FALSE,
    CONSTRAINT configuration_must_be_unique UNIQUE(config_metric, config_platform, config_type));
CREATE INDEX config_platform_index ON test_configurations(config_platform);

CREATE TABLE test_runs (
    run_id serial PRIMARY KEY,
    run_config integer NOT NULL REFERENCES test_configurations ON DELETE CASCADE,
    run_build integer NOT NULL REFERENCES builds ON DELETE CASCADE,
    run_iteration_count_cache smallint,
    run_mean_cache double precision,
    run_sum_cache double precision,
    run_square_sum_cache double precision,
    CONSTRAINT test_config_build_must_be_unique UNIQUE(run_config, run_build));
CREATE INDEX run_config_index ON test_runs(run_config);
CREATE INDEX run_build_index ON test_runs(run_build);

CREATE TABLE run_iterations (
    iteration_run integer NOT NULL REFERENCES test_runs ON DELETE CASCADE,
    iteration_order smallint NOT NULL CHECK(iteration_order >= 0),
    iteration_group smallint CHECK(iteration_group >= 0),
    iteration_value double precision,
    iteration_relative_time float,
    PRIMARY KEY (iteration_run, iteration_order));

CREATE TABLE jobs (
    job_id serial PRIMARY KEY,
    job_type varchar(64) NOT NULL,
    job_created_at timestamp NOT NULL DEFAULT NOW(),
    job_started_at timestamp,
    job_started_by_pid integer,
    job_completed_at timestamp,
    job_attempts integer NOT NULL DEFAULT 0,
    job_payload text,
    job_log text);

CREATE TABLE reports (
    report_id serial PRIMARY KEY,
    report_builder integer NOT NULL REFERENCES builders ON DELETE RESTRICT,
    report_build_number integer,
    report_build integer REFERENCES builds,
    report_created_at timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    report_committed_at timestamp,
    report_content text,
    report_failure varchar(64),
    report_failure_details text);
