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

description("Tests that a SameSite Lax cookie for 127.0.0.1 is sent with a redirect from a page with the same origin.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldHaveCookieWithValue("strict", "20");
    await shouldHaveCookieWithValue("implicit-strict", "20");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "20");
    await shouldHaveCookieWithValue("lax", "20");

    debug("<br>Cookies visible in DOM:");
    shouldHaveDOMCookieWithValue("strict", "20");
    shouldHaveDOMCookieWithValue("implicit-strict", "20");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "20");
    shouldHaveDOMCookieWithValue("lax", "20");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>
