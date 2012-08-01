if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test that when db.close is called in upgradeneeded, the db is cleaned up on refresh.");

function test()
{
    removeVendorPrefixes();
    setDBNameFromPath();

    request = evalAndLog("indexedDB.deleteDatabase(dbname)");
    request.onsuccess = deleteSuccess;
    request.onerror = unexpectedErrorCallback;
}

function deleteSuccess(evt) {
    evalAndLog("request = indexedDB.open(dbname, 7)");
    request.onsuccess = openSuccess;
    request.onupgradeneeded = upgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
    debug("");
}

var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    event = evt;
    debug("");
    debug("upgradeNeeded():");
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "7");

    evalAndLog("transaction = event.target.transaction");
    evalAndLog("db.createObjectStore('os')");
    evalAndLog("db.close()");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function() {
        debug("");
        debug("transaction.oncomplete:");
        evalAndLog("sawTransactionComplete = true");
    }
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    shouldBeTrue("sawTransactionComplete");
    db = evalAndLog("db = event.target.result");
    shouldBe('db.version', "7");
    evalAndLog("transaction = db.transaction('os')");
    finishJSTest();
}

test();
