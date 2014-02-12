if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB Array-type keyPaths");

indexedDBTest(prepareDatabase, testKeyPaths);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('store', {keyPath: ['a', 'b']})");
    evalAndLog("store.createIndex('index', ['c', 'd'])");

    evalAndExpectException("db.createObjectStore('store-with-generator', {keyPath: ['a', 'b'], autoIncrement: true})", "DOMException.INVALID_ACCESS_ERR");
    evalAndExpectException("store.createIndex('index-multientry', ['e', 'f'], {multiEntry: true})", "DOMException.INVALID_ACCESS_ERR");

    debug("");
    debug("Empty arrays are not valid key paths:");
    evalAndExpectException("db.createObjectStore('store-keypath-empty-array', {keyPath: []})", "DOMException.SYNTAX_ERR");
    evalAndExpectException("store.createIndex('index-keypath-empty-array', [])", "DOMException.SYNTAX_ERR");
}

function testKeyPaths()
{
    debug("");
    debug("testKeyPaths():");

    transaction = evalAndLog("transaction = db.transaction(['store'], 'readwrite')");
    transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = transaction.objectStore('store')");
    evalAndLog("index = store.index('index')");

    debug("");
    evalAndLog("request = store.put({a: 1, b: 2, c: 3, d: 4})");
    request.onerror = unexpectedErrorCallback;
    checkStore();

    function checkStore() {
        evalAndLog("request = store.openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
            evalAndLog("cursor = request.result");
            shouldBeNonNull("cursor");
            shouldBeEqualToString("JSON.stringify(cursor.key)", "[1,2]");
            checkIndex();
        };
    };

    function checkIndex() {
        evalAndLog("request = index.openCursor()");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function () {
            evalAndLog("cursor = request.result");
            shouldBeNonNull("cursor");
            shouldBeEqualToString("JSON.stringify(cursor.primaryKey)", "[1,2]");
            shouldBeEqualToString("JSON.stringify(cursor.key)", "[3,4]");
        };
    };

    transaction.oncomplete = finishJSTest;
}
