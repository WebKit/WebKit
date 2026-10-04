<?php

define('MEGABYTES', 1024 * 1024);

function format_uploaded_file($file_row)
{
    return array(
        'id' => $file_row['file_id'],
        'size' => $file_row['file_size'],
        'createdAt' => Database::to_js_time($file_row['file_created_at']),
        'deletedAt' => Database::to_js_time($file_row['file_deleted_at']),
        'mime' => $file_row['file_mime'],
        'filename' => $file_row['file_filename'],
        'extension' => $file_row['file_extension'],
        'author' => $file_row['file_author'],
        'sha256' => $file_row['file_sha256']);
}

function uploaded_file_path_for_row($file_row)
{
    return config_path('uploadDirectory', $file_row['file_id'] . $file_row['file_extension']);
}

function validate_uploaded_file($field_name)
{
    if (array_get($_SERVER, 'CONTENT_LENGTH') && empty($_POST) && empty($_FILES))
        exit_with_error('FileSizeLimitExceeded');

    if (!is_dir(config_path('uploadDirectory', '')))
        exit_with_error('NotSupported');

    $input_file = array_get($_FILES, $field_name);
    if (!$input_file)
        exit_with_error('NoFileSpecified');

    if ($input_file['error'] == UPLOAD_ERR_INI_SIZE || $input_file['error'] == UPLOAD_ERR_FORM_SIZE)
        exit_with_error('FileSizeLimitExceeded');

    if ($input_file['error'] != UPLOAD_ERR_OK)
        exit_with_error('FailedToUploadFile', array('name' => $input_file['name'], 'error' => $input_file['error']));

    if (config('uploadFileLimitInMB') * MEGABYTES < $input_file['size'])
        exit_with_error('FileSizeLimitExceeded');

    return $input_file;
}

function query_file_usage_for_user($db, $user)
{
    if ($user)
        $count_result = $db->query_and_fetch_all('SELECT sum(file_size) as "sum" FROM uploaded_files WHERE file_deleted_at IS NULL AND file_author = $1', array($user));
    else
        $count_result = $db->query_and_fetch_all('SELECT sum(file_size) as "sum" FROM uploaded_files WHERE file_deleted_at IS NULL AND file_author IS NULL');
    if (!$count_result)
        exit_with_error('FailedToQueryDiskUsagePerUser');
    return intval($count_result[0]["sum"]);
}

function query_total_file_usage($db)
{
    $count_result = $db->query_and_fetch_all('SELECT sum(file_size) as "sum" FROM uploaded_files WHERE file_deleted_at IS NULL');
    if (!$count_result)
        exit_with_error('FailedToQueryTotalDiskUsage');
    return intval($count_result[0]["sum"]);
}

function create_uploaded_file_from_form_data($input_file, $remote_user)
{
    $file_sha256 = hash_file('sha256', $input_file['tmp_name']);
    if (!$file_sha256)
        exit_with_error('FailedToComputeSHA256');

    $matches = array();
    $file_extension = null;
    if (preg_match('/(\.[a-zA-Z0-9]{1,5}){1,2}$/', $input_file['name'], $matches)) {
        $file_extension = $matches[0];
        assert(strlen($file_extension) <= 16);
    }

    return array(
        'author' => $remote_user,
        'filename' => $input_file['name'],
        'extension' => $file_extension,
        'mime' => $input_file['type'], // Sanitize MIME types.
        'size' => $input_file['size'],
        'sha256' => $file_sha256
    );
}

function upload_file_in_transaction($db, $input_file, $remote_user, $additional_work = NULL)
{
    $new_file_size = $input_file['size'];
    $user_quota_mb = config('uploadUserQuotaInMB');
    $total_quota_mb = config('uploadTotalQuotaInMB');
    $current_user_usage = query_file_usage_for_user($db, $remote_user);
    $current_total_usage = query_total_file_usage($db);
    $user_space_available = $user_quota_mb * MEGABYTES - $current_user_usage;
    $total_space_available = $total_quota_mb * MEGABYTES - $current_total_usage;

    if ($user_space_available < $new_file_size || $total_space_available < $new_file_size) {
        $file_deletion_cutoff = microtime(true) - config('uploadFileGracePeriodInHours', 12) * 3600;

        $user_quota_bytes = $user_quota_mb * MEGABYTES;
        $total_quota_bytes = $total_quota_mb * MEGABYTES;
        $prune_result = false;
        $prune_ratio = config('uploadQuotaPruningRatio', 0.80);

        // If this upload will put the user over their personal usage quota, prune down their usage to the configured ratio of the personal quota max
        if ($user_space_available < $new_file_size) {
            $target_usage = $user_quota_bytes * $prune_ratio;
            $size_to_free = $current_user_usage - $target_usage + $new_file_size;
            $prune_result = prune_user_files($db, $size_to_free, $remote_user, $file_deletion_cutoff);

            if ($prune_result) {
                $current_total_usage = query_total_file_usage($db);
                $total_space_available = $total_quota_bytes - $current_total_usage;
            }
        }

        // If this upload will exceed the total database quota, prune down the total usage to the configured ratio of the total quota max
        // Prune files from oldest to newest
        if ($total_space_available < $new_file_size) {
            $target_total_usage = $total_quota_bytes * $prune_ratio;
            $total_to_free = $current_total_usage - $target_total_usage + $new_file_size;
            $prune_result = prune_global_oldest_files($db, $total_to_free, $file_deletion_cutoff) || $prune_result;
        }

        $post_prune_user_usage = query_file_usage_for_user($db, $remote_user);
        $post_prune_total_usage = query_total_file_usage($db);
        $post_prune_user_space = $user_quota_bytes - $post_prune_user_usage;
        $post_prune_total_space = $total_quota_bytes - $post_prune_total_usage;

        if ($post_prune_user_space < $new_file_size || $post_prune_total_space < $new_file_size) {
            return array(
                'status' => 'FileSizeQuotaExceeded',
                'usages' => array(
                    'pre_prune' => array(
                        'user_usage_mb' => round($current_user_usage / MEGABYTES, 2),
                        'total_usage_mb' => round($current_total_usage / MEGABYTES, 2)
                    ),
                    'post_prune' => array(
                        'user_usage_mb' => round($post_prune_user_usage / MEGABYTES, 2),
                        'total_usage_mb' => round($post_prune_total_usage / MEGABYTES, 2)
                    )
                ),
                'quotas' => array(
                    'user_quota_mb' => $user_quota_mb,
                    'total_quota_mb' => $total_quota_mb,
                    'grace_period_hours' => config('uploadFileGracePeriodInHours', 12)
                ),
                'prune_failed' => true
            );
        }
    }

    $uploaded_file = create_uploaded_file_from_form_data($input_file, $remote_user);

    $db->begin_transaction();
    $file_row = $db->select_or_insert_row('uploaded_files', 'file',
        array('sha256' => $uploaded_file['sha256']), $uploaded_file, '*');
    if (!$file_row)
        exit_with_error('FailedToInsertFileData');

    // A concurrent session may have inserted another file.
    if (config('uploadUserQuotaInMB') * MEGABYTES < query_file_usage_for_user($db, $remote_user)
        || config('uploadTotalQuotaInMB') * MEGABYTES < query_total_file_usage($db)) {
        $db->rollback_transaction();
        exit_with_error('FileSizeQuotaExceeded');
    }

    if ($additional_work) {
        $error = $additional_work($db, $file_row);
        if ($error) {
            $db->rollback_transaction();
            exit_with_error($error['status'], $error);
        }
    }

    $new_path = uploaded_file_path_for_row($file_row);
    if (!move_uploaded_file($input_file['tmp_name'], $new_path)) {
        $db->rollback_transaction();
        exit_with_error('FailedToMoveUploadedFile');
    }

    if ($file_row['file_deleted_at']) {
        if (!$db->query_and_get_affected_rows("UPDATE uploaded_files SET file_created_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC', file_deleted_at = NULL WHERE file_id = $1", array($file_row['file_id']))) {
            $db->rollback_transaction();
            exit_with_error('FailedToClearDeletedAtField');
        }
        $file_row = $db->select_first_row('uploaded_files', 'file', array('id' => $file_row['file_id']));
    }

    $db->commit_transaction();

    return format_uploaded_file($file_row);
}

function prune_user_files($db, $size_needed, $remote_user, $file_deletion_cutoff)
{
    $file_filters = 'AND extract(epoch from file_created_at at time zone \'utc\') <= $1';
    $file_filters .= $remote_user ? ' AND file_author = $2' : ' AND file_author IS NULL';
    $params = $remote_user? array($file_deletion_cutoff, $remote_user) : array($file_deletion_cutoff);
    $files_to_delete = array();
    $total_size_freed = 0;

    // 1. Delete old build products not associated with any pending or in-progress builds.
    $build_product_query = $db->query("SELECT DISTINCT file_id, file_extension, file_size, file_created_at FROM uploaded_files, commit_set_items
        WHERE file_id = commitset_root_file AND commitset_requires_build is TRUE AND file_deleted_at IS NULL
            AND NOT EXISTS (SELECT request_id FROM build_requests WHERE request_commit_set = commitset_set AND request_status <= 'running')
            $file_filters
        ORDER BY file_created_at", $params);
    if (!$build_product_query)
        return FALSE;
    while ($row = $db->fetch_next_row($build_product_query)) {
        if (!$row) return FALSE;
        $files_to_delete[] = $row;
        $total_size_freed += $row['file_size'];
        if ($total_size_freed >= $size_needed)
            break;
    }

    // 2. If still need more space, delete any uploaded file not associated with any pending or in-progress builds.
    if ($total_size_freed < $size_needed) {
        $unused_file_query = $db->query("SELECT file_id, file_extension, file_size FROM uploaded_files
            WHERE NOT EXISTS (SELECT request_id FROM build_requests, commit_set_items
                WHERE (commitset_root_file = file_id OR commitset_patch_file = file_id)
                    AND request_commit_set = commitset_set AND request_status <= 'running')
                AND file_deleted_at IS NULL
                $file_filters
            ORDER BY file_created_at", $params);
        if (!$unused_file_query)
            return FALSE;
        while ($row = $db->fetch_next_row($unused_file_query)) {
            if (!$row) return FALSE;
            $files_to_delete[] = $row;
            $total_size_freed += $row['file_size'];
            if ($total_size_freed >= $size_needed)
                break;
        }
    }

    if (empty($files_to_delete)) {
        return FALSE;
    }

    return batch_delete_files($db, $files_to_delete);
}

function prune_global_oldest_files($db, $size_to_free, $file_deletion_cutoff)
{
    $files_to_delete = array();
    $total_size_freed = 0;

    // 1. First try to delete old build products from all users
    $build_products_query = $db->query("
        SELECT DISTINCT file_id, file_extension, file_size, file_created_at
        FROM uploaded_files, commit_set_items
        WHERE file_id = commitset_root_file
            AND commitset_requires_build is TRUE
            AND file_deleted_at IS NULL
            AND extract(epoch from file_created_at at time zone 'utc') <= $1
            AND NOT EXISTS (
                SELECT request_id FROM build_requests
                WHERE request_commit_set = commitset_set
                    AND request_status <= 'running'
            )
        ORDER BY file_created_at", array($file_deletion_cutoff));

    if (!$build_products_query)
        return FALSE;

    while ($row = $db->fetch_next_row($build_products_query)) {
        if (!$row) break;
        $files_to_delete[] = $row;
        $total_size_freed += $row['file_size'];
        if ($total_size_freed >= $size_to_free)
            break;
    }

    // 2. If still need more space, get any oldest files from all users
    if ($total_size_freed < $size_to_free) {
        $unused_files_query = $db->query("
            SELECT file_id, file_extension, file_size
            FROM uploaded_files
            WHERE file_deleted_at IS NULL
                AND extract(epoch from file_created_at at time zone 'utc') <= $1
                AND NOT EXISTS (
                    SELECT request_id FROM build_requests, commit_set_items
                    WHERE (commitset_root_file = file_id OR commitset_patch_file = file_id)
                        AND request_commit_set = commitset_set
                        AND request_status <= 'running'
                )
            ORDER BY file_created_at", array($file_deletion_cutoff));

        if (!$unused_files_query)
            return FALSE;

        while ($row = $db->fetch_next_row($unused_files_query)) {
            if (!$row) break;
            $files_to_delete[] = $row;
            $total_size_freed += $row['file_size'];
            if ($total_size_freed >= $size_to_free)
                break;
        }
    }

    if (empty($files_to_delete)) {
        return FALSE;
    }

    return batch_delete_files($db, $files_to_delete);
}

function batch_delete_files($db, $files_to_delete)
{
    if (count($files_to_delete) == 0)
        return TRUE;

    $successfully_deleted = array();
    foreach ($files_to_delete as $file_row) {
        $file_path = uploaded_file_path_for_row($file_row);
        if (!file_exists($file_path) || @unlink($file_path)) {
            $successfully_deleted[] = $file_row['file_id'];
        }
    }

    if (empty($successfully_deleted))
        return FALSE;

    $db->begin_transaction();
    $placeholders = array();
    for ($i = 0; $i < count($successfully_deleted); $i++) {
        $placeholders[] = '$' . ($i + 1);
    }
    $query = "UPDATE uploaded_files SET file_deleted_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
              WHERE file_id IN (" . implode(',', $placeholders) . ")";
    if (!$db->query_and_get_affected_rows($query, $successfully_deleted)) {
        $db->rollback_transaction();
        return FALSE;
    }
    $db->commit_transaction();
    return TRUE;
}

function prune_files_older_than_months($db, $months = null) {
    if ($months === null) {
        $months = config('uploadFileCleanupMonths', 4);
    }
    $cutoff_time = time() - ($months * 30 * 24 * 3600);
    $upload_dir = config_path('uploadDirectory', '');
    $total_deleted = 0;
    $total_size_freed = 0;
    $total_failed = 0;

    $files_query = $db->query("
        SELECT file_id, file_extension, file_size
        FROM uploaded_files
        WHERE file_deleted_at IS NULL
            AND extract(epoch from file_created_at at time zone 'utc') < $1
            AND NOT EXISTS (
                SELECT 1 FROM build_requests, commit_set_items
                WHERE (commitset_root_file = file_id OR commitset_patch_file = file_id)
                    AND request_commit_set = commitset_set
                    AND request_status IN ('pending', 'scheduled', 'running')
            )
        ORDER BY file_created_at",
        array($cutoff_time)
    );

    if (!$files_query) {
        return array(
            'deleted_count' => 0,
            'failed_count' => 0,
            'space_freed_mb' => 0
        );
    }

    $successfully_deleted = array();
    while ($file = $db->fetch_next_row($files_query)) {
        $file_path = $upload_dir . '/' . $file['file_id'] . $file['file_extension'];
        $file_deleted = false;
        if (file_exists($file_path)) {
            $file_deleted = @unlink($file_path);
            if (!$file_deleted) {
                $total_failed++;
            }
        } else {
            $file_deleted = true;
        }
        if ($file_deleted) {
            $successfully_deleted[] = $file['file_id'];
            $total_size_freed += $file['file_size'];
        }
    }

    if (!empty($successfully_deleted)) {
        $placeholders = array();
        for ($i = 0; $i < count($successfully_deleted); $i++) {
            $placeholders[] = '$' . ($i + 1);
        }
        $affected = $db->query_and_get_affected_rows(
            "UPDATE uploaded_files
             SET file_deleted_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
             WHERE file_id IN (" . implode(',', $placeholders) . ")",
            $successfully_deleted
        );
        $total_deleted = $affected ? $affected : 0;
    }

    return array(
        'deleted_count' => $total_deleted,
        'failed_count' => $total_failed,
        'space_freed_mb' => round($total_size_freed / MEGABYTES, 2)
    );
}

function cleanup_zombie_files($db) {
    $upload_dir = config_path('uploadDirectory', '');
    $cleaned_count = 0;
    $space_freed = 0;
    $failed_count = 0;

    $db_files = array();
    $db_query = $db->query("SELECT file_id FROM uploaded_files WHERE file_deleted_at IS NULL");
    while ($row = $db->fetch_next_row($db_query)) {
        $db_files[$row['file_id']] = true;
    }

    $files = scandir($upload_dir);
    foreach ($files as $file_name) {
        if ($file_name == '.' || $file_name == '..') continue;
        if (preg_match('/^(\d+)((\.[A-Za-z0-9]{1,5}){1,2})$/', $file_name, $matches)) {
            $file_id = $matches[1];
            if (!isset($db_files[$file_id])) {
                $file_path = $upload_dir . '/' . $file_name;
                $file_size = @filesize($file_path);
                if (@unlink($file_path)) {
                    $cleaned_count++;
                    $space_freed += $file_size;
                } else {
                    $failed_count++;
                }
            }
        }
    }

    return array(
        'files_deleted' => $cleaned_count,
        'files_failed' => $failed_count,
        'disk_space_freed_mb' => round($space_freed / MEGABYTES, 2)
    );
}

function cleanup_ghost_records($db) {
    $upload_dir = config_path('uploadDirectory', '');
    $ghost_count = 0;
    $ghost_size = 0;

    $records_query = $db->query("
        SELECT file_id, file_extension, file_size
        FROM uploaded_files
        WHERE file_deleted_at IS NULL
        ORDER BY file_id"
    );

    if (!$records_query) {
        return array(
            'db_records_cleaned' => 0,
            'quota_freed_mb' => 0
        );
    }

    $ghost_ids = array();
    while ($row = $db->fetch_next_row($records_query)) {
        $file_path = $upload_dir . '/' . $row['file_id'] . $row['file_extension'];
        if (!file_exists($file_path)) {
            $ghost_ids[] = $row['file_id'];
            $ghost_size += $row['file_size'];
        }
    }
    if (!empty($ghost_ids)) {
        $placeholders = array();
        for ($i = 0; $i < count($ghost_ids); $i++) {
            $placeholders[] = '$' . ($i + 1);
        }
        $affected = $db->query_and_get_affected_rows(
            "UPDATE uploaded_files
             SET file_deleted_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
             WHERE file_id IN (" . implode(',', $placeholders) . ")",
            $ghost_ids
        );
        $ghost_count = $affected ? $affected : 0;
    }

    return array(
        'db_records_cleaned' => $ghost_count,
        'quota_freed_mb' => round($ghost_size / MEGABYTES, 2)
    );
}

?>