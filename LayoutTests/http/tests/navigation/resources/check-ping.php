<?php
if (!file_exists("ping.txt")) {
    $page = $_SERVER['PHP_SELF'];
    // This refresh header is unfortunate, but if the file doesn't
    // exist when this php script starts running, it won't notice
    // its creation even if we sleep and check again.
    header("Refresh: 1; url=$page");
    return;
}

unlink("ping.txt");

echo "PASS\n";
echo "<script>\n";
echo "if (window.layoutTestController)\n";
echo "    layoutTestController.notifyDone();\n";
echo "</script>";
?>
