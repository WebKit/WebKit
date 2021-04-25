<?php
    include_once("../resources/cookie-utilities.php");

    if (shouldResetCookies()) {
        resetCookies();
        exit(0);
    }
    if (hostnameIsEqualToString("127.0.0.1")) {
        header("Location: http://localhost:8000" . $_SERVER["REQUEST_URI"]);
        exit(0);
    }
    wkSetCookie("strict", "14", Array("SameSite" => "Strict", "Max-Age" => 100, "path" => "/"));
    wkSetCookie("implicit-strict", "14", Array("SameSite" => NULL, "Max-Age" => 100, "path" => "/"));
    wkSetCookie("strict-because-invalid-SameSite-value", "14", Array("SameSite" => "invalid", "Max-Age" => 100, "path" => "/"));
    wkSetCookie("lax", "14", Array("SameSite" => "Lax", "Max-Age" => 100, "path" => "/"));
    wkSetCookie("normal", "14", Array("Max-Age" => 100, "path" => "/"));

?>
<!DOCTYPE html>
<html>
<head>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}
</script>
<meta http-equiv="refresh" content="0;http://localhost:8000/cookies/resources/echo-http-and-dom-cookies-and-notify-done.py">
</head>
</html>
