if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test mutating an IndexedDB's objectstore from a cursor.");

indexedDBTest(prepareDatabase, openForwardCursor);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    var objectStore = evalAndLog("objectStore = db.createObjectStore('store')");
    evalAndLog("objectStore.add(1, 1).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add(2, 2).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add(3, 3).onerror = unexpectedErrorCallback");
    evalAndLog("objectStore.add(4, 4).onerror = unexpectedErrorCallback");
}

function openForwardCursor()
{
    debug("openForwardCursor()");
    evalAndLog("trans = db.transaction(['store'], 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = forwardCursorComplete;

    self.objectStore = evalAndLog("trans.objectStore('store')");
    request = evalAndLog("objectStore.openCursor()");
    request.onsuccess = forwardCursor;
    request.onerror = unexpectedErrorCallback;
    self.cursorSteps = 0;
}

function forwardCursor()
{
    debug("forwardCursor()");
    self.cursor = event.target.result;

    if (event.target.result == null) {
        shouldBe("cursorSteps", "5");

        // Let the transaction finish.
        return;
    }

    debug(++cursorSteps);
    shouldBe("cursor.key", "cursorSteps");
    shouldBe("cursor.value", "cursorSteps");

    if (cursorSteps == 1) {
        request = evalAndLog("event.target.source.add(5, 5)");
        request.onsuccess = function() { evalAndLog("cursor.continue()"); }
        request.onerror = unexpectedErrorCallback;
    } else {
        evalAndLog("cursor.continue()");
    }
}

function forwardCursorComplete()
{
    debug("forwardCursorComplete()");
    openReverseCursor()
}

function openReverseCursor()
{
    debug("openReverseCursor()");
    evalAndLog("trans = db.transaction(['store'], 'readwrite')");
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = reverseCursorComplete;

    self.objectStore = evalAndLog("trans.objectStore('store')");
    request = evalAndLog("objectStore.openCursor(null, 'prev')");
    request.onsuccess = reverseCursor;
    request.onerror = unexpectedErrorCallback;
    self.cursorSteps = 6;
}

function reverseCursor()
{
    debug("reverseCursor()");
    self.cursor = event.target.result;

    if (event.target.result == null) {
        shouldBe("cursorSteps", "0");

        // Let the transaction finish.
        return;
    }

    debug(--cursorSteps);
    shouldBe("cursor.key", "cursorSteps");
    shouldBe("cursor.value", "cursorSteps");

    if (cursorSteps == 2) {
        request = evalAndLog("event.target.source.add(0, 0)");
        request.onsuccess = function() { evalAndLog("cursor.continue()"); }
        request.onerror = unexpectedErrorCallback;
    } else {
        evalAndLog("cursor.continue()");
    }
}

function reverseCursorComplete()
{
    debug("reverseCursorComplete()");
    finishJSTest();
}
