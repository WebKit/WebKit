if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyPath edge cases");

function test()
{
    removeVendorPrefixes();

    request = evalAndLog("indexedDB.deleteDatabase('keypath-edges')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        request = evalAndLog("indexedDB.open('keypath-edges')");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = openSuccess;
    };
}

function openSuccess()
{
    debug("");
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");
    request = evalAndLog("request = db.setVersion('1')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        transaction = request.result;
        transaction.onabort = unexpectedAbortCallback;
        evalAndLog("db.createObjectStore('store-with-path', {keyPath: 'foo'})");
        evalAndLog("db.createObjectStore('store-with-path-and-generator', {keyPath: 'foo', autoIncrement: true})");
        transaction.oncomplete = testKeyPaths;
    };
}

function testKeyPaths()
{
    debug("");
    debug("testKeyPaths():");

    transaction = evalAndLog("transaction = db.transaction(['store-with-path'], 'readwrite')");
    store = evalAndLog("store = transaction.objectStore('store-with-path')");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, should throw DATA_ERR");
    evalAndExpectException("store.put(null)", "IDBDatabaseException.DATA_ERR");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, should throw DATA_ERR");
    evalAndExpectException("store.put({})", "IDBDatabaseException.DATA_ERR");

    debug("");
    debug("Key path resolves to a value that is invalid key; should yield 'invalid' key, should throw DATA_ERR");
    evalAndExpectException("store.put({foo: null})", "IDBDatabaseException.DATA_ERR");

    debug("");
    debug("Key path resolves to a value that is valid key; should yield 'string' key, should succeed");
    request = evalAndLog("store.put({foo: 'zoo'})");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        testPassed("store.put succeeded");
    };

    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = testKeyPathsAndGenerator;
}

function testKeyPathsAndGenerator()
{
    debug("");
    debug("testKeyPathsAndGenerator():");

    transaction = evalAndLog("transaction = db.transaction(['store-with-path-and-generator'], 'readwrite')");
    store = evalAndLog("store = transaction.objectStore('store-with-path-and-generator')");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null but insertion would fail, so put request should raise exception");
    evalAndExpectException("store.put(null)", "IDBDatabaseException.DATA_ERR");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null but insertion would fail, so put request should raise exception");
    evalAndExpectException("store.put('string')", "IDBDatabaseException.DATA_ERR");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, key should be generated, put request should succeed");
    request = evalAndLog("store.put({})");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        testPassed("store.put succeeded");

        debug("");
        debug("Key path resolves to a value that is invalid key; should yield 'invalid' key, should throw DATA_ERR");
        evalAndExpectException("store.put({foo: null})", "IDBDatabaseException.DATA_ERR");

        debug("");
        debug("Key path resolves to a value that is valid key; should yield 'string' key, should succeed");
        request = evalAndLog("store.put({foo: 'zoo'})");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
            testPassed("store.put succeeded");

        };
    };

    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = finishJSTest;
}

test();