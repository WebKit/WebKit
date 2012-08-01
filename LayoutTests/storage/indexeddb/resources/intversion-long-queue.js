if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that a database is recreated correctly when an open-with-version call is queued behind both a deleteDatabase and setVersion call");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    preamble(evt);
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = firstSuccessCallback");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function firstSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("connection1 = event.target.result");
    evalAndLog("connection1.onversionchange = connection1VersionChangeCallback");
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = secondSuccessCallback");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function secondSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("connection2 = event.target.result");
    evalAndLog("connection2.onversionchange = connection2VersionChangeCallback");
    evalAndLog("request = connection2.setVersion('version 2')");
    evalAndLog("request.onblocked = setVersionBlockedCallback");
    evalAndLog("request.onsuccess = setVersionSuccessCallback");
    request.onerror = unexpectedErrorCallback;
}

function connection1VersionChangeCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    shouldBeEqualToString("event.version", "version 2");
}

function setVersionBlockedCallback(evt)
{
    event = evt;
    debug("");
    debug("setVersionBlockedCallback():");
    evalAndLog("request = indexedDB.deleteDatabase(dbname)");
    evalAndLog("request.onblocked = deleteDatabaseBlockedCallback");
    evalAndLog("request.onsuccess = deleteDatabaseSuccessCallback");
    request.onerror = unexpectedErrorCallback;

    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("request.onupgradeneeded = upgradeNeededCallback");
    evalAndLog("request.onsuccess = openWithVersionSuccessCallback");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    evalAndLog("connection1.close()");
}

function setVersionSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("transaction = event.target.result");
    evalAndLog("db = transaction.db");
    evalAndLog("db.createObjectStore('some object store')");
    evalAndLog("transaction.oncomplete = setVersionTransactionCompleteCallback");
    transaction.onabort = unexpectedAbortCallback;
}

function setVersionTransactionCompleteCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.target.db.version", "version 2");
}

function connection2VersionChangeCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    debug("FIXME: Fire a versionchange event that has oldVersion and newVersion");
    shouldBeEqualToString("event.oldVersion", "version 2");
    shouldBeNull("event.newVersion");
    shouldBeUndefined("event.version");
}

function deleteDatabaseBlockedCallback(evt)
{
    preamble(evt);
    evalAndLog("connection2.close()");
    shouldBeEqualToString("event.oldVersion", "version 2");
    shouldBeNull("event.newVersion");
    shouldBeUndefined("event.version");
}

function deleteDatabaseSuccessCallback(evt)
{
    preamble(evt);
    shouldBeNonNull("event.oldVersion");
    shouldBeNull("event.newVersion");
    shouldBeNull("event.target.result");
}

var gotUpgradeNeededEvent = false;
function upgradeNeededCallback(evt)
{
    preamble(evt);
    evalAndLog("gotUpgradeNeededEvent = true");
    shouldBe("event.newVersion", "1");
    shouldBe("event.oldVersion", "0");
}

function openWithVersionSuccessCallback(evt)
{
    preamble(evt);
    shouldBeTrue("gotUpgradeNeededEvent");
    shouldBe("event.target.result.objectStoreNames.length", "0");
    finishJSTest();
}

test();
