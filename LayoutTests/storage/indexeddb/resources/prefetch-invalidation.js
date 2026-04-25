if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Ensure IndexedDB's write operations invalidate cursor prefetch caches");

indexedDBTest(prepareDatabase, onOpenSuccess);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
}

function onOpenSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");

    var steps = [
        deleteRange,
        clearStore
    ];

    (function nextStep() {
        var step = steps.shift();
        if (step) {
            doPrefetchInvalidationTest(step, nextStep);
        } else {
            finishJSTest();
            return;
        }
    }());
}

function doPrefetchInvalidationTest(operation, callback)
{
    debug("");
    debug("-------------------------------------------");
    preamble();
    evalAndLog("store = db.transaction('store', 'readwrite').objectStore('store')");
    debug("Populate the store with 100 records.");
    for (var i = 0; i < 100; ++i)
        store.put(i, i);
    evalAndLog("cursorRequest = store.openCursor()");
    continue50Times(operation, callback);
}

function continue50Times(operation, callback)
{
    preamble();
    var count = 0;

    cursorRequest.onsuccess = function() {
        var cursor = cursorRequest.result;
        ++count;
        if (count < 50) {
            cursor.continue();
            return;
        }
        shouldBeNonNull("cursorRequest.result");
        doOperationAndContinue(operation, callback);
    }
}

function doOperationAndContinue(operation, callback)
{
    preamble();
    operation();
    evalAndLog("cursor = cursorRequest.result");
    evalAndLog("cursor.continue()")
    cursorRequest.onsuccess = function onContinueSuccess() {
        preamble();
        shouldBeNull("cursorRequest.result");
        callback();
    };
}

function deleteRange()
{
    return evalAndLog("store.delete(IDBKeyRange.bound(-Infinity, +Infinity))");
}

function clearStore()
{
    return evalAndLog("store.clear()");
}
