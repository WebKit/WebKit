<?php

if (!strlen($_SERVER["PHP_AUTH_USER"]) || !strlen($_SERVER["PHP_AUTH_PW"]))
{
    header("WWW-Authenticate: Basic realm=\"WebKit Bug Test\"");
    header("HTTP/1.0 401 Unauthorized");
    exit;
}

header("Location: http://127.0.0.1:8000/misc/authentication-redirect-4/resources/auth-echo.php", true, 301);
exit;

?>
