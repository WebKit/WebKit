<?php
header("Access-Control-Allow-Origin: *");

$color = $_SERVER['HTTP_HOST'] === $_GET['domain'] ? "green" : "red";
header("Content-Type: text/css");
echo("body { background-color: " . $color . ";}");
?>
