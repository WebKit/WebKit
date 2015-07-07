<?php

// prompt for login if not already present
if (!strlen($_SERVER["PHP_AUTH_USER"]) || !strlen($_SERVER["PHP_AUTH_PW"]))
{
    header("WWW-Authenticate: Basic realm=\"WebKit Bug Test\"");
    header("HTTP/1.0 401 Unauthorized");
    exit;
}

// do redirect if called for
$redirect_codes=array("301", "302", "303", "307");
if (in_array($_GET["redirect"], $redirect_codes))
{
    header("Location: http://redirectuser:redirectpassword@127.0.0.1:8000/misc/authentication-redirect-3/resources/auth-then-redirect.php?redirect=" . $_GET["redirect"], true, $_GET["redirect"]);
    exit;
}

echo "Unknown redirect parameter sent";

?>
