if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Call db.close() in the complete handler for a version change transaction, before the success event associated with the open call fires");

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
    request.onerror = unexpectedErrorCallback;
}

var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    event = evt;
    debug("");
    debug("upgradeNeeded():");
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "7");
    evalAndLog("db.createObjectStore('os')");

    evalAndLog("transaction = event.target.transaction");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function(e)
    {
        debug("");
        debug("transaction.oncomplete:");
        evalAndLog("sawTransactionComplete = true");
        evalAndLog("db.close()");
    }
}

function openSuccess(evt)
{
    event = evt;
    debug("");
    debug("openSuccess():");
    shouldBeTrue("sawTransactionComplete");
    evalAndLog("db = event.target.result");
    shouldBe('db.version', "7");
    evalAndExpectException("transaction = db.transaction('os', 'readwrite')", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'");
    finishJSTest();
}

test();
