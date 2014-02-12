if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test IndexedDB's IDBCursor.continue() with a primary key parameter.");

indexedDBTest(prepareDatabase, verifyContinueCalls);
function prepareDatabase(evt)
{
    preamble(evt);

    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("index = store.createIndex('index', 'indexKey', {multiEntry: true})");

    evalAndLog("store.put({indexKey: ['a', 'b']}, 1)");
    evalAndLog("store.put({indexKey: ['a', 'b']}, 2)");
    evalAndLog("store.put({indexKey: ['a', 'b']}, 3)");
    evalAndLog("store.put({indexKey: ['b']}, 4)");

    var indexExpected = [
        {key: "a", primaryKey: 1},
        {key: "a", primaryKey: 2},
        {key: "a", primaryKey: 3},
        {key: "b", primaryKey: 1},
        {key: "b", primaryKey: 2},
        {key: "b", primaryKey: 3},
        {key: "b", primaryKey: 4}
    ];
    debug("checking index structure...");
    debug("");
    debug("index key  primary key");
    debug("=========  ===========");
    var quiet = true;
    evalAndLog("request = index.openCursor()", quiet);
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function onCursorSuccess() {
        evalAndLog("cursor = request.result", quiet);
        var expectedEntry = indexExpected.shift();
        if (expectedEntry) {
            shouldBe("cursor.key", JSON.stringify(expectedEntry.key), quiet);
            shouldBe("cursor.primaryKey", JSON.stringify(expectedEntry.primaryKey), quiet);
            debug(cursor.key + "          " + cursor.primaryKey);
            evalAndLog("cursor.continue()", quiet);
        } else {
            shouldBeNull("cursor", quiet);
        }
    };
}

var testCases = [
    // Continuing index key
    { call: "cursor.continue()", result: { key: "a", primaryKey: 2 } },
    { call: "cursor.continue('a')", exception: 'DataError' },
    { call: "cursor.continue('b')", result: { key: "b", primaryKey: 1 } },
    { call: "cursor.continue('c')", result: null },

    // Called w/ index key and primary key:
    { call: "cursor.continuePrimaryKey('a', 3)", result: {key: 'a', primaryKey: 3} },
    { call: "cursor.continuePrimaryKey('a', 4)", result: {key: 'b', primaryKey: 1} },
    { call: "cursor.continuePrimaryKey('b', 1)", result: {key: 'b', primaryKey: 1} },
    { call: "cursor.continuePrimaryKey('b', 4)", result: {key: 'b', primaryKey: 4} },
    { call: "cursor.continuePrimaryKey('b', 5)", result: null },
    { call: "cursor.continuePrimaryKey('c', 1)", result: null },

    // Called w/ primary key but w/o index key
    { call: "cursor.continuePrimaryKey(null, 1)", exception: 'DataError' },
    { call: "cursor.continuePrimaryKey(null, 2)", exception: 'DataError' },
    { call: "cursor.continuePrimaryKey(null, 3)", exception: 'DataError' },
    { call: "cursor.continuePrimaryKey(null, 4)", exception: 'DataError' },
    { call: "cursor.continuePrimaryKey(null, 5)", exception: 'DataError' },

    // Called w/ index key but w/o primary key
    { call: "cursor.continuePrimaryKey('a', null)", exception: 'DataError' },
 ];

function verifyContinueCalls() {
    debug("");
    if (!testCases.length) {
        finishJSTest();
        return;
    }

    var quiet = true;
    testCase = testCases.shift();
    debug("Test case: " + testCase.call);
    debug("");
    evalAndLog("tx = db.transaction('store')");
    evalAndLog("request = tx.objectStore('store').index('index').openCursor()");
    var i = 0;
    request.onsuccess = function() {
        ++i;
        evalAndLog("cursor = request.result", true);
        if (i === 1) {
            if ('exception' in testCase) {
                evalAndExpectException(testCase.call, "0", "'DataError'");
            } else {
                evalAndLog(testCase.call);
            }
            return;
        }

        if (i === 2) {
            if (testCase.result) {
                shouldBe("cursor.key", JSON.stringify(testCase.result.key));
                shouldBe("cursor.primaryKey", JSON.stringify(testCase.result.primaryKey));
            } else {
                shouldBeNull("cursor");
            }
        }
    };

    tx.oncomplete = verifyContinueCalls;
}
