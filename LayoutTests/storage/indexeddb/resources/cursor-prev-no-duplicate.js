if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB behavior when iterating backwards with and without NO_DUPLICATE");

function test()
{
    removeVendorPrefixes();

    prepareDatabase();
}

function prepareDatabase()
{
    debug("");
    evalAndLog("openreq = indexedDB.open('cursor-prev-no-duplicate')");
    openreq.onerror = unexpectedErrorCallback;
    openreq.onsuccess = function()
    {
        evalAndLog("db = openreq.result");
        evalAndLog("verreq = db.setVersion('1')");
        verreq.onerror = unexpectedErrorCallback;
        verreq.onsuccess = function()
        {
            deleteAllObjectStores(db);
            store = evalAndLog("store = db.createObjectStore('store')");
            evalAndLog("store.createIndex('index', 'sorted')");
            verreq.result.oncomplete = populateStore;
        };
    };
}

function populateStore()
{
    debug("");
    debug("populating store...");
    evalAndLog("trans = db.transaction('store', 'readwrite')");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("store.put({sorted: 3, value: 111}, 1)");
    evalAndLog("store.put({sorted: 2, value: 222}, 2)");
    evalAndLog("store.put({sorted: 1, value: 333}, 3)");
    evalAndLog("store.put({sorted: 10, value: 444}, 17)");
    evalAndLog("store.put({sorted: 10, value: 555}, 16)");
    evalAndLog("store.put({sorted: 10, value: 666}, 15)");
    trans.oncomplete = testFarRangeCursor_closed;
}


function testFarRangeCursor_closed()
{
    debug("testFarRangeCursor: upper bound is well out of range, results always the same, whether open or closed");

    runTest(makeOpenCursor("store", 7, false, "'prev'"),
            { expectedValue: 333, expectedKey: 3}).onsuccess =
                testFarRangeCursor_open;
}

function testFarRangeCursor_open()
{
    runTest(makeOpenCursor("store", 7, true, "'prev'"),
            { expectedValue: 333, expectedKey: 3}).onsuccess =
                testFarRangeCursor_indexClosed;
}

function testFarRangeCursor_indexClosed()
{
        // here '7' refers to the 'sorted' value
        runTest(makeOpenCursor("index", 7, false, "'prev'"),
                { expectedValue: 111, expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                    testFarRangeCursor_indexOpen;
}
function testFarRangeCursor_indexOpen()
{
    runTest(makeOpenCursor("index", 7, true, "'prev'"),
            { expectedValue: 111, expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                testFarRangeCursor_indexKeyOpen;
}

function testFarRangeCursor_indexKeyOpen()
{
    // here '7' refers to the sorted value
    runTest(makeOpenKeyCursor("index", 7, false, "'prev'"),
            { expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                testFarRangeCursor_indexKeyClosed;
}

function testFarRangeCursor_indexKeyClosed()
{
    runTest(makeOpenKeyCursor("index", 7, true, "'prev'"),
            { expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                testBoundaryCursor_closed;
}

function testBoundaryCursor_closed()
{
    runTest(makeOpenCursor("store", 3, false, "'prev'"),
            { expectedValue: 333, expectedKey: 3}).onsuccess =
                testBoundaryCursor_open;
};

function testBoundaryCursor_open()
{
    runTest(makeOpenCursor("store", 3, true, "'prev'"),
            { expectedValue: 222, expectedKey: 2}).onsuccess =
                testBoundaryCursor_indexClosed;
}

function testBoundaryCursor_indexClosed()
{
    // by index sort order, we should return them in a different order
    runTest(makeOpenCursor("index", 3, false, "'prev'"),
            { expectedValue: 111, expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                testBoundaryCursor_indexOpen;
}

function testBoundaryCursor_indexOpen()
{
    runTest(makeOpenCursor("index", 3, true, "'prev'"),
            { expectedValue: 222, expectedKey: 2, expectedPrimaryKey: 2}).onsuccess =
                testBoundaryCursor_indexKeyClosed;
}

function testBoundaryCursor_indexKeyClosed()
{

    // now the value doesn't matter, just the primary key
    runTest(makeOpenKeyCursor("index", 3, false, "'prev'"),
            { expectedKey: 3, expectedPrimaryKey: 1}).onsuccess =
                testBoundaryCursor_indexKeyOpen;
}

function testBoundaryCursor_indexKeyOpen()
{
    runTest(makeOpenKeyCursor("index", 3, true, "'prev'"),
            { expectedKey: 2, expectedPrimaryKey: 2}).onsuccess =
                testNoDuplicate_closed;
}

function testNoDuplicate_closed()
{
    debug("testNoDuplicate: there are 3 values, but we should return always the first one");

    // PREV_NO_DUPLICATE doesn't really affect non-indexed
    // cursors, but we should make sure we get the right one
    // anyway
    runTest(makeOpenCursor("store", 15, false, "'prevunique'"),
            { expectedValue: 666, expectedKey: 15, expectedPrimaryKey: 15 })
                .onsuccess = testNoDuplicate_open;
}

function testNoDuplicate_open()
{
    // still three values, but now the index says we should return the
    // second one
    runTest(makeOpenCursor("index", 15, false, "'prevunique'"),
            { expectedValue: 666, expectedKey: 10, expectedPrimaryKey: 15}).onsuccess = testNoDuplicate_indexKeyClosed;
}


function testNoDuplicate_indexKeyClosed()
{
    // same behavior as above, without a value
    runTest(makeOpenKeyCursor("index", 15, false, "'prevunique'"),
            { expectedKey: 10, expectedPrimaryKey: 15}).onsuccess =
                finishJSTest;
}


function makeOpenCursor(obj, upperBound, open, direction)
{
    return obj + ".openCursor(IDBKeyRange.upperBound(" + upperBound + ", " +
        open + "), " +
            direction + ")";
}

function makeOpenKeyCursor(obj, upperBound, open, direction)
{
    return obj + ".openKeyCursor(IDBKeyRange.upperBound(" + upperBound + ", " +
        open + "), " +
            direction + ")";
}

function runTest(openCursor, exp)
{
    trans = db.transaction('store', 'readonly');

    // expose these for code in openCursor
    store = trans.objectStore('store');
    index = store.index('index');
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;
    trans.oncomplete = function()
    {
        debug("DONE");
    };

    var result = {};

    var testFunction = function()
    {
        // expose for evalAndLog
        expectation = exp;

        debug("\nTesting: " + openCursor);
        cursor = event.target.result;
        if (cursor === null) {
            testFailed("null cursor");
            return;
        }

        shouldBe("cursor.key", JSON.stringify(expectation.expectedKey));
        if ("value" in cursor) {
            shouldBe("cursor.value.value", JSON.stringify(expectation.expectedValue));
        }
        else if ("expectedValue" in expectation)
            testFailed("Test broken: shouldn't have expectedValue");

        if ("expectedPrimaryKey" in expectation) {
            shouldBe("cursor.primaryKey", JSON.stringify(expectation.expectedPrimaryKey));
        }

        result.onsuccess();
    };

    storeReq = evalAndLog("storeReq = " + openCursor);
    storeReq.onsuccess = testFunction;

    return result;
}

test();