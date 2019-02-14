if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that IDBCursor is not leaked when there is a reference cycle for value attribute");

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    store.put({ name: 'value' }, 'key');
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("store = tx.objectStore('store')");

    evalAndLog("cursorRequest = store.openCursor()");
    cursorRequest.onsuccess = function openCursorRequestSuccess(evt) {
        preamble(evt);
    };

    evalAndLog("getRequest = store.get('key')");
    getRequest.onsuccess = () => {
        shouldBeEqualToString("getRequest.result.name", "value");

        evalAndLog("cursor = cursorRequest.result");
        shouldBeNonNull("cursor");
        evalAndLog("value = cursor.value");
        shouldBeEqualToString("value.name", "value");
        evalAndLog("value.cycle = cursor");

        cursorObservation = internals.observeGC(cursor);
        valueObservation = internals.observeGC(value);
        evalAndLog("cursor = null");
        evalAndLog("cursorRequest = null");
        evalAndLog("gc()");
        shouldBeFalse("cursorObservation.wasCollected");

        evalAndLog("value = null");
        evalAndLog("gc()");
        shouldBeTrue("cursorObservation.wasCollected");
        shouldBeTrue("valueObservation.wasCollected");
    }

    tx.oncomplete = finishJSTest;
}