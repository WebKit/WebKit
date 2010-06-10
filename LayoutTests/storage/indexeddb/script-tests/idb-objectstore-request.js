description("Test IndexedDB's IDBObjectStoreRequest.");
if (window.layoutTestController) 
    layoutTestController.waitUntilDone();

function test()
{
    result = indexedDB.open('name', 'description');
    verifyResult(result);
    result.onsuccess = openSuccess;
    result.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    verifySuccessEvent(event);

    var db = evalAndLog("db = event.result");
    createObjectStore(db);
}

function createObjectStore(db)
{
    // FIXME: remove any previously created object stores.
    // This requires IDBDatabaseRequest::removeObjectStore to be implemented.
    result = db.createObjectStore('storeName', 'keyPath');
    verifyResult(result);
    result.onsuccess = createSuccess;
    result.onerror = unexpectedErrorCallback;
}

function createSuccess()
{
    verifySuccessEvent(event);
    var store = evalAndLog("store = event.result");
    shouldBeEqualToString("store.name", "storeName");
    shouldBeEqualToString("store.keyPath", "keyPath");
    // FIXME: test store.indexNames, as well as all object store's methods.

    done();
}

test();
