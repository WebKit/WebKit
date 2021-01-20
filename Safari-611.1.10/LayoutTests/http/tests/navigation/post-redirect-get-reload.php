<?php
if($_SERVER['REQUEST_METHOD'] == "POST") {
    header("Location: post-redirect-get-reload.php", true, 303);
    exit;
}
?>
<body>
1. Submit a form<br>
1a. The form redirects to a get.<br>
2. Reload<br><br>
The reload should not trigger a form resubmission warning.

<form name="form" action="post-redirect-get-reload.php" method="post"><input type="submit"></input></form>
<script>

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    if (window.sessionStorage["prgl-state"] == null) {
        window.sessionStorage["prgl-state"] = 'submitted';
        document.form.submit();
    } else {
        window.sessionStorage.clear();
        testRunner.waitForPolicyDelegate();
        window.internals.forceReload(false);
    }
}
</script>
