<?php
require_once 'ping-file-path.php';

$DO_NOT_CLEAR_COOKIES = true;
require_once 'save-ping.php';

header('HTTP/1.1 307');
header('Location: save-ping.php' . (isset($_SERVER['QUERY_STRING']) ? '?' . $_SERVER['QUERY_STRING'] : ''));
?>
