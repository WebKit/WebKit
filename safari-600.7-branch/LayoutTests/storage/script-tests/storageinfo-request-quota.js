description("This tests webkitStorageInfo.requestQuota.");

function errorCallback(error)
{
    testFailed("Error occurred: " + error);
    finishJSTest();
}

var grantedQuota;
function quotaCallback(newQuota)
{
    grantedQuota = newQuota;

    // We must be given 0 quota, the same amount as we requested.
    shouldBe("grantedQuota", "0");

    finishJSTest();
}

if (window.webkitStorageInfo) {
    window.jsTestIsAsync = true;

    // Requesting '0' quota for testing (this request must be almost always granted without showing any platform specific notification UI).
    webkitStorageInfo.requestQuota(webkitStorageInfo.TEMPORARY, 0, quotaCallback, errorCallback);
} else
    debug("This test requires window.webkitStorageInfo.");

window.successfullyParsed = true;

