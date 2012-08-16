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
    shouldBeNonNull("trans");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

    debug("createObjectStore():");
    self.store = evalAndLog("store = db.createObjectStore('storeName', null)");
    var storeNames = evalAndLog("storeNames = db.objectStoreNames");

    shouldBeTrue("'name' in store");
    shouldBeTrue("'keyPath' in store");
    shouldBeTrue("'indexNames' in store");
    shouldBeTrue("'transaction' in store");
    shouldBeTrue("'autoIncrement' in store");
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
    shouldBeFalse("store.autoIncrement");
    shouldBe("storeNames.contains('storeName')", "true");
    shouldBe("storeNames.length", "1");

    shouldBeEqualToString("db.createObjectStore('storeWithKeyPath', {keyPath: 'path'}).keyPath", "path");
    shouldBeTrue("db.createObjectStore('storeWithKeyGenerator', {autoIncrement: true}).autoIncrement");

    // FIXME: test all of object store's methods.

    debug("Ask for an index that doesn't exist:");
    evalAndExpectException("store.index('asdf')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
    createIndex();
}

function createIndex()
{
    debug("createIndex():");
    var index = evalAndLog("index = store.createIndex('indexName', 'x', {unique: true})"); // true == unique requirement.
    shouldBeNonNull("index");
    shouldBeTrue("store.indexNames.contains('indexName')");
    index = evalAndLog("index = store.index('indexName')");
    shouldBeNonNull("index");

    debug("Ask for an index that doesn't exist:");
    evalAndExpectException("store.index('asdf')", "DOMException.NOT_FOUND_ERR", "'NotFoundError'");
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
    shouldBeNonNull("setVersionTrans");
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
    shouldBeUndefined("store.indexNames[1]");
    shouldBeUndefined("store.indexNames[100]");
    shouldBeNull("store.indexNames.item(1)");
    shouldBeNull("store.indexNames.item(100)");
    addData();
}

var testDate = new Date("August 25, 1991 20:57:08");
var testDateB = new Date("Wed Jan 05 2011 15:54:49");

function addData()
{
    var transaction = evalAndLog("transaction = db.transaction(['storeName'], 'readwrite')");
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
    evalAndExpectException("store.add({x: 'bar', y: self}, 'bar')", "DOMException.DATA_CLONE_ERR");

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
    shouldBe("event.target.error.name", "'ConstraintError'");

    evalAndLog("event.preventDefault()");

    transaction = evalAndLog("db.transaction(['storeName'], 'readwrite')");
    transaction.onabort = unexpectedErrorCallback;
    var store = evalAndLog("store = transaction.objectStore('storeName')");

    evalAndLog("store.add({x: 'somevalue'}, 'somekey')");
    evalAndExpectException("store.add({x: 'othervalue'}, null)", "IDBDatabaseException.DATA_ERR", "'DataError'");

    transaction = evalAndLog("db.transaction(['storeName'], 'readwrite')");
    transaction.onabort = unexpectedErrorCallback;
    store = evalAndLog("store = transaction.objectStore('storeName')");

    debug("Ensure invalid key pointed at by index keyPath is ignored");
    evalAndLog("store.add({x: null}, 'validkey')");

    transaction = evalAndLog("db.transaction(['storeName'], 'readwrite')");
    transaction.onabort = unexpectedErrorCallback;
    store = evalAndLog("store = transaction.objectStore('storeName')");

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
    evalAndExpectException("store.get({})", "IDBDatabaseException.DATA_ERR", "'DataError'");

    debug("Passing an invalid key into store.delete().");
    evalAndExpectException("store.delete({})", "IDBDatabaseException.DATA_ERR", "'DataError'");

    debug("Passing an invalid key into store.add().");
    evalAndExpectException("store.add(null, {})", "IDBDatabaseException.DATA_ERR", "'DataError'");

    debug("Passing an invalid key into store.put().");
    evalAndExpectException("store.put(null, {})", "IDBDatabaseException.DATA_ERR", "'DataError'");

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
        evalAndExpectException("storeWithInLineKeys.put({key: 1}, 'key')", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        evalAndExpectException("storeWithOutOfLineKeys.put({})", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses in-line keys and the result of evaluating the object store's key path yields a value and that value is not a valid key.");
        evalAndExpectException("storeWithInLineKeys.put({key: null})", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses in-line keys but no key generator and the result of evaluating the object store's key path does not yield a value.");
        evalAndExpectException("storeWithInLineKeys.put({})", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The key parameter was provided but does not contain a valid key.");
        evalAndExpectException("storeWithOutOfLineKeys.put({}, null)", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("");
        debug("IDBObjectStore.add()");
        debug("The object store uses in-line keys and the key parameter was provided.");
        evalAndExpectException("storeWithInLineKeys.add({key: 1}, 'key')", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        evalAndExpectException("storeWithOutOfLineKeys.add({})", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses in-line keys and the result of evaluating the object store's key path yields a value and that value is not a valid key.");
        evalAndExpectException("storeWithInLineKeys.add({key: null})", "IDBDatabaseException.DATA_ERR", "'DataError'");

        debug("The object store uses in-line keys but no key generator and the result of evaluating the object store's key path does not yield a value.");
        evalAndExpectException("storeWithInLineKeys.add({})", "IDBDatabaseException.DATA_ERR");

        debug("The key parameter was provided but does not contain a valid key.");
        evalAndExpectException("storeWithOutOfLineKeys.add({}, null)", "IDBDatabaseException.DATA_ERR");

        finishJSTest();
    };
}

var jsTestIsAsync = true;

test();
