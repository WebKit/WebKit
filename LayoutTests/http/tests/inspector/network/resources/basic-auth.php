<?php
$expectedUsername = "goodUsername";
$expectedPassword = "goodPassword";
$realm = $_SERVER["REQUEST_URI"];

if (!isset($_SERVER['PHP_AUTH_USER']) || $_SERVER['PHP_AUTH_USER'] != $expectedUsername ||
    !isset($_SERVER['PHP_AUTH_PW']) || $_SERVER['PHP_AUTH_PW'] != $expectedPassword) {
    header("WWW-Authenticate: Basic realm=\"" . $realm . "\"");
    header("HTTP/1.0 401 Unauthorized");
    print "Bad username:password (" . $_SERVER['PHP_AUTH_USER'] . ":" . $_SERVER["PHP_AUTH_PW"] . ")";
    exit;
}
?>
Success
