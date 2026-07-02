if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test createIndex failing due to a ConstraintError");

indexedDBTest(prepareDatabase);
function prepareDatabase(event)
{
    trans = event.target.transaction;
    trans.onabort = abortCallback;
    db = event.target.result;

    db.createObjectStore("objectStore");
    objectStore = trans.objectStore("objectStore");
    var req1 = objectStore.put({"key": "value"}, "object1");
    var req2 = objectStore.put({"key": "value"}, "object2");

    waitForRequests([req1, req2], createIndex);
}

function createIndex() {
    // This will asynchronously abort in the backend because of constraint failures.
    evalAndLog("objectStore.createIndex('index', 'key', {unique: true})");
    // Now immediately delete it.
    evalAndLog("objectStore.deleteIndex('index')");
    // Delete it again: backend may have asynchronously aborted the
    // index creation, but this makes sure the frontend doesn't get
    // confused and crash, or think the index still exists.
    evalAndExpectException("objectStore.deleteIndex('index')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    debug("Now requesting object2");
    
    // None of the following 3 lines will actually do anything because the call to createIndex will forcefully abort the transaction.
    var req3 = objectStore.get("object2");
    req3.onsuccess = unexpectedSuccessCallback;
    req3.onerror = errorCallback;
    
    debug("now we wait.");
}

function errorCallback() {
    debug("Error function called: (" + event.target.error.name + ") " + event.target.error.message);
}

function abortCallback() {
    testPassed("Abort function called: (" + event.target.error.name + ") " + event.target.error.message);
    finishJSTest();
}
