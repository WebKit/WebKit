<?php

require_once('../include/json-header.php');
require_once('../include/uploaded-file-helpers.php');

function main()
{
    $input_file = validate_uploaded_file('newFile');

    if (!verify_token(array_get($_POST, 'token')))
        exit_with_error('InvalidToken');

    $current_user = remote_user_name();
    $db = connect();

    $uploaded_file = upload_file_in_transaction($db, $input_file, $current_user);

    exit_with_success(array('uploadedFile' => $uploaded_file));
}

main();

?>
