if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that aborted VERSION_CHANGE transactions are completely rolled back");

function test() {
    removeVendorPrefixes();
    openDBConnection();
}

function openDBConnection()
{
    request = evalAndLog("indexedDB.open('version-change-abort')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    self.db = evalAndLog("db = event.target.result");
    debug("");

    evalAndLog("vcreq = db.setVersion('version 1')");
    vcreq.onsuccess = inSetVersion1;
    vcreq.onerror = unexpectedErrorCallback;
}

function inSetVersion1()
{
    debug("setVersion1() callback");
    shouldBeTrue("vcreq.result instanceof IDBTransaction");
    trans = vcreq.result;
    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = setVersion1Complete;

    evalAndLog("store = db.createObjectStore('store1')");
}

function setVersion1Complete()
{
    debug("setVersion1 complete");
    shouldBeEqualToString("db.version", "version 1");
    debug("");

    evalAndLog("vcreq = db.setVersion('version 2')");
    vcreq.onsuccess = inSetVersion2;
    vcreq.onerror = unexpectedErrorCallback;
}

function inSetVersion2()
{
    debug("setVersion2() callback");
    shouldBeEqualToString("db.version", "version 2");
    shouldBeTrue("vcreq.result instanceof IDBTransaction");
    trans = vcreq.result;
    trans.onabort = setVersion2Abort;
    trans.onerror = unexpectedErrorCallback;
    trans.oncomplete = unexpectedCompleteCallback;

    evalAndLog("store = db.deleteObjectStore('store1')");
    evalAndLog("store = db.createObjectStore('store2')");

    // Ensure the test harness error handler is not invoked.
    self.originalWindowOnError = self.onerror;
    self.onerror = null;

    debug("raising exception");
    throw new Error("This should *NOT* be caught!");
}

function setVersion2Abort()
{
    debug("");
    debug("setVersion2Abort() callback");

    // Restore test harness error handler.
    self.onerror = self.originalWindowOnError;

    shouldBeEqualToString("db.version", "version 1");
    shouldBeTrue("db.objectStoreNames.contains('store1')");
    shouldBeFalse("db.objectStoreNames.contains('store2')");

    finishJSTest();
}

test();