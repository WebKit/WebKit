<?php

if (!function_exists('sys_get_temp_dir')) {
    // Based on http://www.phpit.net/article/creating-zip-tar-archives-dynamically-php/2/
    function sys_get_temp_dir()
    {
        // Try to get from environment variable
        if (!empty($_ENV['TMP'])) {
            return realpath($_ENV['TMP']) . "/";
        } else if (!empty($_ENV['TMPDIR']) ) {
            return realpath($_ENV['TMPDIR']) . "/";
        } else if ( !empty($_ENV['TEMP'])) {
            return realpath( $_ENV['TEMP']) . "/";
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

?>
