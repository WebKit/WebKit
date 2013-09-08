if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB keyPath edge cases");

indexedDBTest(prepareDatabase, testKeyPaths);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("db.createObjectStore('store-with-path', {keyPath: 'foo'})");
    evalAndLog("db.createObjectStore('store-with-path-and-generator', {keyPath: 'foo', autoIncrement: true})");
}

function testKeyPaths()
{
    debug("");
    debug("testKeyPaths():");

    transaction = evalAndLog("transaction = db.transaction(['store-with-path'], 'readwrite')");
    store = evalAndLog("store = transaction.objectStore('store-with-path')");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, should throw DATA_ERR");
    evalAndExpectException("store.put(null)", "0", "'DataError'");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, should throw DATA_ERR");
    evalAndExpectException("store.put({})", "0", "'DataError'");

    debug("");
    debug("Key path resolves to a value that is invalid key; should yield 'invalid' key, should throw DATA_ERR");
    evalAndExpectException("store.put({foo: null})", "0", "'DataError'");

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
    evalAndExpectException("store.put(null)", "0", "'DataError'");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null but insertion would fail, so put request should raise exception");
    evalAndExpectException("store.put('string')", "0", "'DataError'");

    debug("");
    debug("Key path doesn't resolve to a value; should yield null, key should be generated, put request should succeed");
    request = evalAndLog("store.put({})");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        testPassed("store.put succeeded");

        debug("");
        debug("Key path resolves to a value that is invalid key; should yield 'invalid' key, should throw DATA_ERR");
        evalAndExpectException("store.put({foo: null})", "0", "'DataError'");

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
