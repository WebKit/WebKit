<?php

require('../include/json-header.php');
require('../include/uploaded-file-helpers.php');

function main($path)
{
    if (count($path) > 1)
        exit_with_error('InvalidRequest');

    $db = connect();
    if (count($path) && $path[0]) {
        $file_id = intval($path[0]);
        $file_row = $db->select_first_row('uploaded_files', 'file', array('id' => $file_id));
        if (!$file_row)
            exit_with_404();
        $file_path = uploaded_file_path_for_row($file_row);
        return stream_file_content($file_path, $file_row['file_sha256'], $file_row['file_filename']);
    }

    $sha256 = array_get($_GET, 'sha256');
    if ($sha256) {
        $file_row = $db->select_first_row('uploaded_files', 'file', array('sha256' => $sha256, 'deleted_at' => null));
        if (!$file_row)
            exit_with_error('NotFound');
        exit_with_success(array('uploadedFile' => format_uploaded_file($file_row)));
    }
    exit_with_error('InvalidArguments');
}

define('STREAM_CHUNK_SIZE', 64 * 1024);

function stream_file_content($uploaded_file, $etag, $disposition_name)
{
    if (!file_exists($uploaded_file))
        exit_with_404();

    $file_size = filesize($uploaded_file);
    $last_modified = gmdate('D, d M Y H:i:s', filemtime($uploaded_file)) . ' GMT';
    $file_handle = fopen($uploaded_file, "rb");
    if (!$file_handle)
        exit_with_404();

    $headers = getallheaders();

    // We don't support multi-part range request. e.g. bytes=1-3,4-5
    $range = parse_range_header(array_get($headers, 'Range'), $file_size);
    if ($range && (!array_key_exists('If-Range', $headers) || $headers['If-Range'] == $last_modified || $headers['If-Range'] == $etag)) {
        assert($range['start'] >= 0);
        if ($range['start'] > $range['end'] || $range['end'] >= $file_size) {
            header('HTTP/1.1 416 Range Not Satisfiable');
            header("Content-Range: bytes */$file_size");
            exit(416);
        }
        $start = $range['start'];
        $end = $range['end'];
        $content_length = $end - $start + 1;
        header('HTTP/1.1 206 Partial Content');
        header("Content-Range: bytes $start-$end/$file_size");
        fseek($file_handle, $start);
    } else {
        $content_length = $file_size;
        header("Accept-Ranges: bytes");
        header("ETag: $etag");
    }

    $output_buffer = fopen('php://output', 'wb');
    $encoded_filename = urlencode($disposition_name);

    header('Content-Type: application/octet-stream');
    header('Content-Length: ' . $content_length);
    header("Content-Disposition: attachment; filename*=utf-8''$encoded_filename");
    header("Last-Modified: $last_modified");

    set_time_limit(0);
    while (!feof($file_handle) && $content_length) {
        $is_end = $content_length < STREAM_CHUNK_SIZE;
        $chunk_size = $is_end ? $content_length : STREAM_CHUNK_SIZE;
        $chunk = fread($file_handle, $chunk_size);
        $content_length -= $chunk_size;
        fwrite($output_buffer, $chunk, $chunk_size);
        flush();
    }

    exit(0);
}

function parse_range_header($range_header, $file_size)
{
    // We don't support multi-part range request. e.g. bytes=1-3,4-5
    $matches = array();
    $end_byte = $file_size;
    if (!$range_header || !preg_match('/^\s*bytes\s*=\s*((\d+)-(\d*)|-(\d+))\s*$/', $range_header, $matches))
        return NULL;

    $end_byte = $file_size - 1;
    if ($matches[2]) {
        $start_byte = intval($matches[2]);
        if ($matches[3])
            $end_byte = intval($matches[3]);
        else
            $end_byte = $file_size - 1;
    } else {
        $suffix_length = intval($matches[4]);
        if ($file_size < $suffix_length)
            $start_byte = 0;
        else
            $start_byte = $file_size - $suffix_length;
    }

    return array('start' => $start_byte, 'end' => $end_byte);
}

function exit_with_404()
{
    header($_SERVER['SERVER_PROTOCOL'] . ' 404 Not Found');
    exit_with_error('NotFound');
}

main(array_key_exists('PATH_INFO', $_SERVER) ? explode('/', trim($_SERVER['PATH_INFO'], '/')) : array());

?>
