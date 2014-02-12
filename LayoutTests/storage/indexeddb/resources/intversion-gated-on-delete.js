if (this.importScripts) {
    importScripts('../../../resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that a database is recreated correctly when an open-with-version call is queued behind a deleteDatabase call");

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
    debugger;
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = firstSuccessCallback");
    request.onerror = unexpectedErrorCallback;
}

function firstSuccessCallback(evt)
{
    event = evt;
    debug("");
    debug("firstSuccessCallback():");
    evalAndLog("connection1 = event.target.result");
    evalAndLog("connection1.onversionchange = connection1VersionChangeCallback");
    evalAndLog("request = indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = secondSuccessCallback");
    request.onerror = unexpectedErrorCallback;

    evalAndLog("request = indexedDB.deleteDatabase(dbname)");
    evalAndLog("request.onblocked = deleteDatabaseBlockedCallback");
    evalAndLog("request.onsuccess = deleteDatabaseSuccessCallback");
    request.onerror = unexpectedErrorCallback;

    evalAndLog("request = indexedDB.open(dbname, 1)");
    evalAndLog("request.onupgradeneeded = upgradeNeededCallback");
    evalAndLog("request.onsuccess = openWithVersionSuccessCallback");
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
}

function secondSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.onversionchange = connection2VersionChangeCallback");
}

function connection1VersionChangeCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    shouldBe("event.oldVersion", "1");
    shouldBeNull("event.newVersion");
}

function connection2VersionChangeCallback(evt)
{
    preamble(evt);
    shouldBeEqualToString("event.type", "versionchange");
    evalAndLog("event.target.close()");
}

function deleteDatabaseBlockedCallback(evt)
{
    preamble(evt);
    evalAndLog("connection1.close()");
}

function deleteDatabaseSuccessCallback(evt)
{
    preamble(evt);
}

function upgradeNeededCallback(evt)
{
    preamble(evt);
    shouldBe("event.newVersion", "1");
    shouldBe("event.oldVersion", "0");
}

function openWithVersionSuccessCallback(evt)
{
    preamble(evt);
    shouldBe("event.target.result.objectStoreNames.length", "0");
    finishJSTest();
}

test();
