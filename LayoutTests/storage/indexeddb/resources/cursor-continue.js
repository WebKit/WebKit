if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBCursor.continue() with a key parameter.");

var date = new Date();

// In order of how it should be sorted by IndexedDB.
self.testData = [
    0,
    1,
    1,
    3.14159,
    3.14159,
    10,
    11,
    12,
    13,
    date,
    date,
    new Date(date.valueOf() + 1000),
    "A big string",
    "A big string",
    "A bit1",
    "A bit2",
    "A bit3",
    "the BIGGEST string"
];

function test()
{
    removeVendorPrefixes();
    openDatabase();
}

function openDatabase()
{
    result = evalAndLog("indexedDB.open('cursor-continue')");
    result.onsuccess = setVersion;
    result.onerror = unexpectedErrorCallback;
}

function setVersion()
{
    self.db = evalAndLog("db = event.target.result");

    result = evalAndLog("db.setVersion('new version')");
    result.onsuccess = deleteExisting;
    result.onerror = unexpectedErrorCallback;
}

function deleteExisting()
{
    self.trans = evalAndLog("trans = event.target.result");
    shouldBeTrue("trans !== null");
    trans.onabort = unexpectedAbortCallback;

    deleteAllObjectStores(db);

    self.objectStore = evalAndLog("db.createObjectStore('someObjectStore')");
    self.indexObject = evalAndLog("objectStore.createIndex('someIndex', 'x')");
    self.nextToAdd = 0;
    addData();
}

function addData()
{
    result = evalAndLog("objectStore.add({'x': testData[nextToAdd]}, nextToAdd)");
    result.onsuccess = ++self.nextToAdd < testData.length ? addData : ascendingTest;
    result.onerror = unexpectedErrorCallback;
}

function ascendingTest()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.NEXT)");
    request.onsuccess = ascendingCallback;
    request.onerror = unexpectedErrorCallback;
    self.stage = 0;
}

function ascendingCallback()

{
    if (self.stage == 0) {
        shouldBe("event.target.result.primaryKey", "0");
        evalAndLog("event.target.result.continue(3.14159)");
    } else if (self.stage == 1) {
        shouldBe("event.target.result.primaryKey", "3");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 2) {
        shouldBe("event.target.result.primaryKey", "4");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 3) {
        shouldBe("event.target.result.primaryKey", "5");
        evalAndLog("event.target.result.continue(12)");
    } else if (self.stage == 4) {
        shouldBe("event.target.result.primaryKey", "7");
        evalAndLog("event.target.result.continue(date)");
    } else if (self.stage == 5) {
        shouldBe("event.target.result.primaryKey", "9");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 6) {
        shouldBe("event.target.result.primaryKey", "10");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 7) {
        shouldBe("event.target.result.primaryKey", "11");
        evalAndLog("event.target.result.continue('A bit1')");
    } else if (self.stage == 8) {
        shouldBe("event.target.result.primaryKey", "14");
        evalAndLog("event.target.result.continue('A bit3')");
    } else if (self.stage == 9) {
        shouldBe("event.target.result.primaryKey", "16");
        evalAndLog("event.target.result.continue('the BIGGEST string')");
    } else if (self.stage == 10) {
        shouldBe("event.target.result.primaryKey", "17");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 11) {
        shouldBeNull("event.target.result");
        descendingTest();
        return;
    } else {
        testFailed("Illegal stage.");
    }
    self.stage++;
}

function descendingTest()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.PREV)");
    request.onsuccess = descendingCallback;
    request.onerror = unexpectedErrorCallback;
    self.stage = 0;
}

function descendingCallback()
{
    if (self.stage == 0) {
        shouldBe("event.target.result.primaryKey", "17");
        evalAndLog("event.target.result.continue('A bit2')");
    } else if (self.stage == 1) {
        shouldBe("event.target.result.primaryKey", "15");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 2) {
        shouldBe("event.target.result.primaryKey", "14");
        evalAndLog("event.target.result.continue(date)");
    } else if (self.stage == 3) {
        shouldBe("event.target.result.primaryKey", "10");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 4) {
        shouldBe("event.target.result.primaryKey", "9");
        evalAndLog("event.target.result.continue(1)");
    } else if (self.stage == 5) {
        shouldBe("event.target.result.primaryKey", "2");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 6) {
        shouldBe("event.target.result.primaryKey", "1");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 7) {
        shouldBe("event.target.result.primaryKey", "0");
        evalAndLog("event.target.result.continue()");
    } else if (self.stage == 8) {
        shouldBeNull("event.target.result");
        ascendingErrorTestLessThan();
        return;
    } else {
        testFailed("Illegal stage.");
    }
    self.stage++;
}

function ascendingErrorTestLessThan()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.NEXT)");
    self.stage = 0;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        if (self.stage === 0) {
            shouldBe("event.target.result.primaryKey", "0");
            evalAndLog("event.target.result.continue(3.14159)");
        } else if (self.stage == 1) {
            shouldBe("event.target.result.primaryKey", "3");
            evalAndExpectException("event.target.result.continue(1)", "IDBDatabaseException.DATA_ERR");
            ascendingErrorTestEqual();
            return;
        } else {
           testFailed("Illegal stage.");
        }
        self.stage++;
    };
}

function ascendingErrorTestEqual()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.NEXT)");
    self.stage = 0;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        if (self.stage === 0) {
            shouldBe("event.target.result.primaryKey", "0");
            evalAndLog("event.target.result.continue(3.14159)");
        } else if (self.stage == 1) {
            shouldBe("event.target.result.primaryKey", "3");
            evalAndExpectException("event.target.result.continue(3.14159)", "IDBDatabaseException.DATA_ERR");
            descendingErrorTestGreaterThan();
            return;
        } else {
           testFailed("Illegal stage.");
        }
        self.stage++;
    };
}

function descendingErrorTestGreaterThan()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.PREV)");
    self.stage = 0;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        if (self.stage == 0) {
            shouldBe("event.target.result.primaryKey", "17");
            evalAndLog("event.target.result.continue('A bit2')");
        } else if (self.stage == 1) {
            shouldBe("event.target.result.primaryKey", "15");
            evalAndExpectException("event.target.result.continue('A bit3')", "IDBDatabaseException.DATA_ERR");
            descendingErrorTestEqual();
            return;
        } else {
           testFailed("Illegal stage.");
        }
        self.stage++;
    };
}

function descendingErrorTestEqual()
{
    debug("");
    var request = evalAndLog("indexObject.openKeyCursor(null, IDBCursor.PREV)");
    self.stage = 0;
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        if (self.stage == 0) {
            shouldBe("event.target.result.primaryKey", "17");
            evalAndLog("event.target.result.continue('A bit2')");
        } else if (self.stage == 1) {
            shouldBe("event.target.result.primaryKey", "15");
            evalAndLog("cursor = event.target.result");
            evalAndExpectException("event.target.result.continue('A bit2')", "IDBDatabaseException.DATA_ERR");
            self.trans.oncomplete = onTransactionComplete;
            return;
        } else {
           testFailed("Illegal stage.");
        }
        self.stage++;
    };
}

function onTransactionComplete()
{
    evalAndExpectException("cursor.continue()", "IDBDatabaseException.TRANSACTION_INACTIVE_ERR");
    finishJSTest();
}

test();
