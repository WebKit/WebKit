<?php
    if (!$_COOKIE["cookieName"])
        setcookie("cookieName", "cookieValue");
?>
<html>
<head>
<script src="inspector-test.js"></script>
<script>

function test()
{
    var cookieName = "cookieName";
    var cookieDomain = "127.0.0.1";
    // Ensure cookie is deleted before testing.
    PageAgent.deleteCookie(cookieName, cookieDomain, step1);

    function step1()
    {
        InspectorTest.reloadPage(step2);
    }

    function step2()
    {
        WebInspector.Cookies.getCookiesAsync(step3);
    }

    function step3(allCookies)
    {
        for (var i = 0; i < allCookies.length; i++) {
          var cookie = allCookies[i];
            if (cookie.name() === cookieName && cookie.domain() == cookieDomain) {
                InspectorTest.addResult("Cookie: " + cookie.name() + "=" + cookie.value() + ".");
            }
        }
        // Ensure cookie is deleted after testing.
        PageAgent.deleteCookie(cookieName, cookieDomain, step4);
    }

    function step4()
    {
        InspectorTest.completeTest();
    }
}

</script>
</head>
<body onload="runTest()">
    <p>This tests that the cookie set by main resource is shown in resources panel.</p>
    <a href="https://bugs.webkit.org/show_bug.cgi?id=65770">bug 65770</a>.
</body>
</html>
