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

description("Tests that Same-Site cookies for 127.0.0.1 are not sent with a frame navigation for a frame embedded in a page with a different origin.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldHaveCookieWithValue("implicit-strict", "6");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "6");
    await shouldNotHaveCookie("lax");

    debug("<br>Cookies visible in DOM:");
    shouldNotHaveDOMCookie("strict");
    shouldHaveDOMCookieWithValue("implicit-strict", "6");
    shouldHaveDOMCookieWithValue("strict-because-invalid-SameSite-value", "6");
    shouldNotHaveDOMCookie("lax");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>
