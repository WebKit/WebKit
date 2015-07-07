<?php
    setcookie("test_cookie", "1", 0, "/");
?>
<!DOCTYPE html>
<html>
<script>
function checkCookie()
{
    if (document.cookie.indexOf("test_cookie=1") < 0)
        document.getElementById("log").innerHTML += "Cookie is NOT set";
    else
        document.getElementById("log").innerHTML += "Cookie is set";
    document.cookie = "test_cookie=0; path=/; expires=Thu, 01-Jan-1970 00:00:01 GMT";

    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<body onload="checkCookie()">
<div id="log"></div>
</body>
</html>
