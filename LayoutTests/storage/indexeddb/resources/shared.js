var jsTestIsAsync = true;
if (self.importScripts && !self.postMessage) {
    // Shared worker.  Make postMessage send to the newest client, which in
    // our tests is the only client.

    // Store messages for sending until we have somewhere to send them.
    self.postMessage = function(message)
    {
        if (typeof self.pendingMessages === "undefined")
            self.pendingMessages = [];
        self.pendingMessages.push(message);
    };
    self.onconnect = function(event)
    {
        self.postMessage = function(message)
        {
            event.ports[0].postMessage(message);
        };
        // Offload any stored messages now that someone has connected to us.
        if (typeof self.pendingMessages === "undefined")
            return;
        while (self.pendingMessages.length)
            event.ports[0].postMessage(self.pendingMessages.shift());
    };
}

function removeVendorPrefixes()
{
    IDBCursor = self.IDBCursor || self.webkitIDBCursor;
    IDBDatabase = self.IDBDatabase || self.webkitIDBDatabase;
    IDBDatabaseException = self.IDBDatabaseException || self.webkitIDBDatabaseException;
    IDBFactory = self.IDBFactory || self.webkitIDBFactory;
    IDBIndex = self.IDBIndex || self.webkitIDBIndex;
    IDBKeyRange = self.IDBKeyRange || self.webkitIDBKeyRange;
    IDBObjectStore = self.IDBObjectStore || self.webkitIDBObjectStore;
    IDBRequest = self.IDBRequest || self.webkitIDBRequest;
    IDBTransaction = self.IDBTransaction || self.webkitIDBTransaction;

    indexedDB = evalAndLog("indexedDB = self.indexedDB || self.webkitIndexedDB || self.mozIndexedDB || self.msIndexedDB || self.OIndexedDB;");
    debug("");
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
    finishJSTest();
}

function unexpectedErrorCallback(event)
{
    testFailed("Error function called unexpectedly: (" + event.target.error.name + ") " + event.target.webkitErrorMessage);
    finishJSTest();
}

function unexpectedAbortCallback(e)
{
    testFailed("Abort function called unexpectedly!");
    finishJSTest();
}

function unexpectedCompleteCallback()
{
    testFailed("oncomplete function called unexpectedly!");
    finishJSTest();
}

function unexpectedBlockedCallback()
{
    testFailed("onblocked called unexpectedly");
    finishJSTest();
}

function unexpectedUpgradeNeededCallback()
{
    testFailed("onupgradeneeded called unexpectedly");
    finishJSTest();
}

function unexpectedVersionChangeCallback()
{
    testFailed("onversionchange called unexpectedly");
    finishJSTest();
}

function evalAndExpectException(cmd, exceptionCode, exceptionName, _quiet)
{
    if (!_quiet)
        debug("Expecting exception from " + cmd);
    try {
        eval(cmd);
        testFailed("No exception thrown! Should have been " + exceptionCode);
    } catch (e) {
        code = e.code;
        if (!_quiet)
            testPassed("Exception was thrown.");
        shouldBe("code", exceptionCode, _quiet);
        if (exceptionName) {
            ename = e.name;
            shouldBe("ename", exceptionName, _quiet);
        }
    }
}

function evalAndExpectExceptionClass(cmd, expected)
{
    debug("Expecting " + expected + " exception from " + cmd);
    try {
        eval(cmd);
        testFailed("No exception thrown!" );
    } catch (e) {
        testPassed("Exception was thrown.");
        if (eval("e instanceof " + expected))
            testPassed(cmd + " threw " + e);
        else
            testFailed("Expected " + expected + " but saw " + e);
    }
}

function evalAndLogCallback(cmd) {
  function callback() {
    evalAndLog(cmd);
  }
  return callback;
}

function deleteAllObjectStores(db)
{
    while (db.objectStoreNames.length)
        db.deleteObjectStore(db.objectStoreNames.item(0));
    debug("Deleted all object stores.");
}

function setDBNameFromPath() {
    evalAndLog('dbname = "' + self.location.pathname.substring(1 + self.location.pathname.lastIndexOf("/")) + '"');
}

function preamble(evt)
{
    if (evt)
        event = evt;
    debug("");
    debug(preamble.caller.name + "():");
}

// For Workers
if (!self.DOMException) {
    self.DOMException = {
        INDEX_SIZE_ERR: 1,
        DOMSTRING_SIZE_ERR: 2,
        HIERARCHY_REQUEST_ERR: 3,
        WRONG_DOCUMENT_ERR: 4,
        INVALID_CHARACTER_ERR: 5,
        NO_DATA_ALLOWED_ERR: 6,
        NO_MODIFICATION_ALLOWED_ERR: 7,
        NOT_FOUND_ERR: 8,
        NOT_SUPPORTED_ERR: 9,
        INUSE_ATTRIBUTE_ERR: 10,
        INVALID_STATE_ERR: 11,
        SYNTAX_ERR: 12,
        INVALID_MODIFICATION_ERR: 13,
        NAMESPACE_ERR: 14,
        INVALID_ACCESS_ERR: 15,
        VALIDATION_ERR: 16,
        TYPE_MISMATCH_ERR: 17,
        SECURITY_ERR: 18,
        NETWORK_ERR: 19,
        ABORT_ERR: 20,
        URL_MISMATCH_ERR: 21,
        QUOTA_EXCEEDED_ERR: 22,
        TIMEOUT_ERR: 23,
        INVALID_NODE_TYPE_ERR: 24,
        DATA_CLONE_ERR: 25
    };
}
