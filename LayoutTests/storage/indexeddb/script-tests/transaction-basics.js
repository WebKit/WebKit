description("Test IndexedDB transaction basics.");
if (window.layoutTestController)
    layoutTestController.waitUntilDone();

function test()
{
    shouldBeTrue("'indexedDB' in window");
    shouldBeFalse("indexedDB == null");

    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("createObjectStoreCallback():");
    verifySuccessEvent(event);
    db = evalAndLog("db = event.result");

    deleteAllObjectStores(db);

    result = evalAndLog("db.createObjectStore('storeName', null)");
    verifyResult(result);
    result.onsuccess = createSuccess;
    result.onerror = unexpectedErrorCallback;
}

function createSuccess()
{
    verifySuccessEvent(event);
    transaction = evalAndLog("db.transaction()");
    transaction.onabort = abortCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");
    shouldBeEqualToString("store.name", "storeName");
}

function abortCallback()
{
    verifyAbortEvent(event);
    done();
}

test();

var successfullyParsed = true;
