if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that blocked events get delivered properly with the new open api");

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
    shouldBeEqualToString("String(event)", "[object IDBVersionChangeEvent]");
}

gotBlockedEvent = false;
function firstSuccessEventHandler(evt)
{
    preamble(evt);
    shouldBeTrue("gotFirstUpgradeNeededEvent");
    evalAndLog("connection1 = event.target.result");
    evalAndLog("connection1.onversionchange = versionChangeHandler");
    shouldBeEqualToString("String(connection1)", "[object IDBDatabase]");
    evalAndLog("request = indexedDB.open(dbname, 3)");
    request.onsuccess = secondSuccessCallback;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = blockedEventHandler;
    request.onupgradeneeded = secondUpgradeNeededEventHandler;
}

var sawVersionChangeEvent = false;
function versionChangeHandler(evt)
{
    preamble(evt);
    evalAndLog("sawVersionChangeEvent = true");
    shouldBeEqualToString("event.type", "versionchange");
    shouldBeEqualToString("String(event)", "[object IDBVersionChangeEvent]");
    shouldBe("event.target", "connection1");
    shouldBe("event.oldVersion", "2");
    shouldBe("event.newVersion", "3");
}

function blockedEventHandler(evt)
{
    preamble(evt);
    shouldBeTrue("sawVersionChangeEvent");
    evalAndLog("gotBlockedEvent = true");
    shouldBeEqualToString("String(event)", "[object IDBVersionChangeEvent]");
    shouldBe("event.oldVersion", "2");
    shouldBe("event.newVersion", "3");
    shouldBeEqualToString("event.type", "blocked");
    evalAndLog("connection1.close()");
}

gotSecondUpgradeNeededEvent = false;
function secondUpgradeNeededEventHandler(evt)
{
    preamble(evt);
    shouldBeTrue("gotBlockedEvent");
    evalAndLog("gotSecondUpgradeNeededEvent = true");
}

function secondSuccessCallback(evt)
{
    preamble(evt);
    shouldBeTrue("gotSecondUpgradeNeededEvent");
    finishJSTest();
}

test();
