if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/gc.js');
    importScripts('shared.js');
}

description('Verify that that cursors weakly hold script value properties');

const cursorCount = 1000;

indexedDBTest(prepareDatabase, onOpen);

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
    cursorRefs = [];
    for (let i = 0; i < cursorCount; ++i) {
        store.openCursor().onsuccess = (event) => {
            cursor = event.target.result
            cursor.key.cursor = cursor;
            cursor.primaryKey.cursor = cursor;
            cursor.value.cursor = cursor;
            cursorRefs.push(new WeakRef(cursor));
            cursor = null;
        };
    }
    tx.oncomplete = async function() {
        db.close();
        shouldBe('cursorRefs.length', 'cursorCount');

        collected = await gcUntil(() => anyCollected(cursorRefs));

        shouldBeTrue('collected');
        finishJSTest();
    };
}
