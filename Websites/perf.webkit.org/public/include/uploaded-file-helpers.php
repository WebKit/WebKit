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

function query_total_file_size($db, $user)
{
    if ($user)
        $count_result = $db->query_and_fetch_all('SELECT sum(file_size) as "sum" FROM uploaded_files WHERE file_deleted_at IS NULL AND file_author = $1', array($user));
    else
        $count_result = $db->query_and_fetch_all('SELECT sum(file_size) as "sum" FROM uploaded_files WHERE file_deleted_at IS NULL AND file_author IS NULL');
    if (!$count_result)
        return FALSE;
    return intval($count_result[0]["sum"]);
}

function create_uploaded_file_from_form_data($input_file)
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
        'author' => remote_user_name(),
        'filename' => $input_file['name'],
        'extension' => $file_extension,
        'mime' => $input_file['type'], // Sanitize MIME types.
        'size' => $input_file['size'],
        'sha256' => $file_sha256
    );
}

function upload_file_in_transaction($db, $input_file, $remote_user, $additional_work = NULL)
{
    // FIXME: Cleanup old files.

    if (config('uploadUserQuotaInMB') * MEGABYTES - query_total_file_size($db, $remote_user) < $input_file['size'])
        exit_with_error('FileSizeQuotaExceeded');

    $uploaded_file = create_uploaded_file_from_form_data($input_file);

    $db->begin_transaction();
    $file_row = $db->select_or_insert_row('uploaded_files', 'file',
        array('sha256' => $uploaded_file['sha256'], 'deleted_at' => null), $uploaded_file, '*');
    if (!$file_row)
        exit_with_error('FailedToInsertFileData');

    // A concurrent session may have inserted another file.
    if (config('uploadUserQuotaInMB') * MEGABYTES < query_total_file_size($db, $remote_user)) {
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

?>
