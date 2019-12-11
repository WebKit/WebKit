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

description("Tests that a SameSite Lax cookie for 127.0.0.1 is sent with a redirect from a page with a different origin.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldHaveCookieWithValue("implicit-strict", "19");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "19");
    await shouldHaveCookieWithValue("lax", "19");

    debug("<br>Cookies visible in DOM:");
    shouldNotHaveDOMCookie("strict");
    shouldHaveDOMCookieWithValue("implicit-strict", "19");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "19");
    shouldHaveDOMCookieWithValue("lax", "19");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>
