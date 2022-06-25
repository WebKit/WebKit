if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Validate the overloads of IDBObjectStore.openCursor(), IDBIndex.openCursor() and IDBIndex.openKeyCursor().");

indexedDBTest(prepareDatabase, verifyOverloads);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("index = store.createIndex('index', 'value')");
    evalAndLog("store.put({value: 0}, 0)");
}

function verifyOverloads()
{
    debug("");
    debug("verifyOverloads():");
    evalAndLog("trans = db.transaction('store')");
    evalAndLog("store = trans.objectStore('store')");
    evalAndLog("index = store.index('index')");

    checkCursorDirection("store.openCursor()", "next");
    checkCursorDirection("store.openCursor(0)", "next");
    checkCursorDirection("store.openCursor(0, 'next')", "next");
    checkCursorDirection("store.openCursor(0, 'nextunique')", "nextunique");
    checkCursorDirection("store.openCursor(0, 'prev')", "prev");
    checkCursorDirection("store.openCursor(0, 'prevunique')", "prevunique");

    checkCursorDirection("store.openCursor(IDBKeyRange.only(0))", "next");
    checkCursorDirection("store.openCursor(IDBKeyRange.only(0), 'next')", "next");
    checkCursorDirection("store.openCursor(IDBKeyRange.only(0), 'nextunique')", "nextunique");
    checkCursorDirection("store.openCursor(IDBKeyRange.only(0), 'prev')", "prev");
    checkCursorDirection("store.openCursor(IDBKeyRange.only(0), 'prevunique')", "prevunique");

    checkCursorDirection("index.openCursor()", "next");
    checkCursorDirection("index.openCursor(0)", "next");
    checkCursorDirection("index.openCursor(0, 'next')", "next");
    checkCursorDirection("index.openCursor(0, 'nextunique')", "nextunique");
    checkCursorDirection("index.openCursor(0, 'prev')", "prev");
    checkCursorDirection("index.openCursor(0, 'prevunique')", "prevunique");

    checkCursorDirection("index.openCursor(IDBKeyRange.only(0))", "next");
    checkCursorDirection("index.openCursor(IDBKeyRange.only(0), 'next')", "next");
    checkCursorDirection("index.openCursor(IDBKeyRange.only(0), 'nextunique')", "nextunique");
    checkCursorDirection("index.openCursor(IDBKeyRange.only(0), 'prev')", "prev");
    checkCursorDirection("index.openCursor(IDBKeyRange.only(0), 'prevunique')", "prevunique");

    checkCursorDirection("index.openKeyCursor()", "next");
    checkCursorDirection("index.openKeyCursor(0)", "next");
    checkCursorDirection("index.openKeyCursor(0, 'next')", "next");
    checkCursorDirection("index.openKeyCursor(0, 'nextunique')", "nextunique");
    checkCursorDirection("index.openKeyCursor(0, 'prev')", "prev");
    checkCursorDirection("index.openKeyCursor(0, 'prevunique')", "prevunique");

    checkCursorDirection("index.openKeyCursor(IDBKeyRange.only(0))", "next");
    checkCursorDirection("index.openKeyCursor(IDBKeyRange.only(0), 'next')", "next");
    checkCursorDirection("index.openKeyCursor(IDBKeyRange.only(0), 'nextunique')", "nextunique");
    checkCursorDirection("index.openKeyCursor(IDBKeyRange.only(0), 'prev')", "prev");
    checkCursorDirection("index.openKeyCursor(IDBKeyRange.only(0), 'prevunique')", "prevunique");

    trans.oncomplete = finishJSTest;
}

function checkCursorDirection(statement, direction)
{
    request = eval(statement);
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug(statement);
        shouldBeNonNull("event.target.result")
        shouldBeEqualToString("event.target.result.direction", direction);
    };
}
