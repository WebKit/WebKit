if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that integer versions are reverted when their version transactions abort.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = firstUpgradeNeededCallback;
}

function firstUpgradeNeededCallback(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('some os')");
}

function openSuccess(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "2");
    evalAndLog("db.close()");
    evalAndLog("request = indexedDB.open(dbname, 3)");
    evalAndLog("request.onupgradeneeded = secondUpgradeNeededCallback");
    evalAndLog("request.onerror = errorAfterAbortCallback");
    request.onsuccess = unexpectedSuccessCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function secondUpgradeNeededCallback(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('some os 2')");
    evalAndLog("event.target.transaction.abort()");
}

function errorAfterAbortCallback(evt)
{
    preamble(evt);
    shouldBe("db.version", "2");
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = finalSuccessCallback");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
}

function finalSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "2");
    shouldBe("db.objectStoreNames.length", "1");
    shouldBeEqualToString("db.objectStoreNames[0]", "some os");
    finishJSTest();
}

test();
