if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Ensure that aborted VERSION_CHANGE transactions are completely rolled back");

indexedDBTest(prepareDatabase, setVersion1Complete);
function prepareDatabase()
{
    db = event.target.result;
    trans = event.target.transaction;
    shouldBeTrue("trans instanceof IDBTransaction");
    trans.onabort = unexpectedAbortCallback;
    trans.onerror = unexpectedErrorCallback;

    evalAndLog("store = db.createObjectStore('store1')");
}

function setVersion1Complete()
{
    debug("setVersion1 complete");
    shouldBe("db.version", "1");
    debug("");
    db.close();

    evalAndLog("vcreq = indexedDB.open(dbname, 2)");
    vcreq.onupgradeneeded = inSetVersion2;
    vcreq.onerror = setVersion2Abort;
    vcreq.onblocked = unexpectedBlockedCallback;
    vcreq.onsuccess = unexpectedSuccessCallback;
}

function inSetVersion2()
{
    db = event.target.result;
    debug("setVersion2() callback");
    shouldBe("db.version", "2");
    shouldBeTrue("vcreq.transaction instanceof IDBTransaction");
    trans = vcreq.result;
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
    db.close();
    evalAndLog("request = indexedDB.open(dbname)");
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function (e) {
        db = event.target.result;
        shouldBe("db.version", "1");
        shouldBeTrue("db.objectStoreNames.contains('store1')");
        shouldBeFalse("db.objectStoreNames.contains('store2')");

        finishJSTest();
    }
}
