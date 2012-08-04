<html>
<head>
<script>

function loaded()
{
    if (window.sessionStorage.getItem("postedForm")) {
        window.sessionStorage.removeItem("postedForm");
        if (window.testRunner) {
            testRunner.setCustomPolicyDelegate(false);
            testRunner.notifyDone();
        }
        return;
    }

    window.sessionStorage.postedForm = "true";
    document.getElementById("testform").submit();
}

</script>
</head>
<body onload="loaded();">
<form id="testform" action="goback-with-policydelegate.php" method="post">
<input id="somefield" type="text" value="form data">
</form>

This page was reached via a form POST.  After navigating away, there should be no form resubmission nag when navigating back here.
</body>
</html>
