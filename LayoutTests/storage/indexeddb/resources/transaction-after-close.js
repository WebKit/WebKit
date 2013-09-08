if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test closing a database connection in IndexedDB.");

indexedDBTest(prepareDatabase, runFirstRegularTransaction);
function prepareDatabase()
{
    db = event.target.result;
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
    evalAndExpectException("db.transaction(['store'], 'readwrite')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("")
    debug("verify that we can reopen the db after calling close")
    request = evalAndLog("indexedDB.open(dbname)");
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
