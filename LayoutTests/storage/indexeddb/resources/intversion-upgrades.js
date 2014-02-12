if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Upgrade a database, open a second connection at the same version, ensure specifying a lower version causes an error");

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
    request.onupgradeneeded = initialUpgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
}

function initialUpgradeNeeded(evt)
{
    preamble(evt);
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    evalAndLog("connection1 = event.target.result");
    shouldBe("connection1.version", "1");
    evalAndLog("connection1.onversionchange = connection1VersionChangeCallback");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onupgradeneeded = connection2UpgradeNeeded
    request.onerror = unexpectedErrorCallback;
    request.onblocked = connection2BlockedCallback;
    request.onsuccess = connection2Success;
}

function connection1VersionChangeCallback(evt)
{
    preamble(evt);
    evalAndLog("connection1.close()");
}

function connection2BlockedCallback(evt)
{
    preamble(evt);
    debug("This should not be called: http://wkbug.com/71130");
}

function connection2UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("connection2 = event.target.result");
    shouldBeEqualToString("String(connection2)", "[object IDBDatabase]");
    shouldBeEqualToString("String(request.transaction)", "[object IDBTransaction]");
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "2");
    evalAndLog("request.transaction.oncomplete = transactionCompleted");
    request.transaction.onabort = unexpectedAbortCallback;
}

var didTransactionComplete = false;
function transactionCompleted(evt)
{
    preamble(evt);
    evalAndLog("didTransactionComplete = true");
}

function connection2Success(evt)
{
    preamble(evt);
    shouldBeTrue("didTransactionComplete");
    shouldBe("event.target.result", "connection2");
    debug("The next connection opens the database at the same version so connection2 shouldn't get a versionchange event.");
    evalAndLog("connection2.onversionchange = unexpectedVersionChangeCallback");
    shouldBeEqualToString("String(connection2)", "[object IDBDatabase]");
    shouldBeNull("request.transaction");
    shouldBe("connection2.version", "2");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    evalAndLog("request.onsuccess = connection3Success");
    evalAndLog("request.onblocked = unexpectedBlockedCallback");
    request.onerror = unexpectedErrorCallback;
}

function connection3Success(evt)
{
    preamble(evt);
    evalAndLog("event.target.result.close()");
    evalAndLog("connection2.close()");
    evalAndLog("connection3 = event.target.result");
    shouldBe("connection3.version", "2");
    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("request.onerror = errorWhenTryingLowVersion");
    request.onblocked = unexpectedBlockedCallback;
    request.onsuccess = unexpectedSuccessCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
}

function errorWhenTryingLowVersion(evt)
{
    preamble(evt);
    debug("request.webkitErrorMessage = " + request.webkitErrorMessage);
    evalAndLog("request = indexedDB.open(dbname, 4)");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    evalAndLog("request.onupgradeneeded = connection4UpgradeNeeded");
    evalAndLog("request.onsuccess = connection4Success");
}

var gotConnection4UpgradeNeeded = false;
function connection4UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("gotConnection4UpgradeNeeded = true");
    shouldBe("event.oldVersion", "2");
    shouldBe("event.newVersion", "4");
}

function connection4Success(evt)
{
    preamble(evt);
    shouldBeTrue("gotConnection4UpgradeNeeded");
    evalAndLog("connection4 = event.target.result");
    shouldBe("connection4.version", "4");
    evalAndLog("connection4.close()");
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = connection5Success");
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
    request.onerror = unexpectedUpgradeNeededCallback;
}

function connection5Success(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("db.version", "4");
    finishJSTest();
}

test();
