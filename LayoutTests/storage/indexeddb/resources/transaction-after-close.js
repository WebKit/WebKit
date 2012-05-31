if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test closing a database connection in IndexedDB.");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('transaction-after-close')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    self.db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('version 1')");
    request.onsuccess = inSetVersion;
    request.onerror = unexpectedErrorCallback;
}

function inSetVersion()
{
    deleteAllObjectStores(db);

    event.target.result.oncomplete = runFirstRegularTransaction;
    event.target.result.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = db.createObjectStore('store')");
    request = evalAndLog("request = store.put('x', 'y')");
    request.onsuccess = onPutSuccess;
    request.onerror = unexpectedErrorCallback;
}

function onPutSuccess()
{
    testPassed("Put success")
}

function runFirstRegularTransaction()
{
    debug("running first transaction")
    currentTransaction = evalAndLog("currentTransaction = db.transaction(['store'], 'readwrite')");
    currentTransaction.onabort = unexpectedAbortCallback;
    currentTransaction.oncomplete = firstTransactionComplete;
    objectStore = currentTransaction.objectStore('store');
    request = evalAndLog("objectStore.put('a', 'b')");
    request.onerror = unexpectedErrorCallback;
}

function firstTransactionComplete()
{
    evalAndLog("db.close()");
    evalAndExpectException("db.transaction(['store'], 'readwrite')", "IDBDatabaseException.NOT_ALLOWED_ERR");

    debug("")
    debug("verify that we can reopen the db after calling close")
    request = evalAndLog("indexedDB.open('transaction-after-close')");
    request.onsuccess = onSecondOpen
    request.onerror = unexpectedErrorCallback;
}

function onSecondOpen() {
    second_db = evalAndLog("second_db = event.target.result");
    currentTransaction = evalAndLog("currentTransaction = second_db.transaction(['store'], 'readwrite')");
    store = currentTransaction.objectStore('store');
    request = evalAndLog("request = store.put('1', '2')");
    request.onsuccess = onFinalPutSuccess;
    request.onerror = unexpectedErrorCallback;
    currentTransaction.oncomplete = finishJSTest;
    currentTransaction.onabort = unexpectedAbortCallback;
}

function onFinalPutSuccess() {
    testPassed("final put success");
}

test();
