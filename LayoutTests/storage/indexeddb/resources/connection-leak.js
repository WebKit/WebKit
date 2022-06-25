if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Regression test to ensure that IndexedDB connections don't leak");

setDBNameFromPath();
doFirstOpen();

function doFirstOpen()
{
    preamble();

    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("sawUpgradeNeeded1 = false");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function onUpgradeNeeded1() {
        preamble();
        evalAndLog("sawUpgradeNeeded1 = true");
    };
    request.onsuccess = function onOpenSuccess1() {
        preamble();
        shouldBeTrue("sawUpgradeNeeded1");
        evalAndLog("db = request.result");
        evalAndLog("db.close()");

        doSecondOpen();
    };
}

function doSecondOpen()
{
    preamble();

    evalAndLog("request = indexedDB.open(dbname, 1)");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onsuccess = function onOpenSuccess2() {
        preamble();
        evalAndLog("db = request.result");

        evalAndLog("db = null");
        evalAndLog("request = null");

        debug("Run GC outside of request's callback via setTimeout()");
        setTimeout( function() {
            evalAndLog("window.gc()");
            doThirdOpen();
        }, 0);
    };
}

function doThirdOpen()
{
    preamble();

    evalAndLog("request = indexedDB.open(dbname, 2)");
    evalAndLog("sawUpgradeNeeded3 = false");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function onUpgradeNeeded2() {
        preamble();
        evalAndLog("sawUpgradeNeeded3 = true");
    };
    request.onsuccess = function onOpenSuccess3() {
        preamble();
        shouldBeTrue("sawUpgradeNeeded3");
        evalAndLog("db = request.result");
        evalAndLog("db.close()");

        finishJSTest();
    };
}
