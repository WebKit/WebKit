if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's objectStore.openCursor + the cursor it produces in depth.");

// In order of how it should be sorted by IndexedDB.
self.testData = [
    2.718281828459,
    3,
    3.14159265,
    10,
    // FIXME: Dates.
    "A bigger string",
    "The biggest",
    "a low string"
];

indexedDBTest(prepareDatabase);
function prepareDatabase()
{
    db = event.target.result;
    event.target.transaction.onabort = unexpectedAbortCallback;

    evalAndLog("objectStore = db.createObjectStore('someObjectStore')");

    debug("");
    debug("Verify that specifying an invalid direction raises an exception:");
    evalAndExpectExceptionClass("objectStore.openCursor(0, 'invalid-direction')", "TypeError");
    debug("");

    self.nextToAdd = 0;
    addData();
}

function addData()
{
    request = evalAndLog("objectStore.add('', testData[nextToAdd])");
    request.onsuccess = ++self.nextToAdd < testData.length ? addData : scheduleTests;
    request.onerror = unexpectedErrorCallback;
}

function scheduleTests()
{
    debug("Scheduling tests...");
    self.scheduledTests = [];
    for (var i = 0; i < testData.length; ++i) {
        /* left bound, is open, right bound, is open, ascending */
        scheduledTests.unshift([i, true, null, null, true]);
        scheduledTests.unshift([i, false, null, null, true]);
        scheduledTests.unshift([null, null, i, true, true]);
        scheduledTests.unshift([null, null, i, false, true]);
        scheduledTests.unshift([i, true, null, null, false]);
        scheduledTests.unshift([i, false, null, null, false]);
        scheduledTests.unshift([null, null, i, true, false]);
        scheduledTests.unshift([null, null, i, false, false]);
        for (var j = 6; j < testData.length; ++j) {
            scheduledTests.unshift([i, true, j, true, true]);
            scheduledTests.unshift([i, true, j, false, true]);
            scheduledTests.unshift([i, false, j, true, true]);
            scheduledTests.unshift([i, false, j, false, true]);
            scheduledTests.unshift([i, true, j, true, false]);
            scheduledTests.unshift([i, true, j, false, false]);
            scheduledTests.unshift([i, false, j, true, false]);
            scheduledTests.unshift([i, false, j, false, false]);
        }
    }

    debug("Running tests...");
    runNextTest();
}

function runNextTest()
{
    if (!scheduledTests.length) {
        testNullKeyRange();
        return;
    }

    var test = scheduledTests.pop();
    self.lower = test[0];
    self.lowerIsOpen = test[1];
    self.upper = test[2];
    self.upperIsOpen = test[3];
    self.ascending = test[4];

    str = "Next test: ";
    if (lower !== null) {
        str += "lower ";
        if (lowerIsOpen)
            str += "open ";
        str += "bound is " + lower + "; ";
    }
    if (upper !== null) {
        str += "upper ";
        if (upperIsOpen)
            str += "open ";
        str += "bound is " + upper + "; ";
    }
    if (ascending)
        str += "sorted ascending.";
    else
        str += "sorted descending.";

    debug("");
    debug(str);

    if (ascending) {
        if (lower !== null) {
            if (!lowerIsOpen)
                self.expectedIndex = lower;
            else
                self.expectedIndex = lower+1;
        } else
            self.expectedIndex = 0;
    } else {
        if (upper !== null) {
            if (!upperIsOpen)
                self.expectedIndex = upper;
            else
                self.expectedIndex = upper-1;
        } else
            self.expectedIndex = testData.length-1;
    }
    testWithinBounds();

    if (testData[lower] === testData[upper] && (lowerIsOpen || upperIsOpen)) {
        debug("Skipping illegal key range.");
        runNextTest();
        return;
    }

    var keyRange;
    if (lower !== null && upper !== null)
        keyRange = IDBKeyRange.bound(testData[lower], testData[upper], lowerIsOpen, upperIsOpen);
    else if (lower !== null)
        keyRange = IDBKeyRange.lowerBound(testData[lower], lowerIsOpen);
    else
        keyRange = IDBKeyRange.upperBound(testData[upper], upperIsOpen);

    var request = objectStore.openCursor(keyRange, ascending ? 'next' : 'prev');
    request.onsuccess = cursorIteration;
    request.onerror = unexpectedErrorCallback;
}

function testWithinBounds()
{
    if (expectedIndex < 0 || testData.length <= expectedIndex)
        self.expectedIndex = null;
    if (lower !== null && expectedIndex < lower)
        self.expectedIndex = null;
    if (upper !== null && upper < expectedIndex)
        self.expectedIndex = null;
    if (lower !== null && lowerIsOpen && expectedIndex <= lower)
        self.expectedIndex = null;
    if (upper !== null && upperIsOpen && upper <= expectedIndex)
        self.expectedIndex = null;
}

function cursorIteration()
{
    if (expectedIndex === null) {
        shouldBeNull("event.target.result");
        runNextTest();
        return;
    }
    if (event.target.result === null) {
        testFailed("event.target.result should not be null.");
        runNextTest();
        return;
    }

    shouldBe("event.target.result.key", "testData[" + expectedIndex + "]");
    self.expectedIndex = ascending ? expectedIndex+1 : expectedIndex-1;
    testWithinBounds();

    event.target.result.continue();
}

self.nullKeyRangeStep = 0;
function testNullKeyRange()
{
    self.lower = 0;
    self.lowerIsOpen = false;
    self.upper = testData.length-1;
    self.upperIsOpen = false;

    str = "Next test: null key path ";
    if (self.nullKeyRangeStep == 0) {
        str += "sorted ascending.";
        self.ascending = true;
        self.expectedIndex = lower;
        self.nullKeyRangeStep = 1;
    } else if (self.nullKeyRangeStep == 1) {
        str += "sorted descending.";
        self.ascending = false;
        self.expectedIndex = upper;
        self.nullKeyRangeStep = 2;
    } else {
        finishJSTest();
        return;
    }

    debug("");
    debug(str);

    var request = objectStore.openCursor(null, ascending ? 'next' : 'prev');
    request.onsuccess = cursorIteration;
    request.onerror = unexpectedErrorCallback;
}
