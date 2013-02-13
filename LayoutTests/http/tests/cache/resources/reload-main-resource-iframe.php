<?php
if (file_exists(sys_get_temp_dir() . "/reload-main-resource.tmp")) {
    unlink(sys_get_temp_dir() . "/reload-main-resource.tmp");
    echo "<body>";
    echo "<script>";
    echo "window.parent.finish();";
    echo "</script>";
    echo "</body>";
    exit();
}

$tmpFile = fopen(sys_get_temp_dir() . "/reload-main-resource.tmp", 'w');
fclose($tmpFile);

$max_age = 12 * 31 * 24 * 60 * 60; //one year
$last_modified = gmdate(DATE_RFC1123, time() + $max_age);
$expires = gmdate(DATE_RFC1123, time() + $max_age);

header('Cache-Control: public, max-age=' . $max_age);
header('Expires: ' . $expires);
header('Content-Type: text/html');
header('Last-Modified: ' . $last_modified);
?>
