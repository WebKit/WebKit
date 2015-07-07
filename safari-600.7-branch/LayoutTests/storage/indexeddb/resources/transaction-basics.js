if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB transaction basics.");

indexedDBTest(prepareDatabase, testSetVersionAbort1);
function prepareDatabase()
{
    db = event.target.result;
}

function testSetVersionAbort1()
{
    checkMetadataEmpty();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = addRemoveIDBObjects;
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = testSetVersionAbort2;
}

function addRemoveIDBObjects()
{
    debug("addRemoveIDBObjects():");
    db = event.target.result;
    evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.oncomplete = unexpectedCompleteCallback;

    var store = evalAndLog("store = db.createObjectStore('storeFail', null)");
    var index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    evalAndLog("db.deleteObjectStore('storeFail')");
    evalAndExpectException("store.deleteIndex('indexFail')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    trans.abort();
}

function testSetVersionAbort2()
{
    debug("");
    debug("testSetVersionAbort2():");
    checkMetadataEmpty();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = addRemoveAddIDBObjects;
    request.onerror = null;
}

function addRemoveAddIDBObjects()
{
    debug("addRemoveAddIDBObjects():");
    db = event.target.result;
    var trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.addEventListener('abort', testSetVersionAbort3, false);
    trans.oncomplete = unexpectedCompleteCallback;

    var store = evalAndLog("store = db.createObjectStore('storeFail', null)");
    var index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    evalAndLog("db.deleteObjectStore('storeFail')");
    evalAndExpectException("store.deleteIndex('indexFail')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    var store = evalAndLog("store = db.createObjectStore('storeFail', null)");
    var index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    trans.abort();
}

function testSetVersionAbort3()
{
    debug("");
    debug("testSetVersionAbort3():");
    shouldBeFalse("event.cancelable");
    checkMetadataEmpty();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = addIDBObjects;
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = testSetVersionAbort4;
}

function addIDBObjects()
{
    debug("addIDBObjects():");
    db = event.target.result;
    shouldBeFalse("event.cancelable");
    var trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = testInactiveAbortedTransaction;
    trans.oncomplete = unexpectedCompleteCallback;

    store = evalAndLog("store = db.createObjectStore('storeFail', null)");
    index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    trans.abort();
}

function testInactiveAbortedTransaction()
{
    debug("");
    debug("testInactiveAbortedTransaction():");
    evalAndExpectException("index.openCursor()", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.openKeyCursor()", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.get(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.getKey(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.count()", "0", "'TransactionInactiveError'");

    evalAndExpectException("store.put(0, 0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.add(0, 0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.delete(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.clear()", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.openCursor()", "0", "'TransactionInactiveError'");
}

function testSetVersionAbort4()
{
    debug("");
    debug("testSetVersionAbort4():");
    checkMetadataEmpty();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = addIDBObjectsAndCommit;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = testSetVersionAbort5;
}

function addIDBObjectsAndCommit()
{
    db = event.target.result;
    debug("addIDBObjectsAndCommit():");
    var trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    store = evalAndLog("store = db.createObjectStore('storeFail', null)");
    index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    trans.oncomplete = testInactiveCompletedTransaction;
}

function testInactiveCompletedTransaction()
{
    debug("");
    debug("testInactiveCompletedTransaction():");
    evalAndExpectException("index.openCursor()", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.openKeyCursor()", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.get(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.getKey(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("index.count()", "0", "'TransactionInactiveError'");

    evalAndExpectException("store.put(0, 0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.add(0, 0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.delete(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.clear()", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.get(0)", "0", "'TransactionInactiveError'");
    evalAndExpectException("store.openCursor()", "0", "'TransactionInactiveError'");
}

function testSetVersionAbort5()
{
    debug("");
    debug("testSetVersionAbort5():");
    checkMetadataExistingObjectStore();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = removeIDBObjects;
    request.onsuccess = unexpectedSuccessCallback;
    request.onerror = testSetVersionAbort6;
}

function removeIDBObjects()
{
    db = event.target.result;
    debug("removeIDBObjects():");
    var trans = evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.oncomplete = unexpectedCompleteCallback;

    var store = evalAndLog("store = trans.objectStore('storeFail')");
    evalAndLog("store.deleteIndex('indexFail')");
    evalAndLog("db.deleteObjectStore('storeFail')");

    trans.abort();
}

function testSetVersionAbort6()
{
    debug("");
    debug("testSetVersionAbort6():");
    checkMetadataExistingObjectStore();
    evalAndLog("request = newConnection()");
    request.onupgradeneeded = setVersionSuccess;
    request.onsuccess = completeCallback;
}

function checkMetadataEmpty()
{
    shouldBe("self.db.objectStoreNames", "[]");
    shouldBe("self.db.objectStoreNames.length", "0");
    shouldBe("self.db.objectStoreNames.contains('storeFail')", "false");
}

function checkMetadataExistingObjectStore()
{
    shouldBe("db.objectStoreNames", "['storeFail']");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBe("db.objectStoreNames.contains('storeFail')", "true");
}

var version = 1;
function newConnection()
{
    db.close();
    var request = evalAndLog("indexedDB.open(dbname, " + (++version) + ")");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    return request;
}

function setVersionSuccess()
{
    db = event.target.result;
    debug("");
    debug("setVersionSuccess():");
    evalAndLog("trans = event.target.transaction");
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

    evalAndLog("db.createObjectStore('storeName', null)");
}

function completeCallback()
{
    preamble();
    shouldBeFalse("event.cancelable");
    testPassed("complete event fired");
    transaction = evalAndLog("db.transaction(['storeName'])");
    transaction.oncomplete = emptyCompleteCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");
    shouldBeEqualToString("store.name", "storeName");
}

function emptyCompleteCallback()
{
    testPassed("complete event fired");
    testDOMStringList();
}

function testDOMStringList()
{
    debug("");
    debug("Verifying DOMStringList works as argument for IDBDatabase.transaction()");
    debug("db.objectStoreNames is " + db.objectStoreNames);
    debug("... which contains: " + JSON.stringify(Array.prototype.slice.call(db.objectStoreNames)));
    evalAndLog("transaction = db.transaction(db.objectStoreNames)");
    testPassed("no exception thrown");
    for (var i = 0; i < db.objectStoreNames.length; ++i) {
      shouldBeNonNull("transaction.objectStore(" + JSON.stringify(db.objectStoreNames[i]) + ")");
    }
    testPassed("all stores present in transaction");
    transaction.oncomplete = testInvalidMode;
}

function testInvalidMode()
{
    debug("");
    debug("Verify that specifying an invalid mode raises an exception");
    evalAndExpectExceptionClass("db.transaction(['storeName'], 'lsakjdf')", "TypeError");
    testDegenerateNames();
}

function testDegenerateNames()
{
    debug("");
    debug("Test that null and undefined are treated as strings");

    evalAndExpectException("db.transaction(null)", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    evalAndExpectException("db.transaction(undefined)", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");

    evalAndLog("request = newConnection()");
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = function () {
        var trans = request.transaction;
        db = event.target.result;
        evalAndLog("db.createObjectStore('null')");
        evalAndLog("db.createObjectStore('undefined')");
        trans.oncomplete = verifyDegenerateNames;
    };
    function verifyDegenerateNames() {
        shouldNotThrow("transaction = db.transaction(null)");
        shouldBeNonNull("transaction.objectStore('null')");
        shouldNotThrow("transaction = db.transaction(undefined)");
        shouldBeNonNull("transaction.objectStore('undefined')");
        finishJSTest();
    }
}
