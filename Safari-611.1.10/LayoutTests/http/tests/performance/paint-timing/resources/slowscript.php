<?php
if (isset($_GET["delay"]))
    sleep($_GET["delay"]);
header('Cache-Control: no-cache, must-revalidate');
header('Content-Type: text/javascript');

echo($_GET["script"])
?>

