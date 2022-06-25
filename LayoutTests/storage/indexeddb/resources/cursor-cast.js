if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure cursor wrappers are created correctly.");

indexedDBTest(prepareDatabase, verifyWrappers);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put(0, 0)");
}

function verifyWrappers(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store', 'readwrite')");
    evalAndLog("request = tx.objectStore('store').openCursor()");

    request.onsuccess = function onOpenCursorSuccess(evt) {
        preamble(evt);
        evalAndLog("cursor = event.target.result");
        evalAndLog("request = cursor.update(1)");

        request.onsuccess = function onUpdateSuccess(evt) {
            preamble(evt);
            evalAndLog("cursor = null");
            gc();
            gc(); // FIXME: Shouldn't need to call twice. http://crbug.com/288072
            setTimeout(checkCursorType, 0);
        };
    };
}

function checkCursorType() {
    shouldBeEqualToString("request.source.toString()", "[object IDBCursorWithValue]");
    finishJSTest();
}
