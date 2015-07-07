<html>
<head>
<script>

function loaded()
{
    if (window.testRunner)
        testRunner.setCustomPolicyDelegate(true, true);

    window.history.back();
}

</script>
</head>
<body onload="loaded();">
This page turns on DRT's custom policy delegate then navigates back a page.
If you're running the test in the browser, you should not get the form resubmission nag.
</body>
</html>
