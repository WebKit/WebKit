<?php
$realm = $_SERVER['REQUEST_URI'];

header("Cache-Control: no-store");
header("WWW-Authenticate: OAuth realm=\"" . $realm . "\"");
header('HTTP/1.1 401 Unauthorized');
echo "Sent OAuth Challenge.";
exit;
?>
