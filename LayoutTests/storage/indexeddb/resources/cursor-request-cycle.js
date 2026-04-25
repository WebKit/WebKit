if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Verify that that cursors weakly hold request, and work if request is GC'd");

indexedDBTest(prepareDatabase, onOpen);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    store.put("value1", "key1");
    store.put("value2", "key2");
}

function checkCursor(cursor, target, message)
{
    if (!cursor)
        testFailed(message + ": cursor is null");
    if (cursor.key != target.key)
        testFailed(message + ": cursor.key is " + cursor.key + ", should be " + target.key);
    if (cursor.value != target.value)
        testFailed(message + ": cursor.value is " + cursor.value + ", should be " + target.value);
    if (cursor.extra != target.extra)
        testFailed(message + ": cursor.extra is " + cursor.extra + ", should be " + target.extra);
}

function isAnyCollected(observers)
{
    for (let observer of observers) {
        if (observer.wasCollected)
            return true;
    }
    return false;
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store', 'readonly')");
    evalAndLog("store = tx.objectStore('store')");

    debug("Create 1000 cursorRequests and check their results in otherRequestSuccess().");
    cursorRequests = [];
    cursorRequestObservers = [];
    for (let i = 0; i < 1000; ++i) {
        cursorRequest = store.openCursor();
        cursorRequests.push(cursorRequest);
        cursorRequestObservers.push(internals.observeGC(cursorRequest));
        cursorRequest = null;
    }

    evalAndLog("otherRequest = store.get(0)");

    otherRequest.onsuccess = function otherRequestSuccess(evt) {
        preamble(evt);

        debug("Verify that results of openCursor requests can be accessed lazily.");
        evalAndLog("gc()");

        cursors = [];
        cursorObservers = [];
        var target = { key:"key1", value:"value1" };
        for (var i = 0; i < cursorRequests.length; i++) {
            cursor = cursorRequests[i].result;
            checkCursor(cursor, target, "Examine cursorRequests[" + i + "]");
            cursorRequests[i].extra = "123";
            cursor.extra = "456";
            cursors.push(cursor);
            cursorObservers.push(internals.observeGC(cursor));
            cursor = null;

            // Assign a new handler to inspect the request and cursor indirectly.
            cursorRequests[i].onsuccess = (event)=>{
                cursor = event.target.result;
                var target = { key: "key2", value:"value2", extra:"456" };
                checkCursor(cursor, target, "Examine cursor after continue()");
                if (event.target.extra != "123") {
                    testFailed("Examine cursor after continue(): event.target.extra is " + event.target.extra + ", should be 123");
                }
                cursors.push(cursor);
            };
        }

        debug("Ensure requests are not released if cursors are still around.");
        evalAndLog("cursorRequests = null");
        evalAndLog("gc()");
        shouldBeFalse("isAnyCollected(cursorRequestObservers)");

        for (var i = 0; i < cursors.length; i++) {
            cursors[i].continue();
        }

        debug("Ensure requests are not released if they are pending.");
        evalAndLog("cursors = null"); 
        evalAndLog("gc()");
        shouldBeFalse("isAnyCollected(cursorObservers)");
        shouldBeFalse("isAnyCollected(cursorRequestObservers)");
        cursors = []; 

        evalAndLog("finalRequest = store.get(0)");
        finalRequest.onsuccess = function finalRequestSuccess(evt) {
            debug("Check result after transaction commits automatically after finishing requests.");
        };
    };

    tx.oncomplete = onTransactionComplete;
}

function onTransactionComplete(evt)
{
    preamble(evt);
    debug("Ensure requests and cursors are released after transaction commits.");

    shouldBeNonNull("cursors");
    shouldBe("cursors.length", "1000");
    evalAndLog("cursors = null");
    evalAndLog("gc()");
    shouldBeTrue("isAnyCollected(cursorObservers)");
    shouldBeTrue("isAnyCollected(cursorRequestObservers)");

    finishJSTest();
}
