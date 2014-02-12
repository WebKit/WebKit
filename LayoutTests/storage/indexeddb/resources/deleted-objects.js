if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that IndexedDB objects that have been deleted throw exceptions");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    trans = event.target.transaction;
    connection = event.target.result;

    testStore();
}

function testStore()
{
    preamble();

    evalAndLog("deletedStore = connection.createObjectStore('deletedStore')");
    evalAndLog("connection.deleteObjectStore('deletedStore')");

    debug("");

    evalAndExpectException("deletedStore.put(0, 0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.add(0, 0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.delete(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.delete(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.get(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.get(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.clear()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openKeyCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openKeyCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openKeyCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openKeyCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.openKeyCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.createIndex('name', 'path')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.index('name')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.deleteIndex('name')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    testIndex();
}

function testIndex()
{
    preamble();

    evalAndLog("store = connection.createObjectStore('store')");
    evalAndLog("deletedIndex = store.createIndex('deletedIndex', 'path')");
    evalAndLog("store.deleteIndex('deletedIndex')");

    debug("");

    evalAndExpectException("deletedIndex.openCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openKeyCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openKeyCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openKeyCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openKeyCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.openKeyCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.get(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.get(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.getKey(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.getKey(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.count()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.count(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedIndex.count(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    testTransitiveDeletion();
}

function testTransitiveDeletion()
{
    preamble();

    evalAndLog("deletedStore = connection.createObjectStore('deletedStore')");
    evalAndLog("indexOfDeletedStore = deletedStore.createIndex('index', 'path')");
    evalAndLog("connection.deleteObjectStore('deletedStore')");

    debug("");

    evalAndExpectException("indexOfDeletedStore.openCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openKeyCursor()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openKeyCursor(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openKeyCursor(0, 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openKeyCursor(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.openKeyCursor(IDBKeyRange.only(0), 'next')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.get(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.get(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.getKey(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.getKey(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.count()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.count(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("indexOfDeletedStore.count(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

    testObjectStoreCursor();
}

function testObjectStoreCursor()
{
    preamble();

    evalAndLog("deletedStore = connection.createObjectStore('deletedStore')");
    evalAndLog("deletedStore.put(0, 0)");

    request = evalAndLog("deletedStore.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBe("cursor.key", "0");
        shouldBe("cursor.value", "0");

        evalAndLog("connection.deleteObjectStore('deletedStore')");
        evalAndExpectException("cursor.delete()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.update(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.advance(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

        testIndexCursor();
    };
}


function testIndexCursor()
{
    preamble();

    evalAndLog("store.put({id: 123}, 0)");
    evalAndLog("deletedIndex = store.createIndex('deletedIndex', 'id')");

    request = evalAndLog("deletedIndex.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBe("cursor.key", "123");
        shouldBe("cursor.primaryKey", "0");

        evalAndLog("store.deleteIndex('deletedIndex')");
        evalAndExpectException("cursor.delete()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.update(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.advance(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

        testIndexOfDeletedStoreCursor();
    };
}

function testIndexOfDeletedStoreCursor()
{
    preamble();

    evalAndLog("deletedStore = connection.createObjectStore('deletedStore')");
    evalAndLog("deletedStore.put({id: 123}, 0)");
    evalAndLog("index = deletedStore.createIndex('index', 'id')");

    request = evalAndLog("index.openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBe("cursor.key", "123");
        shouldBe("cursor.primaryKey", "0");

        evalAndLog("connection.deleteObjectStore('deletedStore')");
        evalAndExpectException("cursor.delete()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.update(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.continue()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
        evalAndExpectException("cursor.advance(1)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

        trans.onabort = unexpectedAbortCallback;
        trans.oncomplete = finishJSTest;
    };
}
