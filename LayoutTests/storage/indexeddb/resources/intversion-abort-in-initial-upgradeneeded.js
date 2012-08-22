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
    evalAndLog("request.onerror = onError");
    request.onblocked = unexpectedBlockedCallback;
}

function upgradeNeeded(evt)
{
    preamble(evt);
    db = event.target.result;
    shouldBe("db.version", "2");
    transaction = event.target.transaction;
    transaction.oncomplete = unexpectedCompleteCallback;
    transaction.onabort = onAbort;
    evalAndLog("transaction.abort()");
}

function onAbort(evt)
{
    preamble(evt);
    shouldBe("event.target.db.version", "0");
    shouldBeNonNull("request.transaction");
}

function onError(evt)
{
    preamble(evt);
    shouldBe("db", "event.target.result");
    shouldBe("request", "event.target");
    shouldBeEqualToString("event.target.error.name", "AbortError");
    shouldBe("event.target.result.version", "0");
    shouldBeNull("request.transaction");
    finishJSTest();
}

test();
