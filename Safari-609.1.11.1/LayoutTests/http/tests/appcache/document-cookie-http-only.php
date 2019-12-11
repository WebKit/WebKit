<?php
setcookie("scope", "manifest", 0, "/appcache/resources/scope1", null, null, true);
setcookie("scope", "script", 0, "/appcache/resources/scope2", null, null, true);
setcookie("foo", "bar");
?>
<html manifest="resources/scope1/cookie-protected-manifest.php">

<div>This tests that HttpOnly cookies set on the main document are used when accessing resources in the manifest.</div>
<div>This also tests that cookies used by appcache resource loading are scoped properly.</div>
<div id="log">Not checked cookie yet</div>
<div id="result1">Not run yet</div>
<div id="result">Not run yet</div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText()
    testRunner.waitUntilDone();
}

var cookieTest = document.cookie === "foo=bar" ? "PASSED" : "FAILED";
log.innerHTML = cookieTest + ": Some cookies should not be visible from JavaScript.";

function dynamicScriptLoad() {
    var script = document.createElement("script");
    script.type = "text/javascript";
    script.src = "./resources/scope2/cookie-protected-script.php";
    document.getElementsByTagName("head")[0].appendChild(script);

    setTimeout(() => {
        script = document.createElement("script");
        script.type = "text/javascript";
        script.src = "./resources/cookie-protected-script.php";
        document.getElementsByTagName("head")[0].appendChild(script);
    }, 0);
}

function cached()
{
    setTimeout("dynamicScriptLoad();", 0);
}

applicationCache.addEventListener('cached', cached, false);
</script>
</html>
