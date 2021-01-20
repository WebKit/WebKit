<!DOCTYPE html>
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
<script src="../../resources/cookie-utilities.js"></script>
<script>_setCachedCookiesJSON('<?php echo json_encode($_COOKIE); ?>')</script>
</head>
<body>
<script>
window.jsTestIsAsync = true;

description("Tests that Same-Site cookies for 127.0.0.1 are sent with a request initiated from an iframe- and processed by a service worker- with the same origin.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldHaveCookieWithValue("strict", "11");
    await shouldHaveCookieWithValue("implicit-strict", "11");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "11");
    await shouldHaveCookieWithValue("lax", "11");

    debug("<br>Cookies visible in DOM:");
    shouldHaveDOMCookieWithValue("strict", "11");
    shouldHaveDOMCookieWithValue("implicit-strict", "11");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "11");
    shouldHaveDOMCookieWithValue("lax", "11");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>
