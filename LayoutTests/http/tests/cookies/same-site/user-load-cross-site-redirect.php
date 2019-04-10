<?php
    include_once("../resources/cookie-utilities.php");

    if (hostnameIsEqualToString("127.0.0.1") && empty($_SERVER["QUERY_STRING"])) {
        wkSetCookie("strict", "27", Array("SameSite" => "Strict", "Max-Age" => 100, "path" => "/"));
        wkSetCookie("lax", "27", Array("SameSite" => "Lax", "Max-Age" => 100, "path" => "/"));
        wkSetCookie("normal", "27", Array("Max-Age" => 100, "path" => "/"));
        header("Location: http://localhost:8000/resources/redirect.php?url=" . urlencode("http://127.0.0.1:8000" . $_SERVER["REQUEST_URI"] . "?check-cookies"));
        exit(0);
    }
?>
<!DOCTYPE html>
<html>
<head>
<script src="/js-test-resources/js-test.js"></script>
<script src="../resources/cookie-utilities.js"></script>
<script>_setCachedCookiesJSON('<?php echo json_encode($_COOKIE); ?>')</script>
</head>
<body>
<script>
window.jsTestIsAsync = true;

description("This test is representative of a user that loads a site, via the address bar or Command + clicking a hyperlink, that redirects to a cross-site page that expects its SameSite Lax cookies.");

async function checkResult()
{
    debug("Cookies sent with HTTP request:");
    await shouldNotHaveCookie("strict");
    await shouldHaveCookieWithValue("lax", "27");
    await shouldHaveCookieWithValue("normal", "27");

    debug("<br>Cookies visible in DOM:");
    shouldNotHaveDOMCookie("strict");
    shouldHaveDOMCookieWithValue("lax", "27");
    shouldHaveDOMCookieWithValue("normal", "27");

    await resetCookies();
    finishJSTest();
}

checkResult();
</script>
</body>
</html>

