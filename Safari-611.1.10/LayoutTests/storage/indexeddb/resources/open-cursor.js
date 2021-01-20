if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's openCursor.");


function emptyCursorWithKeySuccess()
{
    debug("Empty cursor opened successfully.");
    cursor = event.target.result;
    shouldBeNull("cursor");
    finishJSTest();
}

function openEmptyCursorWithKey()
{
    debug("Opening an empty cursor.");
    request = evalAndLog("objectStore.openCursor(\"InexistentKey\")");
    request.onsuccess = emptyCursorWithKeySuccess;
    request.onerror = unexpectedErrorCallback;
}

function cursorWithKeySuccess()
{
    debug("Cursor opened successfully.");
    // FIXME: check that we can iterate the cursor.
    cursor = event.target.result;
    shouldBeEqualToString("cursor.direction", "next");
    shouldBeEqualToString("cursor.key", "myKey");
    shouldBeEqualToString("cursor.value", "myValue");
    debug("");
    debug("Passing an invalid key into .continue({}).");
    evalAndExpectException("cursor.continue({})", "0", "'DataError'");
    debug("");
    openEmptyCursorWithKey();
}

function openCursorWithKey()
{
    debug("Opening cursor");
    request = evalAndLog("event.target.source.openCursor(\"myKey\")");
    request.onsuccess = cursorWithKeySuccess;
    request.onerror = unexpectedErrorCallback;
}

function emptyCursorSuccess()
{
    debug("Empty cursor opened successfully.");
    cursor = event.target.result;
    shouldBeNull("cursor");
    openCursorWithKey();
}

function openEmptyCursor()
{
    debug("Opening an empty cursor.");
    keyRange = IDBKeyRange.upperBound("InexistentKey");
    request = evalAndLog("objectStore.openCursor(keyRange)");
    request.onsuccess = emptyCursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

function cursorSuccess()
{
    debug("Cursor opened successfully.");
    // FIXME: check that we can iterate the cursor.
    cursor = event.target.result;
    shouldBeEqualToString("cursor.direction", "next");
    shouldBeEqualToString("cursor.key", "myKey");
    shouldBeEqualToString("cursor.value", "myValue");
    debug("");
    debug("Passing an invalid key into .continue({}).");
    evalAndExpectException("event.target.result.continue({})", "0", "'DataError'");
    debug("");
    openEmptyCursor();
}

function openCursor()
{
    debug("Opening cursor");
    keyRange = IDBKeyRange.lowerBound("myKey");
    request = evalAndLog("event.target.source.openCursor(keyRange)");
    request.onsuccess = cursorSuccess;
    request.onerror = unexpectedErrorCallback;
}

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;
    var objectStore = evalAndLog("objectStore = db.createObjectStore('test')");
    request = evalAndLog("objectStore.add('myValue', 'myKey')");
    request.onsuccess = openCursor;
    request.onerror = unexpectedErrorCallback;
}
