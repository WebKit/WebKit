if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Check that read-only transactions within a database can run in parallel.");

indexedDBTest(prepareDatabase, runParallelTransactions);

function prepareDatabase(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    evalAndLog("store = db.createObjectStore('store')");
    evalAndLog("store.put('value', 'key')");
}

function runParallelTransactions(evt)
{
    preamble(evt);
    evalAndLog("db = event.target.result");
    debug("");
    evalAndLog("transaction1 = db.transaction('store', 'readonly')");
    transaction1.onabort = unexpectedAbortCallback;
    transaction1.oncomplete = onTransactionComplete;
    evalAndLog("transaction2 = db.transaction('store', 'readonly')");
    transaction1.onabort = unexpectedAbortCallback;
    transaction2.oncomplete = onTransactionComplete;

    evalAndLog("transaction1GetSuccess = false");
    evalAndLog("transaction2GetSuccess = false");

    debug("Keep both transactions alive until each has reported at least one successful operation");

    function doTransaction1Get() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction1.objectStore('store').get('key');
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            transaction1GetSuccess = true;
            if (!transaction2GetSuccess)
                doTransaction1Get();
        };
    }

    function doTransaction2Get() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction2.objectStore('store').get('key');
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            transaction2GetSuccess = true;
            if (!transaction1GetSuccess)
                doTransaction2Get();
        };
    }

    doTransaction1Get();
    doTransaction2Get();
}

transactionCompletionCount = 0;
function onTransactionComplete(evt)
{
    preamble(evt);

    ++transactionCompletionCount;
    if (transactionCompletionCount < 2) {
        debug("first transaction complete, still waiting...");
        return;
    }

    shouldBeTrue("transaction1GetSuccess");
    shouldBeTrue("transaction2GetSuccess");

    evalAndLog("db.close()");
    finishJSTest();
}
