ALTER TABLE triggerable_configurations ADD COLUMN IF NOT EXISTS trigconfig_id serial PRIMARY KEY;
ALTER TYPE analysis_test_group_repetition_type ADD VALUE IF NOT EXISTS 'paired-parallel';

DO $$

BEGIN

IF NOT EXISTS (SELECT NULL FROM pg_type WHERE typname = 'analysis_test_group_repetition_type') THEN
    CREATE TYPE analysis_test_group_repetition_type as ENUM ('alternating', 'sequential', 'paired-parallel');
END IF;

ALTER TABLE build_requests ADD COLUMN IF NOT EXISTS request_status_description varchar(1024) DEFAULT NULL;
ALTER TABLE platforms ADD COLUMN  IF NOT EXISTS platform_group integer REFERENCES platform_groups DEFAULT NULL;
ALTER TABLE commits ADD COLUMN IF NOT EXISTS commit_revision_identifier varchar(64) DEFAULT NULL;
ALTER TABLE analysis_test_groups ADD COLUMN IF NOT EXISTS testgroup_repetition_type analysis_test_group_repetition_type NOT NULL DEFAULT 'alternating';
ALTER TABLE commits DROP CONSTRAINT IF EXISTS commit_string_identifier_in_repository_must_be_unique;
ALTER TABLE commits ADD CONSTRAINT commit_string_identifier_in_repository_must_be_unique UNIQUE(commit_repository, commit_revision_identifier);

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'commits' AND COLUMN_NAME = 'commit_order' AND DATA_TYPE = 'integer') THEN
    ALTER TABLE commits ALTER commit_order TYPE bigint;
END IF;

CREATE TABLE IF NOT EXISTS platform_groups (
    platformgroup_id serial PRIMARY KEY,
    platformgroup_name varchar(64) NOT NULL,
    CONSTRAINT platform_group_name_must_be_unique UNIQUE (platformgroup_name));

IF NOT EXISTS (SELECT NULL FROM information_schema.tables WHERE TABLE_NAME = 'triggerable_configuration_repetition_types') THEN
    CREATE TABLE triggerable_configuration_repetition_types (
        configrepetition_config INTEGER NOT NULL REFERENCES triggerable_configurations ON DELETE CASCADE,
        configrepetition_type analysis_test_group_repetition_type NOT NULL,
        PRIMARY KEY (configrepetition_config, configrepetition_type));

    INSERT INTO triggerable_configuration_repetition_types (configrepetition_config, configrepetition_type)
        SELECT trigconfig_id, 'alternating' FROM triggerable_configurations;

    INSERT INTO triggerable_configuration_repetition_types (configrepetition_config, configrepetition_type)
        SELECT trigconfig_id, 'sequential' FROM triggerable_configurations;
END IF;

ALTER TABLE platforms ADD COLUMN IF NOT EXISTS platform_group integer REFERENCES platform_groups DEFAULT NULL;

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'builds' AND COLUMN_NAME = 'build_number') THEN
    ALTER TABLE builds ALTER build_number TYPE varchar(64);
    ALTER TABLE builds RENAME build_number TO build_tag;
    ALTER TABLE builds DROP CONSTRAINT  builder_build_time_tuple_must_be_unique;
    ALTER TABLE builds ADD CONSTRAINT builder_build_time_tuple_must_be_unique UNIQUE(build_builder, build_tag, build_time);
END IF;

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'builds' AND COLUMN_NAME = 'build_slave') THEN
    ALTER TABLE builds RENAME build_slave TO build_worker;
END IF;

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'reports' AND COLUMN_NAME = 'report_slave') THEN
    ALTER TABLE reports RENAME report_slave TO report_worker;
END IF;

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'build_slaves') THEN
    ALTER TABLE build_slaves RENAME slave_id TO worker_id;
    ALTER TABLE build_slaves RENAME slave_name TO worker_name;
    ALTER TABLE build_slaves RENAME slave_password_hash TO worker_password_hash;
    ALTER TABLE build_slaves RENAME TO build_workers;
END IF;

END$$;
