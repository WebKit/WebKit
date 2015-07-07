if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check that transactions in different databases can run in parallel.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    evalAndLog("dbname1 = dbname + '1'");
    evalAndLog("dbname2 = dbname + '2'");

    deleteDatabase1();
}

function deleteDatabase1()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = deleteDatabase2;
}

function deleteDatabase2()
{
    preamble();
    request = evalAndLog("indexedDB.deleteDatabase(dbname2)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = openDatabase1;
}

function openDatabase1()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname1, 1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function openOnUpgradeNeeded1(evt) {
        preamble(evt);
        evalAndLog("db1 = event.target.result");
        evalAndLog("store1 = db1.createObjectStore('store')");
        evalAndLog("store1.put(0, 0)");
    };
    request.onsuccess = function openOnSuccess1(evt) {
        preamble(evt);
        evalAndLog("db1 = event.target.result");
        openDatabase2();
    };
}

function openDatabase2()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname2, 1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function onUpgradeNeeded2(evt) {
        preamble(evt);
        evalAndLog("db2 = event.target.result");
        evalAndLog("store2 = db2.createObjectStore('store')");
    };
    request.onsuccess = function openOnSuccess2(evt) {
        preamble(evt);
        evalAndLog("db2 = event.target.result");
        startWork();
    };
}

function startWork()
{
    preamble();
    evalAndLog("transaction1 = db1.transaction('store', 'readwrite')");
    transaction1.onabort = unexpectedAbortCallback;
    transaction1.oncomplete = onTransactionComplete;
    evalAndLog("transaction2 = db2.transaction('store', 'readwrite')");
    transaction1.onabort = unexpectedAbortCallback;
    transaction2.oncomplete = onTransactionComplete;

    evalAndLog("transaction1PutSuccess = false");
    evalAndLog("transaction2PutSuccess = false");

    debug("Keep both transactions alive until each has reported at least one successful operation");

    function doTransaction1Put() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction1.objectStore('store').put(0, 0);
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            transaction1PutSuccess = true;
            if (!transaction2PutSuccess)
                doTransaction1Put();
        };
    }

    function doTransaction2Put() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction2.objectStore('store').put(0, 0);
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            transaction2PutSuccess = true;
            if (!transaction1PutSuccess)
                doTransaction2Put();
        };
    }

    doTransaction1Put();
    doTransaction2Put();
}

transactionCompletionCount = 0;
function onTransactionComplete(evt)
{
    preamble(evt);

    ++transactionCompletionCount;
    if (transactionCompletionCount < 2) {
        debug("first transaction complete, still waiting...");
        return;
    }

    shouldBeTrue("transaction1PutSuccess");
    shouldBeTrue("transaction2PutSuccess");

    evalAndLog("db1.close()");
    evalAndLog("db2.close()");
    finishJSTest();
}

test();
