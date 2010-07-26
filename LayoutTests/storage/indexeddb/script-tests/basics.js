description("Test IndexedDB's basics.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function openCallback()
{
    verifySuccessEvent(event);
    done();
}

function test()
{
    shouldBeTrue("'indexedDB' in window");
    shouldBeFalse("indexedDB == null");

    // FIXME: Verify other IndexedDatabaseRequest constructors, once they're implemented.

    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = openCallback;
    result.onerror = unexpectedErrorCallback;
}

test();

var successfullyParsed = true;
