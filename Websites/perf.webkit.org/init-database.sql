DROP TABLE run_iterations CASCADE;
DROP TABLE test_runs CASCADE;
DROP TABLE test_configurations CASCADE;
DROP TYPE test_configuration_type CASCADE;
DROP TABLE aggregators CASCADE;
DROP TABLE builds CASCADE;
DROP TABLE committers CASCADE;
DROP TABLE commits CASCADE;
DROP TABLE build_commits CASCADE;
DROP TABLE build_slaves CASCADE;
DROP TABLE builders CASCADE;
DROP TABLE repositories CASCADE;
DROP TABLE platforms CASCADE;
DROP TABLE test_metrics CASCADE;
DROP TABLE tests CASCADE;
DROP TABLE reports CASCADE;
DROP TABLE tracker_repositories CASCADE;
DROP TABLE bug_trackers CASCADE;
DROP TABLE task_commits CASCADE;
DROP TABLE analysis_tasks CASCADE;
DROP TABLE analysis_strategies CASCADE;
DROP TYPE analysis_task_result_type CASCADE;
DROP TABLE build_triggerables CASCADE;
DROP TABLE triggerable_configurations CASCADE;
DROP TABLE triggerable_repositories CASCADE;
DROP TABLE bugs CASCADE;
DROP TABLE analysis_test_groups CASCADE;
DROP TABLE root_sets CASCADE;
DROP TABLE roots CASCADE;
DROP TABLE build_requests CASCADE;
DROP TYPE build_request_status_type CASCADE;


CREATE TABLE platforms (
    platform_id serial PRIMARY KEY,
    platform_name varchar(64) NOT NULL,
    platform_hidden boolean NOT NULL DEFAULT FALSE);

CREATE TABLE repositories (
    repository_id serial PRIMARY KEY,
    repository_parent integer REFERENCES repositories ON DELETE CASCADE,
    repository_name varchar(64) NOT NULL,
    repository_url varchar(1024),
    repository_blame_url varchar(1024),
    CONSTRAINT repository_name_must_be_unique UNIQUE(repository_parent, repository_name));

CREATE TABLE bug_trackers (
    tracker_id serial PRIMARY KEY,
    tracker_name varchar(64) NOT NULL,
    tracker_bug_url varchar(1024),
    tracker_new_bug_url varchar(1024));

CREATE TABLE tracker_repositories (
    tracrepo_tracker integer NOT NULL REFERENCES bug_trackers ON DELETE CASCADE,
    tracrepo_repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE);

CREATE TABLE builders (
    builder_id serial PRIMARY KEY,
    builder_name varchar(256) NOT NULL UNIQUE,
    builder_password_hash character(64),
    builder_build_url varchar(1024));

CREATE TABLE build_slaves (
    slave_id serial PRIMARY KEY,
    slave_name varchar(64) NOT NULL UNIQUE,
    slave_password_hash character(64));

CREATE TABLE builds (
    build_id serial PRIMARY KEY,
    build_builder integer REFERENCES builders ON DELETE CASCADE,
    build_slave integer REFERENCES build_slaves ON DELETE CASCADE,
    build_number integer NOT NULL,
    build_time timestamp NOT NULL,
    build_latest_revision timestamp,
    CONSTRAINT builder_build_time_tuple_must_be_unique UNIQUE(build_builder, build_number, build_time));
CREATE INDEX build_builder_index ON builds(build_builder);

CREATE TABLE committers (
    committer_id serial PRIMARY KEY,
    committer_repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE,
    committer_account varchar(320) NOT NULL,
    committer_name varchar(128),
    CONSTRAINT committer_in_repository_must_be_unique UNIQUE(committer_repository, committer_account));
CREATE INDEX committer_account_index ON committers(committer_account);
CREATE INDEX committer_name_index ON committers(committer_name);

CREATE TABLE commits (
    commit_id serial PRIMARY KEY,
    commit_repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE,
    commit_revision varchar(64) NOT NULL,
    commit_parent integer REFERENCES commits ON DELETE CASCADE,
    commit_time timestamp,
    commit_order integer,
    commit_committer integer REFERENCES committers ON DELETE CASCADE,
    commit_message text,
    commit_reported boolean NOT NULL DEFAULT FALSE,
    CONSTRAINT commit_in_repository_must_be_unique UNIQUE(commit_repository, commit_revision));
CREATE INDEX commit_time_index ON commits(commit_time);
CREATE INDEX commit_order_index ON commits(commit_order);

CREATE TABLE build_commits (
    commit_build integer NOT NULL REFERENCES builds ON DELETE CASCADE,
    build_commit integer NOT NULL REFERENCES commits ON DELETE CASCADE,
    PRIMARY KEY (commit_build, build_commit));

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
    config_runs_last_modified timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
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
    run_marked_outlier boolean,
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

CREATE OR REPLACE FUNCTION update_config_last_modified() RETURNS TRIGGER AS $update_config_last_modified$
    BEGIN
        IF TG_OP != 'DELETE' THEN
            UPDATE test_configurations SET config_runs_last_modified = (CURRENT_TIMESTAMP AT TIME ZONE 'UTC') WHERE config_id = NEW.run_config;
        ELSE
            UPDATE test_configurations SET config_runs_last_modified = (CURRENT_TIMESTAMP AT TIME ZONE 'UTC') WHERE config_id = OLD.run_config;
        END IF;
        RETURN NULL;
    END;
$update_config_last_modified$ LANGUAGE plpgsql;

CREATE TRIGGER update_config_last_modified AFTER INSERT OR UPDATE OR DELETE ON test_runs
    FOR EACH ROW EXECUTE PROCEDURE update_config_last_modified();

CREATE TABLE reports (
    report_id serial PRIMARY KEY,
    report_builder integer NOT NULL REFERENCES builders ON DELETE RESTRICT,
    report_slave integer REFERENCES build_slaves ON DELETE RESTRICT,
    report_build_number integer,
    report_build integer REFERENCES builds,
    report_created_at timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    report_committed_at timestamp,
    report_content text,
    report_failure varchar(64),
    report_failure_details text);

CREATE TABLE analysis_strategies (
    strategy_id serial PRIMARY KEY,
    strategy_name varchar(64) NOT NULL);

CREATE TYPE analysis_task_result_type as ENUM ('progression', 'regression', 'unchanged', 'inconclusive');
CREATE TABLE analysis_tasks (
    task_id serial PRIMARY KEY,
    task_name varchar(256) NOT NULL,
    task_author varchar(256),
    task_segmentation integer REFERENCES analysis_strategies,
    task_test_range integer REFERENCES analysis_strategies,
    task_created_at timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    task_platform integer REFERENCES platforms NOT NULL,
    task_metric integer REFERENCES test_metrics NOT NULL,
    task_start_run integer REFERENCES test_runs,
    task_start_run_time timestamp,
    task_end_run integer REFERENCES test_runs,
    task_end_run_time timestamp,
    task_result analysis_task_result_type,
    task_needed boolean,
    CONSTRAINT analysis_task_should_be_unique_for_range UNIQUE(task_start_run, task_end_run),
    CONSTRAINT analysis_task_should_not_be_associated_with_single_run
        CHECK ((task_start_run IS NULL AND task_end_run IS NULL) OR (task_start_run IS NOT NULL AND task_end_run IS NOT NULL)));

CREATE TABLE task_commits (
    taskcommit_task integer NOT NULL REFERENCES analysis_tasks ON DELETE CASCADE,
    taskcommit_commit integer NOT NULL REFERENCES commits ON DELETE CASCADE,
    taskcommit_is_fix boolean NOT NULL
    CONSTRAINT task_commit_must_be_unique UNIQUE(taskcommit_task, taskcommit_commit));

CREATE TABLE bugs (
    bug_id serial PRIMARY KEY,
    bug_task integer REFERENCES analysis_tasks NOT NULL,
    bug_tracker integer REFERENCES bug_trackers NOT NULL,
    bug_number integer NOT NULL);

CREATE TABLE build_triggerables (
    triggerable_id serial PRIMARY KEY,
    triggerable_name varchar(64) NOT NULL UNIQUE);

CREATE TABLE triggerable_repositories (
    trigrepo_triggerable integer REFERENCES build_triggerables NOT NULL,
    trigrepo_repository integer REFERENCES repositories NOT NULL,
    trigrepo_sub_roots boolean NOT NULL DEFAULT FALSE);

CREATE TABLE triggerable_configurations (
    trigconfig_test integer REFERENCES tests NOT NULL,
    trigconfig_platform integer REFERENCES platforms NOT NULL,
    trigconfig_triggerable integer REFERENCES build_triggerables NOT NULL,
    CONSTRAINT triggerable_must_be_unique_for_test_and_platform UNIQUE(trigconfig_test, trigconfig_platform));

CREATE TABLE analysis_test_groups (
    testgroup_id serial PRIMARY KEY,
    testgroup_task integer REFERENCES analysis_tasks NOT NULL,
    testgroup_name varchar(256),
    testgroup_author varchar(256),
    testgroup_created_at timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    testgroup_hidden boolean NOT NULL DEFAULT FALSE,
    CONSTRAINT testgroup_name_must_be_unique_for_each_task UNIQUE(testgroup_task, testgroup_name));
CREATE INDEX testgroup_task_index ON analysis_test_groups(testgroup_task);

CREATE TABLE root_sets (
    rootset_id serial PRIMARY KEY);

CREATE TABLE roots (
    root_set integer REFERENCES root_sets NOT NULL,
    root_commit integer REFERENCES commits NOT NULL);

CREATE TYPE build_request_status_type as ENUM ('pending', 'scheduled', 'running', 'failed', 'completed', 'canceled');
CREATE TABLE build_requests (
    request_id serial PRIMARY KEY,
    request_triggerable integer REFERENCES build_triggerables NOT NULL,
    request_platform integer REFERENCES platforms NOT NULL,
    request_test integer REFERENCES tests NOT NULL,
    request_group integer REFERENCES analysis_test_groups NOT NULL,
    request_order integer NOT NULL,
    request_root_set integer REFERENCES root_sets NOT NULL,
    request_status build_request_status_type NOT NULL DEFAULT 'pending',
    request_url varchar(1024),
    request_build integer REFERENCES builds,
    request_created_at timestamp NOT NULL DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    CONSTRAINT build_request_order_must_be_unique_in_group UNIQUE(request_group, request_order));
CREATE INDEX build_request_triggerable ON build_requests(request_triggerable);    
CREATE INDEX build_request_build ON build_requests(request_build);
