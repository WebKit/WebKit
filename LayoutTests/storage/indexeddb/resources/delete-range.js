if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBObjectStore.delete(IDBKeyRange) method.");

indexedDBTest(prepareDatabase, runTests);
function prepareDatabase()
{
    db = event.target.result;
    evalAndLog("db.createObjectStore('store')");
}

function checkKeys(expected, callback)
{
    debug("getting keys from store...");
    debug("expect: " + JSON.stringify(expected));
    var keys = [];
    request = store.openCursor();
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function () {
        cursor = request.result;
        if (cursor) {
            keys.push(cursor.key);
            cursor.continue();
        } else {
            debug("actual: " + JSON.stringify(keys));
            if (JSON.stringify(expected) === JSON.stringify(keys)) {
                testPassed("Match!");
            } else {
                testFailed("Don't match!");
            }
        }
    };
}

var tests = [
    { lower: 3, upper: 8, lowerOpen: false, upperOpen: false, expected: [1, 2, 9, 10]},
    { lower: 3, upper: 8, lowerOpen: true, upperOpen: false, expected: [1, 2, 3, 9, 10]},
    { lower: 3, upper: 8, lowerOpen: false, upperOpen: true, expected: [1, 2, 8, 9, 10]},
    { lower: 3, upper: 8, lowerOpen: true, upperOpen: true, expected: [1, 2, 3, 8, 9, 10]}
];

function runTests()
{
    function nextTest() {
        var test = tests.shift();
        if (test) {
            debug("");
            evalAndLog("trans = db.transaction('store', 'readwrite')");
            evalAndLog("store = trans.objectStore('store')");
            for (i = 1; i <= 10; ++i) {
                evalAndLog("store.put(" + i + "," + i + ")");
            }
            evalAndLog("store.delete(IDBKeyRange.bound(" +
                test.lower + ", " +
                test.upper + ", " +
                test.lowerOpen + ", " +
                test.upperOpen + "))");
            checkKeys(test.expected, nextTest);
            trans.onabort = unexpectedAbortCallback;
            trans.oncomplete = nextTest;
        } else {
            finishJSTest();
            return;
        }
    }
    nextTest();
}
