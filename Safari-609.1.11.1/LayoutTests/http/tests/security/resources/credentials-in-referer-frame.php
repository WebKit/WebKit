<?php
 if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="WebKit test - credentials-in-referer"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
 }
?>
<script>
if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function log(message)
{
    parent.document.getElementById("log").innerHTML += message + "<br>";
}

window.onload = function() {
    var xhr = new XMLHttpRequest;
    xhr.open("GET", "credentials-in-referer.php", false);
    xhr.send(null);
    log("Sync XHR: " + (xhr.responseText.match(/FAIL/) ? "FAIL" : "PASS"));

    xhr.open("GET", "credentials-in-referer.php", true);
    xhr.send(null);
    xhr.onload = onXHRLoad;
}

function onXHRLoad(evt)
{
    log("Async XHR: " + (evt.target.responseText.match(/FAIL/) ? "FAIL" : "PASS"));
    log("DONE");
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<script src="credentials-in-referer.php"></script>
