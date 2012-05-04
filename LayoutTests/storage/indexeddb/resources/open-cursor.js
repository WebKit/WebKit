if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
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
    shouldBe("cursor.direction", "0");
    shouldBe("cursor.key", "'myKey'");
    shouldBe("cursor.value", "'myValue'");
    debug("");
    debug("Passing an invalid key into .continue({}).");
    evalAndExpectException("cursor.continue({})", "IDBDatabaseException.DATA_ERR");
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
    shouldBe("cursor.direction", "0");
    shouldBe("cursor.key", "'myKey'");
    shouldBe("cursor.value", "'myValue'");
    debug("");
    debug("Passing an invalid key into .continue({}).");
    evalAndExpectException("event.target.result.continue({})", "IDBDatabaseException.DATA_ERR");
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

function setVersionSuccess()
{
    debug("setVersionSuccess():");
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

    var objectStore = evalAndLog("objectStore = db.createObjectStore('test')");
    request = evalAndLog("objectStore.add('myValue', 'myKey')");
    request.onsuccess = openCursor;
    request.onerror = unexpectedErrorCallback;
}

function openSuccess()
{
    var db = evalAndLog("db = event.target.result");

    request = evalAndLog("db.setVersion('new version')");
    request.onsuccess = setVersionSuccess;
    request.onerror = unexpectedErrorCallback;
}

function test()
{
    removeVendorPrefixes();
    request = evalAndLog("indexedDB.open('open-cursor')");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
}

test();