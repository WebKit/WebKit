if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that legacy direction/mode constants work");

function test()
{
    removeVendorPrefixes();
    prepareDatabase();
}

function prepareDatabase()
{
    debug("");
    evalAndLog("openreq = indexedDB.open('legacy-constants')");
    openreq.onerror = unexpectedErrorCallback;
    openreq.onsuccess = function()
    {
        evalAndLog("db = openreq.result");
        evalAndLog("verreq = db.setVersion('1')");
        verreq.onerror = unexpectedErrorCallback;
        verreq.onsuccess = function()
        {
            deleteAllObjectStores(db);
            nstore = evalAndLog("store = db.createObjectStore('store')");
            evalAndLog("store.createIndex('index', 'value')");
            verreq.result.oncomplete = populateStore;
        };
    };
}
function populateStore()
{
    debug("");
    debug("populating store...");
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_WRITE)");
    shouldBeEqualToString("trans.mode", "readwrite");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("store.put({value: 111}, 1)");
    evalAndLog("store.put({value: 222}, 2)");
    trans.oncomplete = doChecks;
}

function doChecks()
{
    var tests = [
        { legacy: 'IDBCursor.NEXT', current: 'next' },
        { legacy: 'IDBCursor.NEXT_NO_DUPLICATE', current: 'nextunique' },
        { legacy: 'IDBCursor.PREV', current: 'prev' },
        { legacy: 'IDBCursor.PREV_NO_DUPLICATE', current: 'prevunique' }
    ];

    function doNext()
    {
        var test = tests.shift();
        if (!test) {
            testObsoleteConstants();
            return;
        }

        evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
        shouldBeEqualToString("trans.mode", "readonly");
        store = trans.objectStore('store');

        evalAndLog("request = store.openCursor(null, " + test.legacy + ")");
        request.onsuccess = function()
        {
            cursor = event.target.result;
            if (!cursor) {
                testWithKey();
                return;
            }
            shouldBeEqualToString("cursor.direction", test.current);
            evalAndLog("cursor.continue();");
        };

        function testWithKey()
        {
            evalAndLog("request = store.openCursor(1, " + test.legacy + ")");
            request.onsuccess = function()
            {
                cursor = event.target.result;
                if (!cursor)
                    return;
                shouldBeEqualToString("cursor.direction", test.current);
                evalAndLog("cursor.continue();");
            };
        }


        trans.oncomplete = doNext;
    }
    doNext();
}

function testObsoleteConstants()
{
    debug("");
    debug("Verify that constants from previous version of the spec (beyond a grace period) have been removed:");

    // http://www.w3.org/TR/2010/WD-IndexedDB-20100819/
    shouldBe("IDBKeyRange.SINGLE", "undefined");
    shouldBe("IDBKeyRange.LEFT_OPEN", "undefined");
    shouldBe("IDBKeyRange.RIGHT_OPEN", "undefined");
    shouldBe("IDBKeyRange.LEFT_BOUND", "undefined");
    shouldBe("IDBKeyRange.RIGHT_BOUND", "undefined");

    // Unclear that this was ever in the spec, but it was present in mozilla tests:
    shouldBe("IDBTransaction.LOADING", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    shouldBe("IDBRequest.LOADING", "undefined");
    shouldBe("IDBRequest.DONE", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    // FIXME: Add when grace period expires: http://webkit.org/b/85315
    //shouldBe("IDBCursor.NEXT", "undefined");
    //shouldBe("IDBCursor.NEXT_NO_DUPLICATE", "undefined");
    //shouldBe("IDBCursor.PREV", "undefined");
    //shouldBe("IDBCursor.PREV_NO_DUPLICATE", "undefined");

    // http://www.w3.org/TR/2011/WD-IndexedDB-20111206/
    // FIXME: Add when grace period expires: http://webkit.org/b/85315
    //shouldBe("IDBTransaction.READ_ONLY", "undefined");
    //shouldBe("IDBTransaction.READ_WRITE", "undefined");
    //shouldBe("IDBTransaction.VERSION_CHANGE", "undefined");

    finishJSTest();
}

test();
