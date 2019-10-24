DO $$

BEGIN
ALTER TABLE build_requests ADD COLUMN IF NOT EXISTS request_status_description varchar(1024) DEFAULT NULL;

IF EXISTS (SELECT NULL FROM information_schema.columns WHERE TABLE_NAME = 'builds' AND COLUMN_NAME = 'build_number') THEN
    ALTER TABLE builds ALTER build_number TYPE varchar(64);
    ALTER TABLE builds RENAME build_number TO build_tag;
    ALTER TABLE builds DROP CONSTRAINT  builder_build_time_tuple_must_be_unique;
    ALTER TABLE builds ADD CONSTRAINT builder_build_time_tuple_must_be_unique UNIQUE(build_builder, build_tag, build_time);
END IF;

END$$;