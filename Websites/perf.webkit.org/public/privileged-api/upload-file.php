<?php

ini_set('upload_max_filesize', '1025M');
ini_set('post_max_size', '1025M');
require_once('../include/json-header.php');
require_once('../include/uploaded-file-helpers.php');

define('MEGABYTES', 1024 * 1024);

function main()
{
    if (array_get($_SERVER, 'CONTENT_LENGTH') && empty($_POST) && empty($_FILES))
        exit_with_error('FileSizeLimitExceeded2');

    if (!verify_token(array_get($_POST, 'token')))
        exit_with_error('InvalidToken');

    if (!is_dir(config_path('uploadDirectory', '')))
        exit_with_error('NotSupported');

    $input_file = array_get($_FILES, 'newFile');
    if (!$input_file)
        exit_with_error('NoFileSpecified');

    if ($input_file['error'] != UPLOAD_ERR_OK)
        exit_with_error('FailedToUploadFile', array('name' => $input_file['name'], 'error' => $input_file['error']));

    if (config('uploadFileLimitInMB') * MEGABYTES < $input_file['size'])
        exit_with_error('FileSizeLimitExceeded');

    $uploaded_file = create_uploaded_file_from_form_data($input_file);

    $current_user = remote_user_name();
    $db = connect();

    // FIXME: Cleanup old files.

    if (config('uploadUserQuotaInMB') * MEGABYTES - query_total_file_size($db, $current_user) < $input_file['size'])
        exit_with_error('FileSizeQuotaExceeded');

    $db->begin_transaction();
    $file_row = $db->select_or_insert_row('uploaded_files', 'file',
        array('sha256' => $uploaded_file['sha256'], 'deleted_at' => null), $uploaded_file, '*');
    if (!$file_row)
        exit_with_error('FailedToInsertFileData');

    // A concurrent session may have inserted another file.
    if (config('uploadUserQuotaInMB') * MEGABYTES < query_total_file_size($db, $current_user)) {
        $db->rollback_transaction();
        exit_with_error('FileSizeQuotaExceeded');
    }

    $new_path = uploaded_file_path_for_row($file_row);
    if (!move_uploaded_file($input_file['tmp_name'], $new_path)) {
        $db->rollback_transaction();
        exit_with_error('FailedToMoveUploadedFile');
    }
    $db->commit_transaction();

    exit_with_success(array('uploadedFile' => format_uploaded_file($file_row)));
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

main();

?>
