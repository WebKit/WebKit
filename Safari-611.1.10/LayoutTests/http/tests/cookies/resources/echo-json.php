<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Credentials: true");
header("Access-Control-Allow-External: true");
header("Access-Control-Allow-Origin: ${_SERVER['HTTP_ORIGIN']}");

echo json_encode($_COOKIE);
?>
