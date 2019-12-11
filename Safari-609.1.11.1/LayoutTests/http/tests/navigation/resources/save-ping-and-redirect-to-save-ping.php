<?php
require_once 'ping-file-path.php';

$DO_NOT_CLEAR_COOKIES = true; // Used by save-Ping.php
require_once 'save-Ping.php';

header('HTTP/1.1 307');
header('Location: save-Ping.php' . (!empty($_SERVER['QUERY_STRING']) ? '?' . $_SERVER['QUERY_STRING'] : ''));
?>
