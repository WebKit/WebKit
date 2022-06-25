if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure IndexedDB's IDBObjectStore.deleteIndex() works if IDBIndex object has not been fetched - regression test for bug 110792.");

indexedDBTest(onFirstUpgradeNeeded, closeAndReOpen, {version: 1});

function onFirstUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.createIndex('index', 'keyPath')");
}

function closeAndReOpen()
{
    preamble();
    evalAndLog("db.close()");
    debug("");
    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = onSecondUpgradeNeeded;
    request.onsuccess = finishJSTest;
}

function onSecondUpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = event.target.transaction.objectStore('store')");
    // Do NOT add a call to store.index('index') here (e.g. to assert it exists)
    // or the bug disappears.
    evalAndLog("store.deleteIndex('index')");
    evalAndExpectException("store.index('index')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
}
