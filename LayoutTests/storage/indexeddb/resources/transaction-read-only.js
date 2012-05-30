if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test read-only transactions in IndexedDB.");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.open('transaction-read-only')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    self.db = evalAndLog("db = event.target.result");
    request = evalAndLog("result = db.setVersion('version 1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
}

function cleanDatabase()
{
    deleteAllObjectStores(db);

    event.target.result.oncomplete = setVersionDone;
    event.target.result.onabort = unexpectedAbortCallback;
    store = evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put('x', 'y')");
}

function setVersionDone()
{
    trans = evalAndLog("trans = db.transaction('store')");
    evalAndExpectException("trans.objectStore('store').put('a', 'b')", "IDBDatabaseException.READ_ONLY_ERR", "'ReadOnlyError'");

    trans = evalAndLog("trans = db.transaction('store')");
    evalAndExpectException("trans.objectStore('store').delete('x')", "IDBDatabaseException.READ_ONLY_ERR", "'ReadOnlyError'");

    trans = evalAndLog("trans = db.transaction('store')");
    cur = evalAndLog("cur = trans.objectStore('store').openCursor()");
    cur.onsuccess = gotCursor;
    cur.onerror = unexpectedErrorCallback;
}

function gotCursor()
{
    shouldBeFalse("!event.target.result");
    evalAndExpectException("event.target.result.delete()", "IDBDatabaseException.READ_ONLY_ERR", "'ReadOnlyError'");

    finishJSTest();
}

test();
