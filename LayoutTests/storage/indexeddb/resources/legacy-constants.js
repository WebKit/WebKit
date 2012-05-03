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
    shouldBe("trans.mode", "'readwrite'");
    evalAndLog("store = trans.objectStore('store');");
    trans.onerror = unexpectedErrorCallback;
    trans.onabort = unexpectedAbortCallback;

    evalAndLog("store.put({value: 111}, 1)");
    evalAndLog("store.put({value: 222}, 2)");
    trans.oncomplete = checkNext;
}

function checkNext()
{
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
    shouldBe("trans.mode", "'readonly'");
    store = trans.objectStore('store');
    evalAndLog("request = store.openCursor(null, IDBCursor.NEXT)");
    request.onsuccess = function()
    {
        cursor = event.target.result;
        if (!cursor)
            return;
        shouldBe("cursor.direction", "'next'");
        evalAndLog("cursor.continue();");
    };
    trans.oncomplete = checkNextNoDuplicate;
}

function checkNextNoDuplicate()
{
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
    shouldBe("trans.mode", "'readonly'");
    store = trans.objectStore('store');
    evalAndLog("request = store.openCursor(null, IDBCursor.NEXT_NO_DUPLICATE)");
    request.onsuccess = function()
    {
        cursor = event.target.result;
        if (!cursor)
            return;
        shouldBe("cursor.direction", "'nextunique'");
        evalAndLog("cursor.continue();");
    };
    trans.oncomplete = checkPrev;
}

function checkPrev()
{
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
    shouldBe("trans.mode", "'readonly'");
    store = trans.objectStore('store');
    evalAndLog("request = store.openCursor(null, IDBCursor.PREV)");
    request.onsuccess = function()
    {
        cursor = event.target.result;
        if (!cursor)
            return;
        shouldBe("cursor.direction", "'prev'");
        evalAndLog("cursor.continue();");
    };
    trans.oncomplete = checkPrevNoDuplicate;
}

function checkPrevNoDuplicate()
{
    evalAndLog("trans = db.transaction('store', IDBTransaction.READ_ONLY)");
    shouldBe("trans.mode", "'readonly'");
    store = trans.objectStore('store');
    evalAndLog("request = store.openCursor(null, IDBCursor.NEXT)");
    request.onsuccess = function()
    {
        cursor = event.target.result;
        if (!cursor)
            return;
        shouldBe("cursor.direction", "'next'");
        evalAndLog("cursor.continue();");
    };
    trans.oncomplete = finishJSTest;
}


test();