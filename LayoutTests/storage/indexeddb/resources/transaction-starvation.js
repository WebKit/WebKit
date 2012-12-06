if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Check that read-only transactions don't starve read-write transactions.");

indexedDBTest(prepareDatabase, runTransactions);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("db.createObjectStore('store')");
}

function runTransactions(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    debug("");

    evalAndLog("readWriteTransactionStarted = false");
    evalAndLog("readWriteTransactionComplete = false");

    startReadOnlyTransaction();
}


function startReadOnlyTransaction()
{
    preamble();
    evalAndLog("transaction = db.transaction('store', 'readonly')");
    transaction.onabort = unexpectedAbortCallback;
    evalAndLog("store = transaction.objectStore('store')");
    debug("Keep the transaction alive with an endless series of gets");

    function doGet() {
        request = store.get(0);
        request.onsuccess = function() {
            if (!readWriteTransactionStarted)
                startReadWriteTransaction();

            if (!readWriteTransactionComplete)
                doGet();
        };
    }
    doGet();
}

function startReadWriteTransaction()
{
    preamble();
    evalAndLog("transaction = db.transaction('store', 'readwrite')");
    transaction.onabort = unexpectedAbortCallback;
    evalAndLog("readWriteTransactionStarted = true");
    transaction.oncomplete = function readWriteTransactionComplete() {
        preamble();
        testPassed("Transaction wasn't starved");
        evalAndLog("readWriteTransactionComplete = true");
        finishJSTest();
    };
}