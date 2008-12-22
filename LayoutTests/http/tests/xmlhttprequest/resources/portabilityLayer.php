<?php

if (!function_exists('sys_get_temp_dir')) {
    // Based on http://www.phpit.net/article/creating-zip-tar-archives-dynamically-php/2/
    // If the builtin PHP sys_get_temp_dir doesn't exist, we replace it with one that will
    // try to guess from the environment.  Since sys_get_temp_dir() doesn't return a trailing
    // slash on all system (see comment at http://us.php.net/sys_get_temp_dir), we don't
    // append a trailing slash, and expect callers to append one when needed.
    function sys_get_temp_dir()
    {
        // Try to get from environment variable
        if (!empty($_ENV['TMP'])) {
            return realpath($_ENV['TMP']);
        } else if (!empty($_ENV['TMPDIR']) ) {
            return realpath($_ENV['TMPDIR']);
        } else if ( !empty($_ENV['TEMP'])) {
            return realpath( $_ENV['TEMP']);
        }
        return FALSE;
    }
}

if (!function_exists('file_put_contents')) {
    function file_put_contents($filename, $data)
    {
        $handle = fopen($filename, "w");
        fwrite($handle, $data);
        fclose($handle);
    }
}

function ensureTrailingSlash($path)
{
    if ($path[strlen($path) - 1] == '/')
        return $path;
    return $path . '/';
}
?>
