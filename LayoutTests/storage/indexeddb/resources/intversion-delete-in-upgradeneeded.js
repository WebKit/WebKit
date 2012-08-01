if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that a deleteDatabase called while handling an upgradeneeded event is queued and fires its events at the right time");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = initiallyDeleted;
    request.onerror = unexpectedErrorCallback;
}

var sawFirstUpgradeNeeded = false;
var alreadyDeleted = false;
function initiallyDeleted(evt) {
    debug("");
    debug("initiallyDeleted():");
    evalAndLog("request = indexedDB.open(dbname, 1)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
    request.onupgradeneeded = firstUpgradeNeeded;
}

function firstUpgradeNeeded(evt)
{
    event = evt;
    debug("");
    debug("firstUpgradeNeeded():");
    shouldBeFalse("sawFirstUpgradeNeeded");
    evalAndLog("sawFirstUpgradeNeeded = true");
    shouldBe("event.oldVersion", "0");
    shouldBe("event.newVersion", "1");
    shouldBeFalse("alreadyDeleted");
    if (alreadyDeleted)
        return;
    alreadyDeleted = true;

    request2 = evalAndLog("deleteRequest = indexedDB.deleteDatabase(dbname)");
    evalAndLog("request2.onsuccess = deleteFromUpgradeNeededSuccess");
    request2.onerror = unexpectedErrorCallback;
    request2.onblocked = deleteBlockedCallback;
}

sawVersionChange = false;
function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("request.onsuccess():");
    shouldBeTrue("sawFirstUpgradeNeeded");
    evalAndLog("db = event.target.result");
    db.onversionchange = versionChangeCallback;
    shouldBe('db.version', '1');
}

function versionChangeCallback(evt) {
    preamble();
    shouldBe("event.oldVersion", "1");
    shouldBeNull("event.newVersion");
    evalAndLog("sawVersionChange = true");
    evalAndLog("db.close()");
}

function deleteBlockedCallback(evt)
{
    preamble();
    debug("This shouldn't happen but for the longstanding http://crbug.com/100123");
}

function deleteFromUpgradeNeededSuccess(evt)
{
    debug("");
    debug("deleteFromUpgradeNeededSuccess():");
    shouldBeTrue("sawVersionChange");
    finishJSTest();
}

test();
