<!-- webkit-test-runner [ dumpJSConsoleLogInStdErr=true ] -->
<html manifest="resources/404-resource.manifest">
<div>This tests that a manifest that contains a missing file will not crash the browser even if the main resource keeps loading after the error occurs.</div>
<div id="result">FAILURE</div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function finish()
{
    window.stop();
    document.getElementById('result').innerHTML = 'SUCCESS';
    document.body.removeChild(document.getElementById('container'));
    if (window.testRunner)
        testRunner.notifyDone();
}

// The test needs to attempt to load a subresource after appcache load fails. There is no way
// to observe appcache loading progress before onload fires, so we just let it run for a while.
window.setTimeout(finish, 2000);
</script>

<div id="container">
<?php
    while (True) {
        usleep(200000);
        print "<img src='/does-not-exist.png'>\n";
        flush();
    }
?>
</div>
</html>
