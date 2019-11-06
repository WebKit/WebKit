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

description("Tests that a SameSite Lax cookie for 127.0.0.1 is sent with a top-level navigation initiated from a frame embedded in a page with a different origin.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldHaveCookieWithValue("implicit-strict", "4");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "4");
    await shouldHaveCookieWithValue("lax", "4");

    debug("<br>Cookies visible in DOM:");
    shouldNotHaveDOMCookie("strict");
    shouldHaveDOMCookieWithValue("implicit-strict", "4");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "4");
    shouldHaveDOMCookieWithValue("lax", "4");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>
