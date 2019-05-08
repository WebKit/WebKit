<?php
if ($_SERVER["HTTP_PURPOSE"] == "prefetch") {
    header('Cache-Control: max-age=3600');
    header("Access-Control-Allow-Origin: http://127.0.0.1:8000");

    echo "<script>";
    echo "parent.window.postMessage('FAIL', '*');";
    echo "</script>";

    exit();
}

header("Access-Control-Allow-Origin: http://127.0.0.1:8000");

echo "<script>";
echo "parent.window.postMessage('PASS', '*');";
echo "</script>";
?>
