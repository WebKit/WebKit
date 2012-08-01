if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that an abort in the initial upgradeneeded sets version back to 0");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess() {
    request = evalAndLog("indexedDB.open(dbname, 2)");
    request.onsuccess = unexpectedSuccessCallback;
    evalAndLog("request.onupgradeneeded = upgradeNeeded");
    debug("FIXME: This should get an error event of type AbortError");
    evalAndLog("request.onerror = unexpectedErrorCallback");
    request.onblocked = unexpectedBlockedCallback;
}

function upgradeNeeded(evt)
{
    debug("");
    debug("upgradeNeeded():");
    event = evt;
    db = event.target.result;
    shouldBe("db.version", "2");
    transaction = event.target.transaction;
    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = onAbort;
    evalAndLog("transaction.abort()");
}

function onAbort(evt)
{
    debug("");
    debug("onAbort():");
    event = evt;
    shouldBe("event.target.db.version", "0");
    finishJSTest();
}

test();
