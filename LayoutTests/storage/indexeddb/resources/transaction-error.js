if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IDBTransaction.error cases.");

indexedDBTest(prepareDatabase, startTest);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("store = db.createObjectStore('storeName')");
    request = evalAndLog("store.add('value', 'key')");
    request.onerror = unexpectedErrorCallback;
}

function startTest()
{
    debug("");
    evalAndLog("trans = db.transaction('storeName')");

    debug("");
    debug("IDBTransaction.error should be null if transaction is not finished:");
    shouldBeNull("trans.error");

    debug("");
    debug("If IDBTransaction.abort() is explicitly called, IDBTransaction.error should be null:");
    evalAndLog("trans.abort()");
    trans.oncomplete = unexpectedCompleteCallback;
    trans.onabort = function() {
        shouldBeNull("trans.error");
        testErrorFromRequest();
    };
}

function testErrorFromRequest()
{
    debug("");
    debug("If the transaction is aborted due to a request error that is not prevented, IDBTransaction.error should match:");
    evalAndLog("trans = db.transaction('storeName', 'readwrite')");
    evalAndLog("request = trans.objectStore('storeName').add('value2', 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = function() {
        shouldBe("request.error.name", "'ConstraintError'");
        evalAndLog("request_error = request.error");
    };
    trans.oncomplete = unexpectedCompleteCallback;
    trans.onabort = function() {
        debug("Transaction received abort event.");
        shouldBeNonNull("trans.error");
        debug("trans.webkitErrorMessage = " + trans.webkitErrorMessage);
        shouldBeNonNull("trans.webkitErrorMessage");
        shouldBe("trans.error", "request_error");
        testErrorFromException();
    };
}

function testErrorFromException()
{
    debug("");
    debug("If the transaction is aborted due to an exception thrown from event callback, IDBTransaction.error should be AbortError:");
    evalAndLog("trans = db.transaction('storeName', 'readwrite')");
    evalAndLog("request = trans.objectStore('storeName').add('value2', 'key')");
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = function() {
        shouldBe("request.error.name", "'ConstraintError'");
        debug("Throwing exception...");

        // Ensure the test harness error handler is not invoked.
        self.originalWindowOnError = self.onerror;
        self.onerror = null;

        throw new Error("This should *NOT* be caught!");
    };
    trans.oncomplete = unexpectedCompleteCallback;
    trans.onabort = function() {
        debug("Transaction received abort event.");

        // Restore test harness error handler.
        self.onerror = self.originalWindowOnError;

        shouldBeNonNull("trans.error");
        debug("trans.webkitErrorMessage = " + trans.webkitErrorMessage);
        shouldBeNonNull("trans.webkitErrorMessage");
        shouldBe("trans.error.name", "'AbortError'");
        testErrorFromCommit();
    };
}

function testErrorFromCommit()
{
    debug("");
    debug("If the transaction is aborted due to an error during commit, IDBTransaction.error should reflect that error:");
    evalAndLog("trans = db.transaction('storeName', 'readwrite')");
    evalAndLog("request = trans.objectStore('storeName').add({id: 1}, 'record1')");
    request.onerror = unexpectedErrorCallback;
    evalAndLog("request = trans.objectStore('storeName').add({id: 1}, 'record2')");
    request.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = function() {
        db.close();
        evalAndLog("request = indexedDB.open(dbname, 2)");
        request.onsuccess = unexpectedSuccessCallback;
        request.onblocked = unexpectedBlockedCallback;
        request.onupgradeneeded = function() {
            evalAndLog("trans = request.transaction");
            debug("This should fail due to the unique constraint:");
            evalAndLog("trans.objectStore('storeName').createIndex('indexName', 'id', {unique: true})");
            trans.oncomplete = unexpectedCompleteCallback;
            trans.onabort = function() {
                debug("Transaction received abort event.");
                shouldBeNonNull("trans.error");
                shouldBe("trans.error.name", "'ConstraintError'");
                debug("trans.webkitErrorMessage = " + trans.webkitErrorMessage);
                shouldBeNonNull("trans.webkitErrorMessage");
                debug("");
                finishJSTest();
            };
        };
    };
}
