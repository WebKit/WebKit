if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's create and removeObjectStore");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('create-and-remove-object-store')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    debug("openSuccess():");
    self.db = evalAndLog("db = event.target.result");
    testCreateAndRemove();
    request = evalAndLog("result = db.setVersion('version 1')");
    request.onsuccess = cleanDatabase;
    request.onerror = unexpectedErrorCallback;
    testCreateAndRemove();
}

function testCreateAndRemove()
{
    debug("Trying create");
    evalAndExpectException('db.createObjectStore("some os")', "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Trying remove");
    evalAndExpectException('db.deleteObjectStore("some os")', "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
}

function cleanDatabase()
{
    deleteAllObjectStores(db);

    os = evalAndLog("db.createObjectStore('tmp')");
    evalAndExpectException("db.createObjectStore('tmp')", "IDBDatabaseException.CONSTRAINT_ERR", "'ConstraintError'");
    event.target.result.oncomplete = setVersionComplete;
}

function setVersionComplete()
{
    trans = evalAndLog("trans = db.transaction(['tmp'])");
    request = evalAndLog("trans.objectStore('tmp').get(0)");
    request.onsuccess = tryOnceMore;
    request.onerror = unexpectedErrorCallback;
}

function tryOnceMore()
{
    shouldBeUndefined("event.target.result");

    testCreateAndRemove();

    debug("Trying create with store that already exists");
    evalAndExpectException("db.createObjectStore('tmp')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    debug("Trying remove with store that already exists");
    evalAndExpectException("db.deleteObjectStore('tmp')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    finishJSTest();
}

test();