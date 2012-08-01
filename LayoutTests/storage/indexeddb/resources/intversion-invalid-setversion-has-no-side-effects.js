if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that when setVersion is called on a db with an integer version, versionchange events don't get fired on other open dbs.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();
    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    preamble();
    evalAndLog("request = indexedDB.open(dbname, 2)");
    request.onsuccess = firstSuccessEventHandler;
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
    gotFirstUpgradeNeededEvent = false;
    request.onupgradeneeded = firstUpgradeNeededCallback;
}

function firstUpgradeNeededCallback(evt)
{
    preamble(evt);
    evalAndLog("gotFirstUpgradeNeededEvent = true");
    shouldBe("event.oldVersion", "0");
    shouldBe("event.newVersion", "2");
}

function firstSuccessEventHandler(evt)
{
    preamble(evt);
    shouldBeTrue("gotFirstUpgradeNeededEvent");
    evalAndLog("connection1 = event.target.result");
    evalAndLog("connection1.onversionchange = unexpectedVersionChangeCallback");
    shouldBeEqualToString("String(connection1)", "[object IDBDatabase]");
    evalAndLog("request = indexedDB.open(dbname)");
    request.onsuccess = secondSuccessCallback;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = unexpectedUpgradeNeededCallback;
}

function secondSuccessCallback(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("request = db.setVersion('version')");
    evalAndLog("request.onerror = setVersionError");
    evalAndLog("request.onsuccess = unexpectedSuccessCallback");
    evalAndLog("request.onblocked = unexpectedBlockedCallback");
}

function setVersionError(evt)
{
    preamble();
    debug("Open up a new db to ensure that if connection1 were to get a versionchange event, it will be delivered before the test finishes");
    evalAndLog("new_db = indexedDB.open(dbname + '2')");
    evalAndLog("new_db.onsuccess = newDBSuccess");
    new_db.onerror = unexpectedErrorCallback;
    new_db.onblocked = unexpectedBlockedCallback;
}

function newDBSuccess(evt)
{
    preamble();
    finishJSTest();
}

test();
