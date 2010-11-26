function done()
{
    isSuccessfullyParsed();
    if (window.layoutTestController)
        layoutTestController.notifyDone()
}

function verifyEventCommon(event)
{
    shouldBeTrue("'source' in event");
    shouldBeTrue("event.source != null");
    shouldBeTrue("'onsuccess' in event.target");
    shouldBeTrue("'onerror' in event.target");
    shouldBeTrue("'readyState' in event.target");
    shouldBe("event.target.readyState", "event.target.DONE");
    debug("");
}

function verifyErrorEvent(event)
{
    debug("Error event fired:");
    shouldBeFalse("'result' in event");
    shouldBeTrue("'code' in event");
    shouldBeTrue("'message' in event");
    verifyEventCommon(event);
}

function verifySuccessEvent(event)
{
    debug("Success event fired:");
    shouldBeTrue("'result' in event");
    shouldBeFalse("'code' in event");
    shouldBeFalse("'message' in event");
    verifyEventCommon(event);
}

function verifyAbortEvent(event)
{
    debug("Abort event fired:");
    shouldBeEqualToString("event.type", "abort");
}

function verifyCompleteEvent(event)
{
    debug("Complete event fired:");
    shouldBeEqualToString("event.type", "complete");
}

function verifyResult(result)
{
    shouldBeTrue("'onsuccess' in result");
    shouldBeTrue("'onerror' in result");
    shouldBeTrue("'readyState' in result");
    debug("An event should fire shortly...");
    debug("");
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
    debug("");
    verifySuccessEvent(event);
    done();
}

function unexpectedErrorCallback()
{
    testFailed("Error function called unexpectedly: (" + event.code + ") " + event.message);
    debug("");
    verifyErrorEvent(event);
    done();
}

function unexpectedAbortCallback()
{
    testFailed("Abort function called unexpectedly!");
    debug("");
    verifyAbortEvent(event);
    done();
}

function unexpectedCompleteCallback()
{
    testFailed("oncomplete function called unexpectedly!");
    debug("");
    verifyCompleteEvent(event);
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
        shouldBe("code", expected);
    }
}

// FIXME: remove the onfinished parameter.
function deleteAllObjectStores(db, onfinished)
{
    while (db.objectStoreNames.length)
        db.deleteObjectStore(db.objectStoreNames.item(0));

    debug("Deleted all object stores.");
    onfinished();
}
