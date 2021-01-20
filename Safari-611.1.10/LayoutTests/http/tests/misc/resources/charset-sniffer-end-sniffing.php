<?php
header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, no-store, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/html");

echo "<html><script>parent.firstScript();</script>\n";

// Dump a lot of text to exceed 1024 chars after finishing the head section.
echo "<body>\n";
for ($i = 0; $i < 10000; ++$i)
    echo "a";

// Send off the first portion so it executes before the rest is received.
ob_flush();
flush();
sleep(1);

echo "<html><script>parent.secondScript();</script>\n";
echo "</body></html>\n";
?>
