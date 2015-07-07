DROP TABLE results CASCADE;
DROP TABLE tests CASCADE;
DROP TABLE build_revisions CASCADE;
DROP TABLE builds CASCADE;
DROP TABLE slaves CASCADE;
DROP TABLE repositories CASCADE;
DROP TABLE builders CASCADE;

CREATE TABLE builders (
    id serial PRIMARY KEY,
    master varchar(64) NOT NULL,
    name varchar(64) NOT NULL UNIQUE,
    build_url varchar(1024));

CREATE TABLE repositories (
    id serial PRIMARY KEY,
    name varchar(64) NOT NULL,
    url varchar(1024),
    blame_url varchar(1024));

CREATE TABLE slaves (
    id serial PRIMARY KEY,
    name varchar(128) NOT NULL UNIQUE);

CREATE TABLE builds (
    id serial PRIMARY KEY,
    builder integer REFERENCES builders ON DELETE CASCADE,
    number integer NOT NULL,
    start_time timestamp,
    end_time timestamp,
    slave integer REFERENCES slaves ON DELETE CASCADE,
    is_processed boolean,
    CONSTRAINT builder_and_build_number_must_be_unique UNIQUE(builder, number));
CREATE INDEX build_builder_index ON builds(builder);
CREATE INDEX build_slave_index ON builds(slave);

CREATE TABLE build_revisions (
    build integer NOT NULL REFERENCES builds ON DELETE CASCADE,
    repository integer NOT NULL REFERENCES repositories ON DELETE CASCADE,
    value varchar(64) NOT NULL,
    time timestamp,
    PRIMARY KEY (repository, build));
CREATE INDEX revision_build_index ON build_revisions(build);
CREATE INDEX revision_repository_index ON build_revisions(repository);
CREATE INDEX revision_time_index ON build_revisions(time);

CREATE TABLE tests (
    id serial PRIMARY KEY,
    name varchar(1024) NOT NULL UNIQUE,
    category varchar(64) NOT NULL,
    reftest_type varchar(64));

CREATE TABLE results (
    id serial PRIMARY KEY,
    test integer REFERENCES tests ON DELETE CASCADE,
    build integer REFERENCES builds ON DELETE CASCADE,
    expected varchar(64) NOT NULL,
    actual varchar(64) NOT NULL,
    modifiers varchar(64) NOT NULL,
    time integer,
    is_flaky boolean,
    CONSTRAINT results_test_build_must_be_unique UNIQUE(test, build));
CREATE INDEX results_test ON results(test);
CREATE INDEX results_build ON results(build);
CREATE INDEX results_is_flaky ON results(is_flaky);

SET work_mem='50MB';
