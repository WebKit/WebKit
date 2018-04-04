<?php
setcookie("foo", "bar", 0, "/", null, null, true);
?>
<html manifest="resources/document-cookie.manifest">

<div>This tests that HttpOnly cookies set on the main document are used when accessing resources in the manifest.</div>
<div id="log">Not checked cookie yet</div>
<div id="result">Not run yet</div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText()
    testRunner.waitUntilDone();
}

var cookieTest = document.cookie === "" ? "PASSED" : "FAILED";
log.innerHTML = cookieTest + ": Cookie should not be visible from JavaScript.";

function dynamicScriptLoad() {
    var script = document.createElement("script");
    script.type = "text/javascript";
    script.src = "./resources/cookie-protected-script.php";
    document.getElementsByTagName("head")[0].appendChild(script);
}

function cached()
{
    setTimeout("dynamicScriptLoad();", 0);
}

applicationCache.addEventListener('cached', cached, false);
</script>
</html>
