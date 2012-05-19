if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's webkitIDBObjectStore.clear().");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('objectstore-clear')");
    request.onsuccess = startSetVersion;
    request.onerror = unexpectedErrorCallback;
}

function startSetVersion()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteExisting;
    request.onerror = unexpectedErrorCallback;
}

function deleteExisting()
{
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeNonNull("trans");

    deleteAllObjectStores(db);

    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    self.index = evalAndLog("store.createIndex('indexName', '')");
    shouldBeTrue("store.indexNames.contains('indexName')");

    request = evalAndLog("store.add('value', 'key')");
    request.onsuccess = createSecondObjectStoreAndAddValue;
    request.onerror = unexpectedErrorCallback;
}

function createSecondObjectStoreAndAddValue()
{
    otherStore = evalAndLog("otherStore = db.createObjectStore('otherStoreName', null)");

    request = evalAndLog("otherStore.add('value', 'key')");
    request.onsuccess = clearObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function clearObjectStore()
{
    request = evalAndLog("store.clear()");
    request.onsuccess = clearSuccess;
    request.onerror = unexpectedErrorCallback;
}

function clearSuccess()
{
    shouldBeUndefined("event.target.result");

    request = evalAndLog("store.openCursor()");
    request.onsuccess = openCursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openCursorSuccess()
{
    shouldBeNull("event.target.result");

    index = evalAndLog("index = store.index('indexName')");
    request = evalAndLog("index.openKeyCursor()");
    request.onsuccess = openKeyCursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openKeyCursorSuccess()
{
    debug("openKeyCursorSuccess():");
    shouldBeNull("event.target.result");
    trans.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    transaction = evalAndLog("db.transaction(['otherStoreName'])");
    transaction.onabort = unexpectedErrorCallback;
    var otherStore = evalAndLog("otherStore = transaction.objectStore('otherStoreName')");

    request = evalAndLog("otherStore.get('key')");
    request.onsuccess = getSuccess;
    request.onerror = unexpectedErrorCallback;
}

function getSuccess()
{
    shouldBeEqualToString("event.target.result", "value");

    finishJSTest();
}

test();