if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Test that initial version after a successful open of a non-existent db is 1");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    request = evalAndLog("indexedDB.open(dbname)");
    request.onsuccess = openSuccess;
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
    request.onupgradeneeded = function() {
      testPassed("Got upgradeneeded event");
    }
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    db = evalAndLog("db = event.target.result");
    debug("Test line from IDBFactory.open: If no version is specified and no database exists, set database version to 1.");
    shouldBe('db.version', '1');
    finishJSTest();
}

test();
