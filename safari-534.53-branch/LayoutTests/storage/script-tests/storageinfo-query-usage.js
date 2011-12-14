description("This tests webkitStorageInfo API works.");

function errorCallback(error)
{
    testFailed("Error occurred: " + error);
    finishJSTest();
}

var returnedUsage;
var returnedQuota;
function usageCallback(usage, quota)
{
    returnedUsage = usage;
    returnedQuota = quota;

    // Usage should be 0 (if other storage tests have correctly cleaned up their test data before exiting).
    shouldBe("returnedUsage", "0");

    // Quota value would vary depending on the test environment.
    shouldBeGreaterThanOrEqual("returnedQuota", "returnedUsage");

    finishJSTest();
}

if (window.webkitStorageInfo) {
    window.jsTestIsAsync = true;
    webkitStorageInfo.queryUsageAndQuota(webkitStorageInfo.TEMPORARY, usageCallback, errorCallback);
} else
    debug("This test requires window.webkitStorageInfo.");

window.successfullyParsed = true;
