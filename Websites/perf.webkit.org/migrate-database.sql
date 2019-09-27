BEGIN;

ALTER TABLE build_requests ADD COLUMN IF NOT EXISTS request_status_description varchar(1024) DEFAULT NULL;

END;