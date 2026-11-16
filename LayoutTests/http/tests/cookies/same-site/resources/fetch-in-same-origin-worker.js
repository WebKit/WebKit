importScripts("/js-test-resources/js-test.js");
importScripts("../../resources/cookie-utilities.js");

async function checkResult()
{
    await shouldHaveCookieWithValue("strict", "12");
    await shouldHaveCookieWithValue("implicit-strict", "12");
    await shouldHaveCookieWithValue("strict-because-invalid-SameSite-value", "12");
    await shouldHaveCookieWithValue("lax", "12");
    finishJSTest();
}

checkResult();
