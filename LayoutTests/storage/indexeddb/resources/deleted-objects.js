if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that IndexedDB objects that have been deleted throw exceptions");

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    trans = event.target.transaction;
    connection = event.target.result;

    debug("");
    evalAndLog("deletedStore = connection.createObjectStore('deletedStore')");
    evalAndLog("store = connection.createObjectStore('store')");
    evalAndLog("deletedIndex = store.createIndex('deletedIndex', 'path')");

    debug("");
    evalAndLog("connection.deleteObjectStore('deletedStore')");
    evalAndLog("store.deleteIndex('deletedIndex')");

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
    evalAndExpectException("deletedStore.createIndex('name', 'path')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.index('name')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.deleteIndex('name')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count()", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count(0)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    evalAndExpectException("deletedStore.count(IDBKeyRange.only(0))", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");

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

    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = finishJSTest;
}
