<?php

define('MEGABYTES', 1024 * 1024);

function format_uploaded_file($file_row)
{
    return array(
        'id' => $file_row['file_id'],
        'size' => $file_row['file_size'],
        'createdAt' => Database::to_js_time($file_row['file_created_at']),
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
    if (config('uploadUserQuotaInMB') * MEGABYTES - query_file_usage_for_user($db, $remote_user) < $new_file_size
        || config('uploadTotalQuotaInMB') * MEGABYTES - query_total_file_usage($db) < $new_file_size) {
        // Instead of <quota> - <used> - <new file size>, just ask for <new file size>
        // since finding files to delete is an expensive operation.
        if (!prune_old_files($db, $new_file_size, $remote_user))
            exit_with_error('FileSizeQuotaExceeded');
    }

    $uploaded_file = create_uploaded_file_from_form_data($input_file, $remote_user);

    $db->begin_transaction();
    $file_row = $db->select_or_insert_row('uploaded_files', 'file',
        array('sha256' => $uploaded_file['sha256'], 'deleted_at' => null), $uploaded_file, '*');
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

    $db->commit_transaction();

    return format_uploaded_file($file_row);
}

function delete_file($db, $file_row)
{
    $db->begin_transaction();

    if (!$db->query_and_get_affected_rows("UPDATE uploaded_files SET file_deleted_at = CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
        WHERE file_id = $1", array($file_row['file_id']))) {
        $db->rollback_transaction();
        return FALSE;
    }

    $file_path = uploaded_file_path_for_row($file_row);
    // The file may have been deleted by a concurrent session by the time we get here.
    if (file_exists($file_path) && !unlink($file_path)) {
        $db->rollback_transaction();
        return FALSE;
    }

    $db->commit_transaction();
    return TRUE;
}

function prune_old_files($db, $size_needed, $remote_user)
{
    $user_filter = $remote_user ? 'AND file_author = $1' : 'AND file_author IS NULL';
    $params = $remote_user ? array($remote_user) : array();

    // 1. Delete old build products created for a patch not associated with any pending or in-progress builds.
    $build_product_query = $db->query("SELECT file_id, file_extension, file_size FROM uploaded_files, commit_set_items
        WHERE file_id = commitset_root_file AND commitset_patch_file IS NOT NULL AND file_deleted_at IS NULL
            AND NOT EXISTS (SELECT request_id FROM build_requests WHERE request_commit_set = commitset_set AND request_status <= 'running')
            $user_filter
        ORDER BY file_created_at", $params);
    if (!$build_product_query)
        return FALSE;
    while ($row = $db->fetch_next_row($build_product_query)) {
        if (!$row || !delete_file($db, $row))
            return FALSE;
        $size_needed -= $row['file_size'];
        if ($size_needed <= 0)
            return TRUE;
    }

    // 2. Delete any uploaded file not associated with any pending or in-progress builds.
    $unused_file_query = $db->query("SELECT file_id, file_extension, file_size FROM uploaded_files
        WHERE NOT EXISTS (SELECT request_id FROM build_requests, commit_set_items
            WHERE (commitset_root_file = file_id OR commitset_patch_file = file_id)
                AND request_commit_set = commitset_set AND request_status <= 'running')
            AND file_deleted_at IS NULL
            $user_filter
        ORDER BY file_created_at", $params);
    if (!$unused_file_query)
        return FALSE;
    while ($row = $db->fetch_next_row($unused_file_query)) {
        if (!$row || !delete_file($db, $row))
            return FALSE;
        $size_needed -= $row['file_size'];
        if ($size_needed <= 0)
            return TRUE;
    }
    return FALSE;
}

?>
