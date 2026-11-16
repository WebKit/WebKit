if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

if (window.testRunner)
    testRunner.setAllowStorageQuotaIncrease(false);

var quota = 400 * 1024; // default quota for testing.
var numIndices = 10;

description("This test checks that the size estimate associated with adding an object to a store with many indices is reasonable.");

indexedDBTest(prepareDatabase, onOpenSuccess);

function prepareDatabase(event)
{
    preamble(event);

    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");

    let db = event.target;
    for (let i = 0; i < numIndices; i++) {
        let indexName = 'a' + i;
        store.createIndex(indexName, indexName);
    }
}

function onOpenSuccess(event)
{
    preamble(event);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.transaction('store', 'readwrite').objectStore('store')");

    // The size of the indexed attributes a0, a1, ... is small, so they shouldn't have a material
    // effect on the estimated put size for quota purposes.
    evalAndLog(`request = store.add({a0: 0, a1: 1, payload: new Uint8Array(${quota / numIndices})}, 42)`);

    request.onsuccess = function(event) {
        reqEvent = event;
        shouldBe("reqEvent.target.result", "42");
        finishJSTest();
    }

    request.onerror = function(event) {
        testFailed("Add operation failed, but it should succeed because it uses much less space than the storage limit.");
        finishJSTest();
    }
}
