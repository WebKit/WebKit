if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that -0 and +0 are treated as equal keys in a unique index.");

indexedDBTest(prepareDatabase, onOpenSuccess);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    self.store = evalAndLog("db.createObjectStore('store')");
    self.indexObject = evalAndLog("store.createIndex('index', 'v', {unique: true})");
}

function onOpenSuccess()
{
    debug("onOpenSuccess():");
    self.transaction = evalAndLog("transaction = db.transaction(['store'], 'readwrite')");
    transaction.onabort = function() {
        debug("transaction.onabort:");
        shouldBe("transaction.error.name", "'ConstraintError'");
        testCount();
    };

    // Insert a record whose indexed value is -0.
    request = evalAndLog("transaction.objectStore('store').put({v: -0}, 'first')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = insertSecondRecord;
}

function insertSecondRecord()
{
    debug("insertSecondRecord():");

    // Insert a record whose indexed value is +0. Because -0 and +0 are equal
    // per the IndexedDB key comparison algorithm, this must violate the unique
    // constraint.
    request = evalAndLog("transaction.objectStore('store').put({v: 0}, 'second')");
    request.onerror = secondRecordFailed;
    request.onsuccess = unexpectedSuccessCallback;
}

function secondRecordFailed()
{
    debug("secondRecordFailed():");
    shouldBe("event.target.error.name", "'ConstraintError'");
    evalAndLog("event.preventDefault()");
    testCount();
}

function testCount()
{
    debug("testCount():");
    // Verify only one record exists.
    self.transaction2 = evalAndLog("transaction2 = db.transaction(['store'], 'readonly')");
    request = evalAndLog("transaction2.objectStore('store').count()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        debug("countSuccess():");
        shouldBe("event.target.result", "1");
        testCursor();
    };
}

function testCursor()
{
    debug("testCursor():");
    // Open a cursor on the index and verify it yields exactly one entry.
    self.cursorCount = 0;
    request = evalAndLog("transaction2.objectStore('store').index('index').openCursor()");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        if (event.target.result) {
            cursorCount++;
            event.target.result.continue();
        } else {
            shouldBe("cursorCount", "1");
            finishJSTest();
        }
    };
}
