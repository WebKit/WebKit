if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's create and removeObjectStore");

indexedDBTest(prepareDatabase, setVersionComplete);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    os = evalAndLog("db.createObjectStore('tmp')");
    evalAndExpectException("db.createObjectStore('tmp')", "0", "'ConstraintError'");
}

function setVersionComplete()
{
    trans = evalAndLog("trans = db.transaction(['tmp'])");
    request = evalAndLog("trans.objectStore('tmp').get(0)");
    request.onsuccess = tryToCreateAndDelete;
    request.onerror = unexpectedErrorCallback;
}

function tryToCreateAndDelete()
{
    shouldBeUndefined("event.target.result");

    debug("Trying create");
    evalAndExpectException('db.createObjectStore("some os")', "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Trying remove");
    evalAndExpectException('db.deleteObjectStore("some os")', "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    debug("Trying create with store that already exists");
    evalAndExpectException("db.createObjectStore('tmp')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Trying remove with store that already exists");
    evalAndExpectException("db.deleteObjectStore('tmp')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    finishJSTest();
}
