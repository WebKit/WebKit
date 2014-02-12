if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that a database is recreated correctly when an open-with-version call is queued behind both a deleteDatabase and an open-with-version call");

indexedDBTest(prepareDatabase, connection1Success);
function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
}

function connection1Success(evt)
{
    preamble(evt);
    evalAndLog("connection1 = event.target.result");
    shouldBe("db", "connection1");
    evalAndLog("connection1.onversionchange = connection1VersionChangeCallback");
    evalAndLog("request = indexedDB.open(dbname, 2)");
    evalAndLog("request.onsuccess = connection2Success");
    evalAndLog("request.onupgradeneeded = connection2UpgradeNeeded");
    evalAndLog("request.onblocked = connection2Blocked");
    request.onerror = unexpectedErrorCallback;
}

function connection1VersionChangeCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "2");
}

function connection2Blocked(evt)
{
    preamble(evt);
    evalAndLog("request = indexedDB.deleteDatabase(dbname)");
    evalAndLog("request.onblocked = deleteDatabaseBlockedCallback");
    evalAndLog("request.onsuccess = deleteDatabaseSuccessCallback");
    request.onerror = unexpectedErrorCallback;

    evalAndLog("request = indexedDB.open(dbname, 3)");
    evalAndLog("request.onupgradeneeded = connection3UpgradeNeeded");
    evalAndLog("request.onsuccess = connection3Success");
    request.onerror = unexpectedErrorCallback;
    evalAndLog("connection1.close()");
}

function deleteDatabaseBlockedCallback(evt)
{
    preamble(evt);
    shouldBe("event.oldVersion", "1");
    shouldBeNull("event.newVersion");
}

function deleteDatabaseSuccessCallback(evt)
{
    preamble(evt);
    shouldBeUndefined("event.target.result");
    shouldBeEqualToString("event.type", "success");
}

function connection2UpgradeNeeded(evt)
{
    preamble(evt);
    shouldBe("event.oldVersion", "1");
    shouldBe("event.newVersion", "2");
    evalAndLog("db = event.target.result");
    shouldBe("db.objectStoreNames.length", "0");
    evalAndLog("db.createObjectStore('some object store')");
    evalAndLog("transaction = event.target.transaction");
    evalAndLog("transaction.oncomplete = connection2TransactionComplete");
}

function connection2Success(evt)
{
    preamble(evt);
    evalAndLog("connection2 = event.target.result");
    connection2.onversionchange = unexpectedVersionChangeCallback;
    evalAndLog("connection2.close()");
}

function connection2TransactionComplete(evt)
{
    preamble(evt);
    shouldBe("db.version", "2");
}

var gotUpgradeNeededEvent = false;
function connection3UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("gotUpgradeNeededEvent = true");
    shouldBe("event.newVersion", "3");
    shouldBe("event.oldVersion", "0");
}

function connection3Success(evt)
{
    preamble(evt);
    shouldBeTrue("gotUpgradeNeededEvent");
    shouldBe("event.target.result.objectStoreNames.length", "0");
    finishJSTest();
}
