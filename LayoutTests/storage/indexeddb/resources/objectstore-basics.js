if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBObjectStore.");

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('objectstore-basics')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess(evt)
{
    event = evt;
    debug("openSuccess():");
    self.db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function setVersionSuccess(evt)
{
    event = evt;
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

    debug("createObjectStore():");
    self.store = evalAndLog("store = db.createObjectStore('storeName', null)");
    var storeNames = evalAndLog("storeNames = db.objectStoreNames");

    shouldBeTrue("'name' in store");
    shouldBeTrue("'keyPath' in store");
    shouldBeTrue("'indexNames' in store");
    shouldBeTrue("'transaction' in store");
    shouldBeTrue("'put' in store");
    shouldBeEqualToString("typeof store.put", "function");
    shouldBeTrue("'add' in store");
    shouldBeEqualToString("typeof store.add", "function");
    shouldBeTrue("'delete' in store");
    shouldBeEqualToString("typeof store.delete", "function");
    shouldBeTrue("'get' in store");
    shouldBeEqualToString("typeof store.get", "function");
    shouldBeTrue("'clear' in store");
    shouldBeEqualToString("typeof store.clear", "function");
    shouldBeTrue("'openCursor' in store");
    shouldBeEqualToString("typeof store.openCursor", "function");
    shouldBeTrue("'createIndex' in store");
    shouldBeEqualToString("typeof store.createIndex", "function");
    shouldBeTrue("'index' in store");
    shouldBeEqualToString("typeof store.index", "function");
    shouldBeTrue("'deleteIndex' in store");
    shouldBeEqualToString("typeof store.deleteIndex", "function");
    shouldBeTrue("'count' in store");
    shouldBeEqualToString("typeof store.count", "function");

    shouldBeEqualToString("store.name", "storeName");
    shouldBeNull("store.keyPath");
    shouldBe("storeNames.contains('storeName')", "true");
    shouldBe("storeNames.length", "1");
    // FIXME: test all of object store's methods.

    debug("Ask for an index that doesn't exist:");
    try {
        debug("index = store.index('asdf')");
        index = store.index('asdf');
        testFailed("Asking for a store that doesn't exist should have thrown.");
    } catch (err) {
        testPassed("Exception thrown.");
        code = err.code;
        shouldBe("code", "IDBDatabaseException.NOT_FOUND_ERR");
    }

    createIndex();
}

function createIndex()
{
    debug("createIndex():");
    var index = evalAndLog("index = store.createIndex('indexName', 'x', {unique: true})"); // true == unique requirement.
    shouldBeTrue("index !== null");
    shouldBeTrue("store.indexNames.contains('indexName')");
    index = evalAndLog("index = store.index('indexName')");
    shouldBeTrue("index !== null");

    debug("Ask for an index that doesn't exist:");
    try {
        debug("index = store.index('asdf')");
        index = store.index('asdf');
        testFailed("Asking for a store that doesn't exist should have thrown.");
    } catch (err) {
        testPassed("Exception thrown.");
        code = err.code
        shouldBe("code", "IDBDatabaseException.NOT_FOUND_ERR");
    }
    trans.oncomplete = testSetVersionAbort;
}

function testSetVersionAbort()
{
    request = evalAndLog('db.setVersion("version fail")');
    request.onsuccess = createAnotherIndex;
    request.onerror = unexpectedErrorCallback;
}

function createAnotherIndex(evt)
{
    event = evt;
    shouldBeEqualToString("db.version", "version fail");

    var setVersionTrans = evalAndLog("setVersionTrans = event.target.result");
    shouldBeTrue("setVersionTrans !== null");
    setVersionTrans.oncomplete = unexpectedCompleteCallback;
    setVersionTrans.onabort = checkMetadata;
    self.store = evalAndLog("store = setVersionTrans.objectStore('storeName')");
    var index = evalAndLog("index = store.createIndex('indexFail', 'x')");

    setVersionTrans.abort();
}

function checkMetadata()
{
    shouldBeEqualToString("db.version", "new version");
    shouldBe("store.transaction", "setVersionTrans");
    shouldBe("store.indexNames", "['indexName']");
    shouldBe("store.indexNames.length", "1");
    shouldBe("store.indexNames.contains('')", "false");
    shouldBe("store.indexNames.contains('indexFail')", "false");
    shouldBe("store.indexNames.contains('indexName')", "true");
    shouldBeEqualToString("store.indexNames[0]", "indexName");
    shouldBeNull("store.indexNames[1]");
    shouldBeNull("store.indexNames[100]");
    shouldBeNull("store.indexNames.item(1)");
    shouldBeNull("store.indexNames.item(100)");
    addData();
}

var testDate = new Date("August 25, 1991 20:57:08");
var testDateB = new Date("Wed Jan 05 2011 15:54:49");

function addData()
{
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedAbortCallback;
    self.store = evalAndLog("store = transaction.objectStore('storeName')");

    debug("Try to insert data with a Date key:");
    request = evalAndLog("store.add({x: 'foo'}, testDate)");
    request.onsuccess = addDateSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addDateSuccess(evt)
{
    event = evt;
    debug("Try to insert a value not handled by structured clone:");
    try {
        debug("store.add({x: 'bar', y: self}, 'bar')");
        store.add({x: 'bar', y: self}, 'bar');
        testFailed("Passing a DOM node as value should have thrown.");
    } catch (err) {
        testPassed("Exception thrown");
        code = err.code;
        // FIXME: When we move to DOM4-style exceptions this should look for the
        // name "DataCloneError".
        shouldBe("code", "25");
    }

    debug("Try to insert data where key path yields a Date key:");
    request = evalAndLog("store.add({x: testDateB, y: 'value'}, 'key')");
    request.onsuccess = addSuccess;
    request.onerror = unexpectedErrorCallback;
}

function addSuccess(evt)
{
    event = evt;
    debug("addSuccess():");
    shouldBeEqualToString("event.target.result", "key");

    request = evalAndLog("event.target.source.add({x: 'foo'}, 'zzz')");
    request.onsuccess = unexpectedSuccessCallback;
    request.addEventListener('error', addAgainFailure, false);
}

function addAgainFailure(evt)
{
    event = evt;
    debug("addAgainFailure():");
    shouldBe("event.target.errorCode", "IDBDatabaseException.CONSTRAINT_ERR");

    evalAndLog("event.preventDefault()");

    transaction = evalAndLog("db.transaction(['storeName'], IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    evalAndLog("store.add({x: 'somevalue'}, 'somekey')");
    evalAndExpectException("store.add({x: 'othervalue'}, null)", "IDBDatabaseException.DATA_ERR");

    transaction = evalAndLog("db.transaction(['storeName'], IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    evalAndExpectException("store.add({x: null}, 'validkey')", "IDBDatabaseException.DATA_ERR");

    transaction = evalAndLog("db.transaction(['storeName'], IDBTransaction.READ_WRITE)");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    request = evalAndLog("store.get('key')");
    request.addEventListener('success', getSuccess, true);
    request.onerror = unexpectedErrorCallback;
}

function getSuccess(evt)
{
    event = evt;
    debug("getSuccess():");
    shouldBeEqualToString("event.target.result.y", "value");

    var store = evalAndLog("store = event.target.source");
    request = evalAndLog("store.get(testDate)");
    request.addEventListener('success', getSuccessDateKey, false);
    request.onerror = unexpectedErrorCallback;
}

function getSuccessDateKey(evt)
{
    event = evt;
    debug("getSuccessDateKey():");
    shouldBeEqualToString("event.target.result.x", "foo");

    request = evalAndLog("store.delete('key')");
    request.onsuccess = removeSuccess;
    request.onerror = unexpectedErrorCallback;
}

function removeSuccess(evt)
{
    event = evt;
    debug("removeSuccess():");
    shouldBe("event.target.result", "undefined");

    request = evalAndLog("store.delete('key')");
    request.onsuccess = removeSuccessButNotThere;
    request.onerror = unexpectedErrorCallback;
}

function removeSuccessButNotThere(evt)
{
    event = evt;
    debug("removeSuccessButNotThere():");
    shouldBe("event.target.result", "undefined");
    var store = evalAndLog("store = event.target.source");

    debug("Passing an invalid key into store.get().");
    evalAndExpectException("store.get({})", "IDBDatabaseException.DATA_ERR");

    debug("Passing an invalid key into store.delete().");
    evalAndExpectException("store.delete({})", "IDBDatabaseException.DATA_ERR");

    debug("Passing an invalid key into store.add().");
    evalAndExpectException("store.add(null, {})", "IDBDatabaseException.DATA_ERR");

    debug("Passing an invalid key into store.put().");
    evalAndExpectException("store.put(null, {})", "IDBDatabaseException.DATA_ERR");

    testPreConditions();
}

function testPreConditions()
{
    debug("");
    debug("testPreConditions():");
    request = evalAndLog("db.setVersion('precondition version')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        storeWithInLineKeys = evalAndLog("storeWithInLineKeys = db.createObjectStore('storeWithInLineKeys', {keyPath: 'key'})");
        storeWithOutOfLineKeys = evalAndLog("storeWithOutOfLineKeys = db.createObjectStore('storeWithOutIOfLineKeys')");
        storeWithIndex = evalAndLog("storeWithIndex = db.createObjectStore('storeWithIndex')");
        index = evalAndLog("index = storeWithIndex.createIndex('indexName', 'indexKey')");

        debug("");
        debug("IDBObjectStore.put()");
        debug("The object store uses in-line keys and the key parameter was provided.");
        evalAndExpectException("storeWithInLineKeys.put({key: 1}, 'key')", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        evalAndExpectException("storeWithOutOfLineKeys.put({})", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses in-line keys and the result of evaluating the object store's key path yields a value and that value is not a valid key.");
        evalAndExpectException("storeWithInLineKeys.put({key: null})", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses in-line keys but no key generator and the result of evaluating the object store's key path does not yield a value.");
        evalAndExpectException("storeWithInLineKeys.put({})", "IDBDatabaseException.DATA_ERR");

        debug("The key parameter was provided but does not contain a valid key.");
        evalAndExpectException("storeWithOutOfLineKeys.put({}, null)", "IDBDatabaseException.DATA_ERR");

        debug("If there are any indexes referencing this object store whose key path is a string, evaluating their key path on the value parameter yields a value, and that value is not a valid key.");
        evalAndExpectException("storeWithIndex.put({indexKey: null}, 'key')", "IDBDatabaseException.DATA_ERR");

        debug("");
        debug("IDBObjectStore.add()");
        debug("The object store uses in-line keys and the key parameter was provided.");
        evalAndExpectException("storeWithInLineKeys.add({key: 1}, 'key')", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        evalAndExpectException("storeWithOutOfLineKeys.add({})", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses in-line keys and the result of evaluating the object store's key path yields a value and that value is not a valid key.");
        evalAndExpectException("storeWithInLineKeys.add({key: null})", "IDBDatabaseException.DATA_ERR");

        debug("The object store uses in-line keys but no key generator and the result of evaluating the object store's key path does not yield a value.");
        evalAndExpectException("storeWithInLineKeys.add({})", "IDBDatabaseException.DATA_ERR");

        debug("The key parameter was provided but does not contain a valid key.");
        evalAndExpectException("storeWithOutOfLineKeys.add({}, null)", "IDBDatabaseException.DATA_ERR");

        debug("If there are any indexes referencing this object store whose key path is a string, evaluating their key path on the value parameter yields a value, and that value is not a valid key.");
        evalAndExpectException("storeWithIndex.add({indexKey: null}, 'key')", "IDBDatabaseException.DATA_ERR");

        finishJSTest();
    };
}

var jsTestIsAsync = true;

test();
