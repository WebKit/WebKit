-- Configuration file for postgres

-- Drop existing schema. WARNING: this will delete all data in the database
DROP SCHEMA IF EXISTS public CASCADE;
CREATE SCHEMA public;

SET search_path TO public;
SET constraint_exclusion = partition;
SET work_mem='1GB';

CREATE EXTENSION plsh;

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
    id bigserial PRIMARY KEY,
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

-- Code specific to the table partitioning functions below were borrowed from:
-- https://blog.engineyard.com/2013/scaling-postgresql-performance-table-partitioning
CREATE OR REPLACE FUNCTION
public.server_partition_function()
RETURNS TRIGGER AS 
$BODY$
DECLARE
_new_time int;
_tablename text;
_startdate text;
_enddate text;
_result record;
BEGIN
_tablename := 'results_partition_'||CURRENT_DATE;

-- Check if the partition needed for the current record exists
PERFORM 1
FROM   pg_catalog.pg_class c
JOIN   pg_catalog.pg_namespace n ON n.oid = c.relnamespace
WHERE  c.relkind = 'r'
AND    c.relname = _tablename
AND    n.nspname = 'public';

-- If the partition needed does not yet exist, then we create it:
-- Note that || is string concatenation (joining two strings to make one)
IF NOT FOUND THEN
_enddate:=_startdate::timestamp + INTERVAL '1 day';
EXECUTE 'CREATE TABLE public.' || quote_ident(_tablename) || ' (
) INHERITS (public.results)';

-- Table permissions are not inherited from the parent.
-- If permissions change on the master be sure to change them on the child also.
EXECUTE 'ALTER TABLE public.' || quote_ident(_tablename) || ' OWNER TO test-results-user';
EXECUTE 'GRANT ALL ON TABLE public.' || quote_ident(_tablename) || ' TO test-results-user';

-- Indexes are defined per child, so we assign a default index that uses the partition columns
EXECUTE 'CREATE INDEX ' || quote_ident(_tablename||'_indx1') || ' ON public.' || quote_ident(_tablename) || ' (time, id)';
END IF;

-- Insert the current record into the correct partition, which we are sure will now exist.
EXECUTE 'INSERT INTO public.' || quote_ident(_tablename) || ' VALUES ($1.*)' USING NEW;
RETURN NULL;
END;
$BODY$
LANGUAGE plpgsql;


CREATE TRIGGER results_trigger
BEFORE INSERT ON public.results
FOR EACH ROW EXECUTE PROCEDURE public.server_partition_function();


-- Maintenance function
CREATE OR REPLACE FUNCTION
public.partition_maintenance(in_tablename_prefix text, in_master_tablename text, in_asof date)
RETURNS text AS
$BODY$
DECLARE
_result record;
_current_time_without_special_characters text;
_out_filename text;
_return_message text;
return_message text;
BEGIN
-- Get the current date in YYYYMMDD_HHMMSS.ssssss format
_current_time_without_special_characters := 
REPLACE(REPLACE(REPLACE(NOW()::TIMESTAMP WITHOUT TIME ZONE::TEXT, '-', ''), ':', ''), ' ', '_');

-- Initialize the return_message to empty to indicate no errors hit
_return_message := '';

--Validate input to function
IF in_tablename_prefix IS NULL THEN
RETURN 'Child table name prefix must be provided'::text;
ELSIF in_master_tablename IS NULL THEN
RETURN 'Master table name must be provided'::text;
ELSIF in_asof IS NULL THEN
RETURN 'You must provide the as-of date, NOW() is the typical value';
END IF;

FOR _result IN SELECT * FROM pg_tables WHERE schemaname='public' LOOP

IF POSITION(in_tablename_prefix in _result.tablename) > 0 AND char_length(substring(_result.tablename from '[0-9-]*$')) <> 0 AND (in_asof - interval '90 days') > to_timestamp(substring(_result.tablename from '[0-9-]*$'),'YYYY-MM-DD') THEN

_out_filename := '/Volumes/Data/postgres/partition_dump/' || _result.tablename || '_' || _current_time_without_special_characters || '.sql.gz';
BEGIN
-- Call function export_partition(child_table text) to export the file
PERFORM public.export_partition(_result.tablename::text, _out_filename::text);
-- If the export was successful drop the child partition
EXECUTE 'DROP TABLE public.' || quote_ident(_result.tablename);
_return_message := return_message || 'Dumped table: ' || _result.tablename::text || ', ';
RAISE NOTICE 'Dumped table %', _result.tablename::text;
EXCEPTION WHEN OTHERS THEN
_return_message := return_message || 'ERROR dumping table: ' || _result.tablename::text || ', ';
RAISE NOTICE 'ERROR DUMPING %', _result.tablename::text;
END;
END IF;
END LOOP;

RETURN _return_message || 'Done'::text;
END;
$BODY$
LANGUAGE plpgsql VOLATILE COST 100;

ALTER FUNCTION public.partition_maintenance(text, text, date) OWNER TO test-results-user;

GRANT EXECUTE ON FUNCTION public.partition_maintenance(text, text, date) TO test-results-user;
GRANT EXECUTE ON FUNCTION public.partition_maintenance(text, text, date) TO test-results-user;

-- The function below is again generic and allows you to pass in the table name of the file you would like to export to the operating system and the name of the compressed file that will contain the exported table.
-- Helper Function for partition maintenance
CREATE OR REPLACE FUNCTION public.export_partition(text, text) RETURNS text AS
$BASH$
#!/bin/bash
tablename=${1}
filename=${2}
# NOTE: pg_dump must be available in the path.
/usr/local/bin/pg_dump -U test-results-user -t public."${tablename}" test-results-user | gzip -c > ${filename} ;
$BASH$
LANGUAGE plsh;

ALTER FUNCTION public.export_partition(text, text) OWNER TO test-results-user;

GRANT EXECUTE ON FUNCTION public.export_partition(text, text) TO test-results-user;
GRANT EXECUTE ON FUNCTION public.export_partition(text, text) TO test-results-user;