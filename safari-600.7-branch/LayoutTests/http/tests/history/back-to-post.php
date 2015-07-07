<?php
header("Cache-control: no-cache, no-store");
header("Pragma: no-cache");
?>

<html>
<body>
<p>Test for <a href="rdar://problem/6791439">rdar://problem/6791439</a>
Getting an error page instead of login page navigating back in gmail.</p>
<div id="result">FAIL: Script did not run</div>

<form action="back-to-post.php?1" method="post">
<input name="a" value="b">
<input id="mysubmit" type="submit" name="Submit" value="Submit">
</form>
<script>

function submitForm()
{
    // Submit form in a timeout to make sure that we create a new back/forward list item.
    setTimeout(function() {document.forms[0].submit()}, 0);
}

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

// Disable b/f cache (not necessary in DRT since the b/f cache is disabled by default).
if (!window.testRunner)
    window.onunload = function() {}

if (document.location.search == "") {
    window.name = ""; // Use window.name to communicate between steps.
    submitForm();
} else if (document.location.search == "?1") {
    if (window.name == "finish") {
        window.name = "";
        document.getElementById("result").innerHTML = 'PASS';
        if (window.testRunner)
            testRunner.notifyDone();
    } else {
        document.forms[0].action = "?2";
        submitForm();
    }
} else {
    // Test that going back to form submission result works.
    window.name = "finish";
    history.back();
}
</script>
</body>
</html>
