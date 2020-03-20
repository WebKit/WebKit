if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description('Verify that that cursors weakly hold script value properties');

if (window.internals) {
    indexedDBTest(prepareDatabase, onOpen);
} else {
    testFailed('This test requires access to the Internals object');
    finishJSTest();
}

function prepareDatabase(evt)
{
    db = event.target.result;
    store = db.createObjectStore('store');
    store.put({value: 'value'}, ['key']);
}

function onOpen(evt)
{
    // evalAndLog() is not used as that generates new DOM nodes.

    db = evt.target.result;
    tx = db.transaction('store', 'readonly');
    store = tx.objectStore('store');
    cursorObservers = [];
    for (let i = 0; i < 1000; ++i) {
        store.openCursor().onsuccess = (event) => {
            cursor = event.target.result
            cursor.key.cursor = cursor;
            cursor.primaryKey.cursor = cursor;
            cursor.value.cursor = cursor;
            cursorObservers.push(internals.observeGC(cursor));
            cursor = null;
        };
    }
    tx.oncomplete = function() {
        db.close();
        shouldBe('cursorObservers.length', '1000');

        gc();

        anyCollected = false;
        for (let observer of cursorObservers) {
            if (observer.wasCollected)
                anyCollected = true;
        }

        shouldBeTrue('anyCollected');
        finishJSTest();
    };
}
