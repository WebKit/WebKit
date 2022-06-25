if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's webkitIDBObjectStore.clear().");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

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
