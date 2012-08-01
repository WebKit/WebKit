if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Try to call db.close() after upgradeneeded but before the corresponding success event fires");

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
    evalAndLog("db.createObjectStore('objectstore')");

    evalAndLog("transaction = event.target.transaction");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function() {
        debug("");
        debug("transaction.oncomplete:");
        evalAndLog("sawTransactionComplete = true");
        evalAndLog("setTimeout(closeDB, 0)");
    }
}

var didCallCloseDB = false;
function closeDB()
{
    debug("");
    debug("closeDB():");
    evalAndLog("didCallCloseDB = true");
    evalAndLog("db.close()");
    finishJSTest();
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    shouldBeFalse("didCallCloseDB");
    shouldBeTrue("sawTransactionComplete");
    db = evalAndLog("db = event.target.result");
    shouldBe('db.version', "7");
    if (db.version !== 7) {
        finishJSTest();
        return;
    }
    shouldBe("db.objectStoreNames.length", "1");
    evalAndLog("db.transaction(db.objectStoreNames)");
}

test();
