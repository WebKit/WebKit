if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure IndexedDB's cursor prefetch cache requests are invalidated");

indexedDBTest(prepareDatabase, onOpenSuccess);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    debug("Populate with even records...");
    for (var i = 0; i < 10; i += 2)
        store.put(i, i);
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");

    evalAndLog("tx = db.transaction('store', 'readwrite')");
    evalAndLog("store = tx.objectStore('store')");
    evalAndLog("request = store.openCursor()");

    kPrefetchThreshold = 3;

    expected = ["0", "2", "4", "6", "7", "8", "9"];
    continueCount = 0;
    request.onsuccess = function cursorSuccess() {
        preamble();
        cursor = request.result;
        if (!cursor)
            return;
        ++continueCount;

        expect = expected.shift();
        shouldBe("cursor.key", expect);
        evalAndLog("cursor.continue()");

        if (continueCount === kPrefetchThreshold) {
            debug("\nThat should have triggered a prefetch, injecting odd records...");
            for (var i = 1; i < 10; i += 2)
                store.put(i, i);
        }
    };

    tx.oncomplete = function() {
        shouldBe("continueCount", "7");
        finishJSTest();
    };
}
