DO $$

BEGIN
ALTER TABLE build_requests ADD COLUMN IF NOT EXISTS request_status_description varchar(1024) DEFAULT NULL;
ALTER TABLE platforms ADD COLUMN  IF NOT EXISTS platform_group integer REFERENCES platform_groups DEFAULT NULL;
ALTER TABLE commits ADD COLUMN IF NOT EXISTS commit_revision_identifier varchar(64) DEFAULT NULL;
ALTER TABLE commits DROP CONSTRAINT commit_string_identifier_in_repository_must_be_unique;
ALTER TABLE commits ADD CONSTRAINT commit_string_identifier_in_repository_must_be_unique UNIQUE(commit_repository, commit_revision_identifier);

CREATE TABLE IF NOT EXISTS platform_groups (
    platformgroup_id serial PRIMARY KEY,
    platformgroup_name varchar(64) NOT NULL,
    CONSTRAINT platform_group_name_must_be_unique UNIQUE (platformgroup_name));

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