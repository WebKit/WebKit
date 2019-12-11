<?php
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-cache, must-revalidate");
    header("Pragma: no-cache");
    if ($_GET["csp"]) {
        $contentSecurityPolicy = $_GET["csp"];
        // If the magic quotes option is enabled, the CSP could be escaped and
        // the test would fail.
        if (get_magic_quotes_gpc())
            $contentSecurityPolicy = stripslashes($contentSecurityPolicy);
        header("Content-Security-Policy: " . $contentSecurityPolicy);
    }
?>
