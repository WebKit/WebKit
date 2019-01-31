<?php
header("Cache-Control: no-store");
?>
<!DOCTYPE html>
<html>
<body>
<script src="/js-test-resources/js-test.js"></script>
<script>
description("Tests that lax same-site cookies are sent on a cross-site history navigation.");
jsTestIsAsync = true;

onload = function() {
<?php
if (isset($_COOKIE["foo"])) {
    echo "    testPassed('Was able to read the cookie after the back navigation');";
    echo "    finishJSTest();";
    echo "    return;";
} else {
    echo "    document.cookie = 'foo=bar; SameSite=lax';";
    echo "    setTimeout(() => { window.location = 'http://localhost:8000/cookies/same-site/resources/navigate-back.html'; }, 0);";
}
?>
}
</script>
</body>
</html>
