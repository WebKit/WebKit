if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that only the first open call gets an upgradeneeded");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

var gotFirstUpgradeNeededEvent = false;
var gotSecondUpgradeNeededEvent = false;
function deleteSuccess(evt) {
    request = evalAndLog("indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = connection1OpenSuccess");
    evalAndLog("request.onupgradeneeded = connection1UpgradeNeeded");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;

    request = evalAndLog("indexedDB.open(dbname)");
    evalAndLog("request.onsuccess = connection2OpenSuccess");
    evalAndLog("request.onupgradeneeded = connection2UpgradeNeeded");
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
}

function connection1UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("gotFirstUpgradeNeededEvent = true");
    shouldBe("event.oldVersion", "0");
    shouldBe("event.newVersion", "1");
}

function connection1OpenSuccess(evt)
{
    preamble(evt);
    db = evalAndLog("db = event.target.result");
    shouldBeTrue("gotFirstUpgradeNeededEvent");
    shouldBe("db.version", "1");
}

function connection2UpgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("gotSecondUpgradeNeededEvent = true");
}

function connection2OpenSuccess(evt)
{
    preamble(evt);
    db = evalAndLog("db = event.target.result");
    shouldBeFalse("gotSecondUpgradeNeededEvent");
    shouldBe("db.version", "1");
    finishJSTest();
}

test();
