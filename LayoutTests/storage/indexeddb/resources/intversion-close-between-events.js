if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
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
    request.onsuccess = request.onerror = openSuccessOrError;
    request.onupgradeneeded = upgradeNeeded;
    request.onblocked = unexpectedBlockedCallback;
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
var didGetOpenSuccessOrError = false;

function closeDB()
{
    db.close();
    didCallCloseDB = true;
    checkFinished();
}

function openSuccessOrError(evt)
{
    // May get either a success or error event, depending on when the timeout fires
    // relative to the open steps.
    didGetOpenSuccessOrError = true;

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

    if (!didCallCloseDB || !didGetOpenSuccessOrError) {
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
