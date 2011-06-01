description("This tests webkitStorageInfo methods do not crash when they are not given optional callbacks parameters.");

var testFinished = false;
var numberOfExpectedCallbacks = 0;

function callback()
{
    testPassed("storageInfo callback called.");
    --numberOfExpectedCallbacks;
    checkCompleted();
}

function checkCompleted()
{
    if (numberOfExpectedCallbacks == 0 && testFinished)
        finishJSTest();
}

function isValidStorageType(type)
{
    return (type == webkitStorageInfo.TEMPORARY || type == webkitStorageInfo.PERSISTENT);
}

function callStorageInfoMethodsWithFewerParameters(type)
{
    webkitStorageInfo.requestQuota(type, 0);
    webkitStorageInfo.queryUsageAndQuota(type);
    if (isValidStorageType(type))
        ++numberOfExpectedCallbacks;
    webkitStorageInfo.requestQuota(type, 0, callback);
    if (isValidStorageType(type))
        ++numberOfExpectedCallbacks;
    webkitStorageInfo.queryUsageAndQuota(type, callback);
}

if (window.webkitStorageInfo) {
    window.jsTestIsAsync = true;

    // Any of the following attempts should not cause crashing.
    callStorageInfoMethodsWithFewerParameters(webkitStorageInfo.TEMPORARY);
    callStorageInfoMethodsWithFewerParameters(webkitStorageInfo.PERSISTENT);
    callStorageInfoMethodsWithFewerParameters(10);

    testFinished = true;
    checkCompleted();

} else
    debug("This test requires window.webkitStorageInfo.");

window.successfullyParsed = true;

