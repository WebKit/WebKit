<?php
if ($_SERVER["HTTP_PURPOSE"] == "prefetch") {
    echo "<script>";
    echo "parent.window.postMessage('FAIL', '*');";
    echo "</script>";

    exit();
}

echo "<script>";
echo "parent.window.postMessage('PASS', '*');";
echo "</script>";
?>
