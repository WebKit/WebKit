if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that int version upgrades a string version and that string versions are then disallowed");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    shouldBeEqualToString("String(request)", "[object IDBOpenDBRequest]");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    debug("");
    debug("Call open with no version parameter:");
    request = evalAndLog("indexedDB.open(dbname)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");
    shouldBeEqualToString("db.version", "");
    evalAndLog('request = db.setVersion("some version")');
    request.onsuccess = inSetVersion;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function inSetVersion(evt)
{
    event = evt;
    debug("");
    debug("inSetVersion():");
    trans = event.target.result;
    trans.oncomplete = setVersionDone;
    trans.onabort = unexpectedAbortCallback;
    db = trans.db;
    shouldBeEqualToString("db.version", "some version");
    // FIXME: Add a test that calls open with version from here.
}

function setVersionDone(evt)
{
    event = evt;
    debug("");
    debug("setVersionDone():");
    db = event.target.db;
    shouldBeEqualToString("db.version", "some version");
    evalAndLog("db.close()");
    request = evalAndLog("indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = secondOpen");
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
}

function secondOpen(evt)
{
    event = evt;
    debug("");
    debug("secondOpen():");
    evalAndLog("db = event.target.result");
    shouldBeEqualToString("String(db)", "[object IDBDatabase]");
    shouldBeEqualToString("db.version", "some version");
    evalAndLog("db.close()");
    // FIXME: Add tests that call it with 0, -1, and -4.
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onupgradeneeded = firstUpgradeNeeded
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = firstOpenWithVersion;
}

function firstUpgradeNeeded(evt)
{
    event = evt;
    debug("");
    debug("firstUpgradeNeeded():");
    evalAndLog("db = event.target.result");
    shouldBeEqualToString("String(db)", "[object IDBDatabase]");
    shouldBeEqualToString("String(request.transaction)", "[object IDBTransaction]");
    shouldBeEqualToString("event.oldVersion", "some version");
    shouldBe("event.newVersion", "2");
    evalAndLog("request.transaction.oncomplete = transactionCompleted");
    request.transaction.onabort = unexpectedAbortCallback;
}

var didTransactionComplete = false;
function transactionCompleted(evt)
{
    event = evt;
    debug("");
    debug("transactionCompleted():");
    evalAndLog("didTransactionComplete = true");
}

function firstOpenWithVersion(evt)
{
    event = evt;
    debug("");
    debug("firstOpenWithVersion():");
    shouldBeTrue("didTransactionComplete");
    evalAndLog("db = event.target.result");
    shouldBeEqualToString("String(db)", "[object IDBDatabase]");
    shouldBeNull("request.transaction");
    shouldBe("db.version", "2");
    shouldBeEqualToString("String(request)", "[object IDBOpenDBRequest]");
    evalAndLog('request = db.setVersion("string version 2")');
    shouldBeEqualToString("String(request)", "[object IDBVersionChangeRequest]");
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
    evalAndLog("request.onerror = setVersionNotAllowed");
}

function setVersionNotAllowed(evt)
{
    event = evt;
    debug("");
    debug("setVersionNotAllowed():");
    shouldBeEqualToString("event.type", "error");
    debug("request.webkitErrorMessage = " + request.webkitErrorMessage);
    evalAndLog("db.close()");
    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("request.onerror = errorWhenTryingLowVersion");
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
}

function errorWhenTryingLowVersion(evt)
{
    event = evt;
    debug("");
    debug("errorWhenTryingLowVersion():");
    debug("request.webkitErrorMessage = " + request.webkitErrorMessage);
    evalAndLog("request = indexedDB.open(dbname, 4)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    evalAndLog("request.onupgradeneeded = secondUpgradeNeeded");
    evalAndLog("request.onsuccess = thirdOpenSuccess");
}

var gotSecondUpgradeNeeded = false;
function secondUpgradeNeeded(evt)
{
    event = evt;
    debug("");
    debug("secondUpgradeNeeded():");
    evalAndLog("gotSecondUpgradeNeeded = true");
    shouldBe("event.oldVersion", "2");
    shouldBe("event.newVersion", "4");
}

function thirdOpenSuccess(evt)
{
    event = evt;
    debug("");
    debug("thirdOpenSuccess():");
    shouldBeTrue("gotSecondUpgradeNeeded");
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "4");
    evalAndLog("db.close()");
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = fourthOpenSuccess");
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onerror = unexpectedUpgradeNeededCallback;
}

function fourthOpenSuccess(evt)
{
    event = evt;
    debug("");
    debug("fourthOpenSuccess():");
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "4");
    finishJSTest();
}

test();
