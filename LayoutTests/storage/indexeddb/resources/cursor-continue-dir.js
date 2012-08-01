if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that continue() calls against cursors are validated by direction.");

function test()
{
    removeVendorPrefixes();
    evalAndLog("dbname = self.location.pathname");
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = function() {
        evalAndLog("request = indexedDB.open(dbname)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            evalAndLog("db = request.result");
            request = evalAndLog("db.setVersion('1')");
            request.onerror = unexpectedErrorCallback;
            request.onblocked = unexpectedBlockedCallback;
            request.onsuccess = function() {
                trans = request.result;
                trans.onabort = unexpectedAbortCallback;

                evalAndLog("store = db.createObjectStore('store')");
                for (i = 1; i <= 10; ++i) {
                    evalAndLog("store.put(" + i + "," + i + ")");
                }

                trans.oncomplete = testCursors;
            };
        };
    };
}

function testCursors()
{
    evalAndLog("trans = db.transaction('store')");
    evalAndLog("store = trans.objectStore('store')");
    testForwardCursor();
}

function testForwardCursor()
{
    evalAndLog("request = store.openCursor(IDBKeyRange.bound(-Infinity, Infinity), 'next')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBeNonNull("cursor");
        debug("Expect DataError if: The parameter is less than or equal to this cursor's position and this cursor's direction is \"next\" or \"nextunique\".");
        shouldBe("cursor.key", "1");
        evalAndExpectException("cursor.continue(-1)", "IDBDatabaseException.DATA_ERR", "'DataError'");

        testReverseCursor();
    };
}

function testReverseCursor()
{
    evalAndLog("request = store.openCursor(IDBKeyRange.bound(-Infinity, Infinity), 'prev')");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function() {
        evalAndLog("cursor = request.result");
        shouldBeNonNull("cursor");
        debug("Expect DataError if: The parameter is greater than or equal to this cursor's position and this cursor's direction is \"prev\" or \"prevunique\".");
        shouldBe("cursor.key", "10");
        evalAndExpectException("cursor.continue(11)", "IDBDatabaseException.DATA_ERR", "'DataError'");

        finishJSTest();
    };
}

test();
