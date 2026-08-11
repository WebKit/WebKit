if (this.importScripts) {
    importScripts('../../../resources/js-test.js');
    importScripts('shared.js');
}

description("Check that transactions that may overlap complete properly.");

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

    evalAndLog("transaction1GetSuccess = 0");
    evalAndLog("transaction2GetSuccess = 0");

    function doTransaction1Get() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction1.objectStore('store').get('key');
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            ++transaction1GetSuccess;
            if (!transaction2GetSuccess && transaction1GetSuccess < 10)
                doTransaction1Get();
        };
    }

    function doTransaction2Get() {
        // NOTE: No logging since execution order is not deterministic.
        request = transaction2.objectStore('store').get('key');
        request.onerror = unexpectedErrorCallback;
        request.onsuccess = function() {
            ++transaction2GetSuccess;
            if (!transaction1GetSuccess && transaction2GetSuccess < 10)
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

    shouldBeNonZero("transaction1GetSuccess");
    shouldBeNonZero("transaction2GetSuccess");

    evalAndLog("db.close()");
    finishJSTest();
}
