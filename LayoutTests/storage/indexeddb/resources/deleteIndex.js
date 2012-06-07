if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB deleteIndex method");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('deleteIndex')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    db = evalAndLog("db = event.target.result");

    request = evalAndLog("request = db.setVersion('1')");
    request.onsuccess = twiddleIndexes;
    request.onerror = unexpectedErrorCallback;
}

function twiddleIndexes()
{
    transaction = evalAndLog("transaction = event.target.result;");
    transaction.onerror = unexpectedErrorCallback;
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = postTwiddling;
    deleteAllObjectStores(db);

    objectStore = evalAndLog("objectStore = db.createObjectStore('foo');");
    evalAndExpectException("objectStore.deleteIndex('first')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    shouldThrow("objectStore.deleteIndex()"); // TypeError: not enough arguments.
    index = evalAndLog("index = objectStore.createIndex('first', 'first');");
    evalAndExpectException("objectStore.deleteIndex('FIRST')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    index = evalAndLog("index = objectStore.createIndex('second', 'second');");
    returnValue = evalAndLog("returnValue = objectStore.deleteIndex('first');");
    shouldBe("returnValue", "undefined");
}

function postTwiddling()
{
    evalAndExpectException("db.createObjectStore('bar');", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("objectStore.deleteIndex('second')", "IDBDatabaseException.TRANSACTION_INACTIVE_ERR", "'TransactionInactiveError'");
    finishJSTest();
}

test();