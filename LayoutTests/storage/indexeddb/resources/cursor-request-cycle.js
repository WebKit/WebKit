if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/gc.js');
    importScripts('shared.js');
}

description("Verify that that cursors weakly hold request, and work if request is GC'd");

const cursorCount = 1000;

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

var checkedPendingRequests = false;

function continuedCursorSuccess(event)
{
    // A WeakRef keeps its target alive for the rest of the turn it was created in, so this check
    // cannot run in the turn that created cursorRefs. Here the other requests are still pending.
    if (!checkedPendingRequests) {
        checkedPendingRequests = true;
        debug("Ensure requests are not released if they are pending.");
        evalAndLog("gc()");
        shouldBeFalse("anyCollected(cursorRefs)");
        shouldBeFalse("anyCollected(cursorRequestRefs)");
    }

    cursor = event.target.result;
    var target = { key: "key2", value: "value2", extra: "456" };
    checkCursor(cursor, target, "Examine cursor after continue()");
    if (event.target.extra != "123") {
        testFailed("Examine cursor after continue(): event.target.extra is " + event.target.extra + ", should be 123");
    }
    cursors.push(cursor);
}

function onOpen(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("tx = db.transaction('store', 'readonly')");
    evalAndLog("store = tx.objectStore('store')");

    debug("Create " + cursorCount + " cursorRequests and check their results in otherRequestSuccess().");
    cursorRequests = [];
    cursorRequestRefs = [];
    for (let i = 0; i < cursorCount; ++i) {
        cursorRequest = store.openCursor();
        cursorRequests.push(cursorRequest);
        cursorRequestRefs.push(new WeakRef(cursorRequest));
        cursorRequest = null;
    }

    evalAndLog("otherRequest = store.get(0)");

    otherRequest.onsuccess = function otherRequestSuccess(evt) {
        preamble(evt);

        debug("Verify that results of openCursor requests can be accessed lazily.");
        evalAndLog("gc()");

        cursors = [];
        cursorRefs = [];
        var target = { key:"key1", value:"value1" };
        for (var i = 0; i < cursorRequests.length; i++) {
            cursor = cursorRequests[i].result;
            checkCursor(cursor, target, "Examine cursorRequests[" + i + "]");
            cursorRequests[i].extra = "123";
            cursor.extra = "456";
            cursors.push(cursor);
            cursorRefs.push(new WeakRef(cursor));
            cursor = null;

            // Assign a new handler to inspect the request and cursor indirectly.
            cursorRequests[i].onsuccess = continuedCursorSuccess;
        }

        debug("Ensure requests are not released if cursors are still around.");
        nukeArray(cursorRequests);
        evalAndLog("gc()");
        shouldBeFalse("anyCollected(cursorRequestRefs)");

        for (var i = 0; i < cursors.length; i++) {
            cursors[i].continue();
        }

        // The pending-request check happens in the first continuedCursorSuccess().
        nukeArray(cursors);

        evalAndLog("finalRequest = store.get(0)");
        finalRequest.onsuccess = function finalRequestSuccess(evt) {
            debug("Check result after transaction commits automatically after finishing requests.");
        };
    };

    tx.oncomplete = onTransactionComplete;
}

async function onTransactionComplete(evt)
{
    preamble(evt);
    debug("Ensure requests and cursors are released after transaction commits.");

    shouldBeNonNull("cursors");
    shouldBe("cursors.length", "cursorCount");
    nukeArray(cursors);
    collected = await gcUntil(() => anyCollected(cursorRefs) && anyCollected(cursorRequestRefs));
    shouldBeTrue("collected");

    finishJSTest();
}
