if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/gc.js');
    importScripts('shared.js');
}

description("Verify that IDBCursor is not leaked when there is a reference cycle for value attribute");

const cursorCount = 100;

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

    debug("Open " + cursorCount + " cursors.");
    cursorRequests = [];
    for (let i = 0; i < cursorCount; ++i)
        cursorRequests.push(store.openCursor());

    cursors = [];
    values = [];
    cursorRefs = [];
    valueRefs = [];

    evalAndLog("getRequest = store.get('key')");
    getRequest.onsuccess = () => {
        shouldBeEqualToString("getRequest.result.name", "value");

        debug("Give every cursor a value that refers back to the cursor.");
        for (let i = 0; i < cursorCount; ++i) {
            let cursor = cursorRequests[i].result;
            let value = cursor.value;
            value.cycle = cursor;
            cursors.push(cursor);
            values.push(value);
            cursorRefs.push(new WeakRef(cursor));
            valueRefs.push(new WeakRef(value));
        }
        shouldBe("cursors.length", "cursorCount");
        shouldBeEqualToString("values[0].name", "value");
    };

    tx.oncomplete = async () => {
        debug("Ensure cursors are not released while their values are referenced.");
        nukeArray(cursors);
        nukeArray(cursorRequests);
        await turnEventLoop();
        gc();
        shouldBeFalse("anyCollected(cursorRefs)");

        debug("Ensure cursors and values are released once the values are dropped.");
        nukeArray(values);
        collected = await gcUntil(() => anyCollected(cursorRefs) && anyCollected(valueRefs));
        shouldBeTrue("collected");
        finishJSTest();
    }
}
