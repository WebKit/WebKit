if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Exercise optional arguments with missing vs. undefined in IndexedDB methods.");

indexedDBTest(prepareDatabase, checkOptionalArguments);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store', {keyPath: 'id'})");
    evalAndLog("store.createIndex('by_name', 'name', {unique: true})");

    evalAndLog("store.put({id: 1, name: 'a'})");
}

function checkOptionalArguments(event)
{
    evalAndLog("tx = db.transaction('store', 'readwrite')");
    tx.oncomplete = finishJSTest;

    evalAndLog("store = tx.objectStore('store')");
    evalAndLog("index = store.index('by_name')");

    shouldBe("IDBKeyRange.lowerBound(0).lowerOpen", "false");
    shouldBe("IDBKeyRange.upperBound(0).upperOpen", "false");
    shouldBe("IDBKeyRange.bound(0, 1).lowerOpen", "false");
    shouldBe("IDBKeyRange.bound(0, 1).upperOpen", "false");

    shouldBe("IDBKeyRange.lowerBound(0, undefined).lowerOpen", "false");
    shouldBe("IDBKeyRange.upperBound(0, undefined).upperOpen", "false");
    shouldBe("IDBKeyRange.bound(0, 1, undefined, undefined).lowerOpen", "false");
    shouldBe("IDBKeyRange.bound(0, 1, undefined, undefined).upperOpen", "false");

    shouldNotThrow("store.add({id: 2, name: 'b'})");
    shouldNotThrow("store.put({id: 3, name: 'c'})");
    shouldNotThrow("store.add({id: 4, name: 'd'}, undefined)");
    shouldNotThrow("store.put({id: 5, name: 'e'}, undefined)");

    tasks = [
        function(callback) { verifyCursor("store.openCursor()", "next", 5, callback); },
        function(callback) { verifyCursor("store.openCursor(null)", "next", 5, callback); },
        function(callback) { verifyCursor("store.openCursor(IDBKeyRange.lowerBound(4))", "next", 2, callback); },
        function(callback) { verifyCursor("store.openCursor(3)", "next", 1, callback); },

        function(callback) { verifyCursor("index.openCursor()", "next", 5, callback); },
        function(callback) { verifyCursor("index.openCursor(null)", "next", 5, callback); },
        function(callback) { verifyCursor("index.openCursor(IDBKeyRange.lowerBound('b'))", "next", 4, callback); },
        function(callback) { verifyCursor("index.openCursor('c')", "next", 1, callback); },

        function(callback) { verifyCursor("index.openKeyCursor()", "next", 5, callback); },
        function(callback) { verifyCursor("index.openKeyCursor(null)", "next", 5, callback); },
        function(callback) { verifyCursor("index.openKeyCursor(IDBKeyRange.lowerBound('b'))", "next", 4, callback); },
        function(callback) { verifyCursor("index.openKeyCursor('c')", "next", 1, callback); },

        function(callback) { verifyCount("store.count()", 5, callback); },
        function(callback) { verifyCount("store.count(null)", 5, callback); },
        function(callback) { verifyCount("store.count(IDBKeyRange.lowerBound(2))", 4, callback); },

        function(callback) { verifyCount("index.count()", 5, callback); },
        function(callback) { verifyCount("index.count(null)", 5, callback); },
        function(callback) { verifyCount("index.count(IDBKeyRange.lowerBound('b'))", 4, callback); },

        continueUndefined,

    ];
    function doNextTask() {
        var task = tasks.shift();
        if (task) {
            task(doNextTask);
        }
    }
    doNextTask();
}

function verifyCursor(expr, direction, expected, callback)
{
    preamble();
    cursor = null;
    continues = 0;
    evalAndLog("request = " + expr);
    request.onerror = unexpectedErrorCallback;

    request.onsuccess = function() {
        if (request.result) {
            if (!cursor) {
                evalAndLog("cursor = request.result");
                shouldBeEqualToString("cursor.direction", direction);
            }
            ++continues;
            cursor.continue();
        } else {
            shouldBe("continues", JSON.stringify(expected));
            callback();
        }
    };
}

function verifyCount(expr, expected, callback)
{
    preamble();
    evalAndLog("request = " + expr);
    request.onerror = unexpectedErrorCallback;

    request.onsuccess = function() {
        shouldBe("request.result", JSON.stringify(expected));
        callback();
    };
}

function continueUndefined(callback)
{
    preamble();
    first = true;
    evalAndLog("request = store.openCursor()");
    request.onerror = unexpectedErrorCallback;

    request.onsuccess = function() {
        if (first) {
            first = false;
            evalAndLog("cursor = request.result");
            shouldBeNonNull("request.result");
            shouldNotThrow("cursor.continue(undefined)");
            callback();
        }
    };
}
