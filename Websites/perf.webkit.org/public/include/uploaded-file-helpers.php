<?php

function format_uploaded_file($file_row)
{
    return array(
        'id' => $file_row['file_id'],
        'size' => $file_row['file_size'],
        'createdAt' => $file_row['file_created_at'],
        'mime' => $file_row['file_mime'],
        'filename' => $file_row['file_filename'],
        'author' => $file_row['file_author'],
        'sha256' => $file_row['file_sha256']);
}

function uploaded_file_path_for_row($file_row)
{
    return config_path('uploadDirectory', $file_row['file_id'] . $file_row['file_extension']);
}

?>
