var jsTestIsAsync = true;

function done()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone()
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
    done();
}

function unexpectedErrorCallback()
{
    testFailed("Error function called unexpectedly: (" + event.target.errorCode + ") " + event.target.webkitErrorMessage);
    done();
}

function unexpectedAbortCallback()
{
    testFailed("Abort function called unexpectedly!");
    done();
}

function unexpectedCompleteCallback()
{
    testFailed("oncomplete function called unexpectedly!");
    done();
}

function unexpectedBlockedCallback()
{
    testFailed("onblocked called unexpectedly");
    done();
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

function evalAndExpectExceptionClass(cmd, expected)
{
    debug("Expecting " + expected + " exception from " + cmd);
    try {
        eval(cmd);
        testFailed("No exception thrown!" );
    } catch (e) {
		testPassed("Exception was thrown.");
		if (eval("e instanceof " + expected))
			testPassed(cmd + " threw " + e);
		else
			testFailed("Expected " + expected + " but saw " + e);
    }
}

function deleteAllObjectStores(db)
{
    while (db.objectStoreNames.length)
        db.deleteObjectStore(db.objectStoreNames.item(0));
    debug("Deleted all object stores.");
}
