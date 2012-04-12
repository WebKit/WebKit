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
    shouldBeTrueQuiet("Boolean(indexedDB && IDBCursor && IDBDatabase && IDBDatabaseException && IDBFactory && IDBIndex && IDBKeyRange && IDBObjectStore && IDBRequest && IDBTransaction)");
    debug("");
}

function unexpectedSuccessCallback()
{
    testFailed("Success function called unexpectedly.");
    finishJSTest();
}

function unexpectedErrorCallback(event)
{
    testFailed("Error function called unexpectedly: (" + event.target.errorCode + ") " + event.target.webkitErrorMessage);
    finishJSTest();
}

function unexpectedAbortCallback()
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

function evalAndExpectException(cmd, expected)
{
    debug("Expecting exception from " + cmd);
    try {
        eval(cmd);
        testFailed("No exception thrown! Should have been " + expected);
    } catch (e) {
        code = e.code;
        testPassed("Exception was thrown.");
        shouldBe("code", expected);
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
