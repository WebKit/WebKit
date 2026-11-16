if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check that readwrite transactions with overlapping scopes do not run in parallel.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = openConnection1;
}

function openConnection1()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname, 1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function openOnUpgradeNeeded1(evt) {
        preamble(evt);
        evalAndLog("db = event.target.result");
        evalAndLog("store = db.createObjectStore('store')");
    };
    request.onsuccess = function openOnSuccess(evt) {
        preamble(evt);
        evalAndLog("db1 = event.target.result");
        openConnection2();
    };
}

function openConnection2()
{
    preamble();
    request = evalAndLog("indexedDB.open(dbname, 1)");
    request.onerror = unexpectedBlockedCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
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
    evalAndLog("transaction2 = db2.transaction('store', 'readwrite')");
    transaction1.onabort = unexpectedAbortCallback;

    evalAndLog("transaction1PutSuccess = false");
    evalAndLog("transaction1Complete = false");
    evalAndLog("transaction2PutSuccess = false");
    evalAndLog("transaction2Complete = false");

    debug("");
    debug("Keep transaction1 alive for a while and ensure transaction2 doesn't start");

    evalAndLog("count = 0");

    (function doTransaction1Put() {
        request = evalAndLog("transaction1.objectStore('store').put(1, count++)");
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function put1OnSuccess(evt) {
            preamble(evt);
            evalAndLog("transaction1PutSuccess = true");
            if (count < 5) {
                doTransaction1Put();
            }
        };
    }());

    transaction1.oncomplete = function onTransaction1Complete(evt) {
        preamble(evt);
        evalAndLog("transaction1Complete = true");
        shouldBeFalse("transaction2PutSuccess");
        shouldBeFalse("transaction2Complete");
    };

    request = evalAndLog("transaction2.objectStore('store').put(2, 0)");
    request.onerror = unexpectedErrorCallback;
    request.onsuccess = function put2OnSuccess(evt) {
        preamble(evt);
        evalAndLog("transaction2PutSuccess = true");
        shouldBeTrue("transaction1Complete");
    };

    transaction2.oncomplete = function onTransaction2Complete(evt) {
        preamble(evt);
        evalAndLog("transaction2Complete = true");
        shouldBeTrue("transaction1PutSuccess");
        shouldBeTrue("transaction1Complete");
        shouldBeTrue("transaction2PutSuccess");

        finishJSTest();
    };
}

test();
