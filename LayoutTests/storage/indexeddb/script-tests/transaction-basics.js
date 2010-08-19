description("Test IndexedDB transaction basics.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function abortCallback()
{
    verifyAbortEvent(event);
    done();
}

function createTransactionCallback()
{
    verifySuccessEvent(event);
    transaction = evalAndLog("event.result.transaction()");
    transaction.onabort = abortCallback;
}

function test()
{
    shouldBeTrue("'indexedDB' in window");
    shouldBeFalse("indexedDB == null");

    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = createTransactionCallback;
    result.onerror = unexpectedErrorCallback;
}

test();

var successfullyParsed = true;
