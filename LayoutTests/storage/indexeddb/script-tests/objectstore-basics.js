description("Test the basics of IndexedDB's IDBObjectStore.");
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
    db = evalAndLog("db = event.result");

    deleteAllObjectStores(db);

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
    var storeNames = evalAndLog("storeNames = db.objectStores");

    shouldBeEqualToString("store.name", "storeName");
    shouldBeNull("store.keyPath");
    shouldBe("storeNames.contains('storeName')", "true");
    shouldBe("storeNames.length", "1");
    // FIXME: test all of object store's methods.

    result = evalAndLog("event.result.createIndex('indexName', 'x')");
    verifyResult(result);
    result.onsuccess = addIndexSuccess;
    result.onerror = unexpectedErrorCallback;
}

function addIndexSuccess()
{
    debug("addIndexSuccess():");
    verifySuccessEvent(event);
    shouldBeTrue("event.source.indexNames.contains('indexName')");

    result = evalAndLog("event.source.add({x: 'value'}, 'key')");
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
    shouldBeEqualToString("event.result.x", "value");
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

var successfullyParsed = true;
