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
            finishJSTest();
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

test();