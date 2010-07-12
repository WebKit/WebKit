description("Test IndexedDB's IDBObjectStoreRequest.");
if (window.layoutTestController) 
    layoutTestController.waitUntilDone();

function test()
{
    result = evalAndLog("indexedDB.open('name', 'description')");
    verifyResult(result);
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    verifySuccessEvent(event);
    var db = evalAndLog("db = event.result");

    // FIXME: remove any previously created object stores.
    // This requires IDBDatabaseRequest::removeObjectStore to be implemented.

    result = evalAndLog("db.createObjectStore('storeName', null)");
    verifyResult(result);
    result.onsuccess = createSuccess;
    result.onerror = unexpectedErrorCallback;
}

function createSuccess()
{
    debug("createSuccess():");
    verifySuccessEvent(event);
    var store = evalAndLog("store = event.result");

    shouldBeEqualToString("store.name", "storeName");
    shouldBeNull("store.keyPath");
    // FIXME: test store.indexNames, as well as all object store's methods.

    result = evalAndLog("store.add('value', 'key')");
    verifyResult(result);
    result.onsuccess = addSuccess;
    result.onerror = unexpectedErrorCallback;
}

function addSuccess()
{
    debug("addSuccess():");
    verifySuccessEvent(event);
    shouldBeEqualToString("event.result", "key");
    var store = evalAndLog("store = event.source");

    result = evalAndLog("store.get('key')");
    verifyResult(result);
    result.onsuccess = getSuccess;
    result.onerror = unexpectedErrorCallback;
}

function getSuccess()
{
    debug("getSuccess():");
    verifySuccessEvent(event);
    shouldBeEqualToString("event.result", "value");
    var store = evalAndLog("store = event.source");

    result = evalAndLog("store.remove('key')");
    verifyResult(result);
    result.onsuccess = removeSuccess;
    result.onerror = unexpectedErrorCallback;
}

function removeSuccess()
{
    debug("removeSuccess():");
    verifySuccessEvent(event);
    shouldBeNull("event.result");
    done();
}

test();
