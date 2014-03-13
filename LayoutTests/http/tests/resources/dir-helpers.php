<?php
require_once 'portabilityLayer.php';

function tmpdir($dir=FALSE, $prefix='php')
{
    if ($dir) {
        $tmpFile = tempnam($dir, $prefix);
    } else {
        if (!sys_get_temp_dir())
            return FALSE;
        $tmpFile = tempnam(sys_get_temp_dir(), $prefix);
    }

    if (!file_exists($tmpFile))
        return FALSE;
    unlink($tmpFile);
    mkdir($tmpFile);
    if (!is_dir($tmpFile))
        return FALSE;

    return $tmpFile;
}

function rrmdir($dir)
{
    if (is_dir($dir)) {
        $objects = array_diff(scandir($dir), array(".", ".."));
        foreach ($objects as $object)
            (is_dir($dir . "/" . $object)) ? rrmdir($dir . "/" . $object) : unlink($dir . "/" . $object);
        reset($objects);
        rmdir($dir);
    }
}

function rcopy($src, $dst)
{
    if (file_exists($dst))
        rrmdir($dst);
    if (is_dir($src)) {
        mkdir($dst);
        $files = array_diff(scandir($src), array(".", ".."));
        foreach ($files as $file)
          rcopy("$src/$file", "$dst/$file");
    } else if (file_exists($src)) {
        copy($src, $dst);
    }
}

function first_dir($name, $dir=FALSE)
{
    $result = FALSE;
    $root = $dir ? $dir : "./";
    $queue = array(realpath($root));

    while (sizeof($queue)) {
        $vertex = array_pop($queue);
        $objects = array_diff(scandir($vertex), array(".", ".."));
        foreach ($objects as $object) {
            $fullPath = $vertex . "/" . $object;
            if (is_dir($fullPath)) {
                if ($name == basename($fullPath)) {
                    $result = $fullPath;
                    goto cleanup;
                } else {
                    array_unshift($queue, $fullPath);
                }
            }
        }
    }


cleanup:

    return $result;
}
?>

