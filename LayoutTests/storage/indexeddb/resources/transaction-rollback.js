if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB transaction rollback.");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("db.createObjectStore('myObjectStore')");
    shouldBe("db.objectStoreNames.length", "1");
}

function setVersionComplete()
{
    debug("setVersionComplete():");

    self.transaction = evalAndLog("transaction = db.transaction(['myObjectStore'], 'readwrite')");
    transaction.onabort = abortCallback;
    transaction.oncomplete = unexpectedCompleteCallback;

    self.store = evalAndLog("store = transaction.objectStore('myObjectStore')");
    request = evalAndLog("store.add('rollbackValue', 'rollbackKey123')");
    request.onsuccess = addSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addSuccess()
{
    debug("addSuccess():");
    shouldBeEqualToString("event.target.result", "rollbackKey123");

    request = evalAndLog("store.openCursor()");
    request.onsuccess = openCursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openCursorSuccess()
{
    debug("openCursorSuccess():");
    self.cursor = evalAndLog("cursor = event.target.result");

    transaction.abort();
}

function abortCallback()
{
    debug("abortCallback():");
    debug('Transaction was aborted.');

    self.transaction = evalAndLog("transaction = db.transaction(['myObjectStore'], 'readonly')");
    self.store = evalAndLog("store = transaction.objectStore('myObjectStore')");
    request = evalAndLog("store.get('rollbackKey123')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = getSuccess;
}

function getSuccess()
{
    debug("getSuccess():");
    shouldBe("event.target.result", "undefined");

    shouldBeEqualToString("cursor.value", "rollbackValue");
    finishJSTest();
}
