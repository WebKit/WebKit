if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
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
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.onsuccess = addIndex;
    request.onerror = unexpectedErrorCallback;
}

function addIndex()
{
    shouldBeEqualToString("event.target.result", "value");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = deleteObjectStore;
    request.onerror = unexpectedErrorCallback;
}

function deleteObjectStore()
{
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("db.deleteObjectStore('storeName')");
    createObjectStoreAgain();
}

function createObjectStoreAgain()
{
    evalAndLog("db.createObjectStore('storeName', null)");
    trans.oncomplete = getValueAgain;
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
