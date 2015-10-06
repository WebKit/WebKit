<?php

if (!isset($_COOKIE["key"])) {
    echo "FAIL: Cookie is not set";
    exit;
}

if ($_GET["key"] == $_COOKIE["key"]) {
    $fp = fopen("../../../../resources/Ahem.ttf", "rb");
    header("Content-type: application/octet-stream");
    header("HTTP/1.0 200 OK");
    fpassthru($fp);
} else {
    echo "FAIL: Cookie: {$_COOKIE['key']}, Query: {$_GET['key']}";
    exit;
}

?>
