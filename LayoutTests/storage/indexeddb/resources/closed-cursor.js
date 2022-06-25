if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that that cursors accessed after being closed are well behaved");

indexedDBTest(prepareDatabase, onOpen);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put({value: 'value'}, ['key'])");
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");
    evalAndLog("cursorRequest = store.openCursor()");
    cursorRequest.onsuccess = function openCursorSuccess(evt) {
        preamble(evt);
        evalAndLog("cursor = cursorRequest.result");
        debug("Don't continue the cursor, so it retains its key/primaryKey/value");
    };
    tx.oncomplete = function transactionComplete(evt) {
        preamble(evt);
        shouldBeEqualToString("JSON.stringify(cursor.key)", '["key"]');
        shouldBeEqualToString("JSON.stringify(cursor.primaryKey)", '["key"]');
        shouldBeEqualToString("JSON.stringify(cursor.value)", '{"value":"value"}');
        finishJSTest();
    };
}
