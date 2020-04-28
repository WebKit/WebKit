<?php
if (isset($_GET["delay"]))
    sleep($_GET["delay"]);
header('Cache-Control: no-cache, must-revalidate');
header('Location: style.css');
?>
