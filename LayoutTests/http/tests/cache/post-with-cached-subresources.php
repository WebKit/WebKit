<?php
require_once '../resources/portabilityLayer.php';

// This test loads an uncacheable main resource and a cacheable image subresource.
// We request this page as a GET, then reload this page with a POST.
// On the post request, the image should be loaded from the cache and no HTTP
// request should be sent. The image resource will return 404 if it receives
// a second request, which will cause us to FAIL.
// See https://bugs.webkit.org/show_bug.cgi?id=38690
header('Cache-Control: no-cache, no-store, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('Expires: ' . gmdate(DATE_RFC1123, time()-1));
?>
<html>
<body>
<div id="result"></div>
<script>
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}
    
function logAndFinish(message)
{
    document.getElementById("result").appendChild(document.createTextNode(message));
    var xhr = new XMLHttpRequest;
    xhr.open("GET", "../resources/reset-temp-file.php?filename=post.tmp", false);
    xhr.send(null);
    if (window.testRunner)
        testRunner.notifyDone();
}
</script>
<?php
clearstatcache();
if ($_POST["submit"] == "finish") {
    // The initial load of the image might have been done from the cache. In that case, touch the
    // state file to indicate a load has already occurred.
    if (!file_exists(sys_get_temp_dir() . "/post.tmp")) {
        echo "<script>";
        echo "xhr = new XMLHttpRequest;";
        echo "xhr.open('GET', '../resources/touch-temp-file.php?filename=post.tmp', false);";
        echo "xhr.send(null);";
        echo "</script>";
    }
    echo "<img src='resources/post-image-to-verify.php' onload=\"logAndFinish('PASS');\" onerror=\"logAndFinish('FAIL');\"></img>";
} else {
    echo "<form action='post-with-cached-subresources.php' method='post'>";
    echo "<input type='submit' id='submit' name='submit' value='finish'>";
    echo "</form>";
    echo "<img src='resources/post-image-to-verify.php' onload=\"document.getElementById('submit').click();\" onerror=\"logAndFinish('FAIL on initial load');\"></img>";
} 
?>
</body>
</html>
