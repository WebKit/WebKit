<?php
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-store, no-cache, must-revalidate");
    header("Pragma: no-cache");
    header("Content-Type: text/plain");
    setcookie("TestCookie", "TestCookieValue");

    echo("Cookie set.");
?>
