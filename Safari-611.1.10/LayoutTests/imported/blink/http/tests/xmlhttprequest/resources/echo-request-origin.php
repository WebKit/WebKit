<?php
header('Content-Type: text/plain');
if (isset($_SERVER['HTTP_ORIGIN']))
    echo 'Origin: ' . $_SERVER['HTTP_ORIGIN'];
else
    echo 'Origin: <missing>';
?>
