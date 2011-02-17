function done()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone()
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
}

function unexpectedErrorCallback()
{
    testFailed("Error function called unexpectedly: (" + event.target.errorCode + ") " + event.target.webkitErrorMessage);
}

function unexpectedAbortCallback()
{
    testFailed("Abort function called unexpectedly!");
}

function unexpectedCompleteCallback()
{
    testFailed("oncomplete function called unexpectedly!");
}

function unexpectedBlockedCallback()
{
    testFailed("onblocked called unexpectedly");
}

function evalAndExpectException(cmd, expected)
{
    debug("Expecting exception from " + cmd);
    try {
        eval(cmd);
        testFailed("No exception thrown! Should have been " + expected);
    } catch (e) {
        code = e.code;
        testPassed("Exception was thrown.");
        shouldBe("code", expected);
    }
}

function deleteAllObjectStores(db)
{
    while (db.objectStoreNames.length)
        db.deleteObjectStore(db.objectStoreNames.item(0));
    debug("Deleted all object stores.");
}
