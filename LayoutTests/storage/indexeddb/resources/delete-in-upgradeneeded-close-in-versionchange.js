if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that a deleteDatabase called while handling an upgradeneeded event is queued and fires its events at the right time. The close() call to unblock the delete occurs in the connection's 'versionchange' event handler.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = initiallyDeleted;
    request.onerror = unexpectedErrorCallback;
}

var sawUpgradeNeeded = false;
var sawVersionChange = false;
var sawDeleteBlocked = false;

function initiallyDeleted(evt) {
    preamble(evt);
    evalAndLog("request = indexedDB.open(dbname, 1)");
    request.onupgradeneeded = upgradeNeededCallback;
    request.onsuccess = unexpectedSuccessCallback;
}

function upgradeNeededCallback(evt)
{
    preamble(evt);
    shouldBeFalse("sawUpgradeNeeded");
    evalAndLog("sawUpgradeNeeded = true");
    shouldBe("event.oldVersion", "0");
    shouldBe("event.newVersion", "1");

    evalAndLog("db = event.target.result");
    db.onversionchange = versionChangeCallback;
    request2 = evalAndLog("deleteRequest = indexedDB.deleteDatabase(dbname)");
    evalAndLog("request2.onsuccess = deleteSuccessCallback");
    request2.onerror = unexpectedErrorCallback;
    request2.onblocked = deleteBlockedCallback;
}

function versionChangeCallback(evt) {
    preamble(evt);
    debug("FIXME: These shouldn't be undefined. http://crbug.com/153122");
    shouldBe("event.oldVersion", "1");
    shouldBeNull("event.newVersion");
    evalAndLog("sawVersionChange = true");

    debug("Closing the connection before the IDBOpenDBRequest's success fires will cause the open to fail.");
    evalAndLog("db.close()");
}

function deleteBlockedCallback(evt)
{
    preamble(evt);
    shouldBeTrue("sawVersionChange");
    evalAndLog("sawDeleteBlocked = true");
}

function deleteSuccessCallback(evt)
{
    preamble(evt);
    shouldBeTrue("sawVersionChange");
    debug("FIXME: Blocked events shouldn't fire if connections close in versionchange handler. http://wkbug.com/71130");
    shouldBeFalse("sawDeleteBlocked");
    shouldBeTrue("sawUpgradeNeeded");
    finishJSTest();
}

test();
