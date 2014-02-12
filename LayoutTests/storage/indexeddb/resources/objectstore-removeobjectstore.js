if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's webkitIDBObjectStore.deleteObjectStore().");

indexedDBTest(prepareDatabase, getValue);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = db.createObjectStore('storeName', null)");

    self.index = evalAndLog("store.createIndex('indexName', '')");
    shouldBeTrue("store.indexNames.contains('indexName')");

    request = evalAndLog("store.add('value', 'key')");
    request.onerror = unexpectedErrorCallback;
}

function getValue()
{
    transaction = evalAndLog("db.transaction(['storeName'])");
    transaction.onabort = unexpectedErrorCallback;
    transaction.oncomplete = addIndex;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.onsuccess = checkResult;
    request.onerror = unexpectedErrorCallback;
}

function checkResult()
{
    shouldBeEqualToString("event.target.result", "value");
}

function addIndex()
{
    evalAndLog("db.close()");

    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onupgradeneeded = deleteObjectStore;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = getValueAgain;
    request.onblocked = unexpectedBlockedCallback;
}

function deleteObjectStore()
{
    trans = request.transaction;
    db = request.result;
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("db.deleteObjectStore('storeName')");
    createObjectStoreAgain();
}

function createObjectStoreAgain()
{
    evalAndLog("db.createObjectStore('storeName', null)");
}

function getValueAgain()
{
    transaction = evalAndLog("db.transaction(['storeName'])");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.onsuccess = verifyNotFound;
    request.onerror = unexpectedErrorCallback;
}

function verifyNotFound()
{
    shouldBe("event.target.result", "undefined");
    shouldBeFalse("event.target.source.indexNames.contains('indexName')");

    finishJSTest();
}
