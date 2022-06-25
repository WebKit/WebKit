if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test for http://crbug.com/108071");

// Have to be at least 5 here: 1 initial, 3 continues to trigger prefetch and 1
// post-abort outstanding continue.
var names = ['Alpha', 'Bravo', 'Charlie', 'Delta', 'Echo'];

indexedDBTest(prepareDatabase, iterateAndDeleteFirstElement);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    trans = event.target.transaction;

    var objectStore = evalAndLog("objectStore = db.createObjectStore('store', {keyPath: 'id'})");
    resetObjectStore();
}

function resetObjectStore()
{
    debug("\nresetObjectStore():");

    objectStore = trans.objectStore('store');
    evalAndLog("objectStore.clear()");
    for (i = 0; i < names.length; i++) {
        request = evalAndLog("objectStore.add({id: " + i + ", name: \"" + names[i] + "\"})");
        request.onerror = unexpectedErrorCallback;
    }

    debug("");
}

function iterateAndDeleteFirstElement()
{
    debug("iterateAndDeleteFirstElement():");

    evalAndLog("trans = db.transaction(['store'], 'readwrite')");
    trans.onabort = transactionAborted;
    trans.oncomplete = unexpectedCompleteCallback;

    // Create the cursor and iterate over the whole thing.
    request = evalAndLog("trans.objectStore('store').openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        var cursor = event.target.result;
        if (cursor == null) {
            resetObjectStore();
            prefetchAndAbort();
            return;
        }

        debug(cursor.value.id + ": " + cursor.value.name);

        if (cursor.value.id == 0) {
            // Delete the first element when we see it.
            request = evalAndLog("trans.objectStore('store').delete(0)");
            request.onerror = unexpectedErrorCallback;
            request.onsuccess = function() { cursor.continue(); };
        } else {
            cursor.continue();
        }
    }
}

function prefetchAndAbort()
{
    debug("prefetchAndAbort():");

    request = evalAndLog("trans.objectStore('store').openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        var cursor = event.target.result;
        debug(cursor.value.id + ": " + cursor.value.name);
        // Have to iterate to 3 in order to prefetch, but can't iterate past 3
        // so that there will be a continue remaining.
        if (cursor.value.id == 3) {
            evalAndLog("trans.abort()");
        } else {
            cursor.continue();
        }
    }
}

function transactionAborted()
{
    testPassed("Transaction aborted as expected");
    finishJSTest();
}
