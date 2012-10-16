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
    preamble(evt);

    evalAndLog("request = indexedDB.open(dbname, 7)");
    request.onsuccess = openSuccess;
    request.onupgradeneeded = upgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
    request.onerror = unexpectedErrorCallback;
}

var sawTransactionComplete = false;
function upgradeNeeded(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    shouldBe("event.newVersion", "7");
    evalAndLog("db.createObjectStore('objectstore')");

    evalAndLog("transaction = event.target.transaction");
    transaction.onabort = unexpectedAbortCallback;
    transaction.oncomplete = function transactionComplete(evt) {
        preamble(evt);
        evalAndLog("sawTransactionComplete = true");

        debug("");
        debug("Now try and close the database after oncomplete but before onsuccess.");
        debug("This will not happen in single process ports. In multi-process ports");
        debug("it may and is fine; but flaky crashes would indicate a regression.");
        debug("");

        evalAndLog("setTimeout(closeDB, 0)");
    };
}

var didCallCloseDB = false;
var didGetOpenSuccess = false;

function closeDB()
{
    db.close();
    didCallCloseDB = true;
    checkFinished();
}

function openSuccess(evt)
{
    didGetOpenSuccess = true;

    var quiet = true;
    if (didCallCloseDB) {
        evalAndExpectException("db.transaction(db.objectStoreNames)", "DOMException.INVALID_STATE_ERR", "'InvalidStateError'", quiet);
    } else {
        evalAndLog("db.transaction(db.objectStoreNames)", quiet);
    }

    checkFinished();
}

function checkFinished()
{
    preamble();

    if (!didCallCloseDB || !didGetOpenSuccess) {
        debug("Not done yet...");
        return;
    }

    shouldBeTrue("didCallCloseDB");
    shouldBeTrue("sawTransactionComplete");
    shouldBe('db.version', "7");
    shouldBe("db.objectStoreNames.length", "1");

    finishJSTest();
}

test();
