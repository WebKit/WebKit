if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that a request's result is dirtied when a cursor is continued");

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    store.put("value", "key");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");

    evalAndLog("cursorRequest = store.openCursor()");
    cursorRequest.onsuccess = function cursorRequestSuccess(evt) {
        preamble(evt);
        if (!cursorRequest.result)
            return;

        evalAndLog("cursor = cursorRequest.result");
        evalAndLog("cursor.continue()");
        evalAndExpectException("cursorRequest.result", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");    };

    tx.oncomplete = finishJSTest;
}
