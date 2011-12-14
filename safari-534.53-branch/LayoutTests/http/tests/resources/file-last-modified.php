<?php
date_default_timezone_set('UTC');
$filePath = $_GET['path'];
if (file_exists($filePath)) {
    echo date("U", filemtime($filePath));
}
?>
