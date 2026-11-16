importScripts("/js-test-resources/js-test.js");
importScripts("../../resources/cookie-utilities.js");

async function checkResult()
{
    setBaseURLWhenFetchingCookies("http://127.0.0.1:8000");
    await shouldNotHaveCookie("strict");
    await shouldNotHaveCookie("implicit-strict");
    await shouldNotHaveCookie("strict-because-invalid-SameSite-value");
    await shouldNotHaveCookie("lax");
    finishJSTest();
}

checkResult();
