<?php
if (!file_exists("ping.txt")) {
    $page = $_SERVER['PHP_SELF'];
    // This refresh header is unfortunate, but if the file doesn't
    // exist when this php script starts running, it won't notice
    // its creation even if we sleep and check again.
    header("Refresh: 1; url=$page");
    return;
}

echo "<html><body>\n";
echo "Ping sent successfully";
$pingFile = fopen("ping.txt", 'r');
while ($line = fgets($pingFile)) {
    echo "<br>";
    echo trim($line);
}
fclose($pingFile);
unlink("ping.txt");
echo "<script>";
echo "if (window.layoutTestController)";
echo "    layoutTestController.notifyDone();";
echo "</script>";
echo "</body></html>";
?>
