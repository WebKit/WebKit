description("Makes sure that a read-only transaction scheduled after a read-write transaction with overlapping scope does not start until the read-write transaction completes");

indexedDBTest(prepareDatabase, versionChangeSuccessCallback);

function prepareDatabase()
{
    evalAndLog("connection = event.target.result;");
    evalAndLog("objectStore = connection.createObjectStore('store');");
}

function versionChangeSuccessCallback()
{
    evalAndLog("transaction1 = connection.transaction('store', 'readwrite');");
    evalAndLog("transaction2 = connection.transaction('store', 'readonly');");
    evalAndLog("getRequest = transaction2.objectStore('store').get('foo');");

    debug("Starting a series of 20 puts in the readwrite transaction to make sure the readonly transaction is deferred for some time");
    evalAndLog("putCount = 0;");
    evalAndLog("readWriteTransactionDone = false;");
    evalAndLog("getRequestDone = false;");
    var putSuccess = function() {
        if (++putCount == 20) {
            debug("20th put complete");
            return;
        }
        
        var newRequest = transaction1.objectStore('store').put('bar', 'foo');
        newRequest.onsuccess = putSuccess;
    };

    var request = transaction1.objectStore('store').put('bar', 'foo');
    request.onsuccess = putSuccess;



    transaction1.oncomplete = function() {
        debug("Transaction 1 (readwrite) completed with " + putCount + " puts completed.");
        shouldBeFalse("getRequestDone");
        shouldBe("putCount", "20");
        readWriteTransactionDone = true;
    }

    getRequest.onsuccess = function() {
        debug("Getting the value of 'foo' completed - This means the readonly transaction started. At this point " + putCount + " puts completed in the readwrite transaction.");
        shouldBeTrue("readWriteTransactionDone");
        shouldBe("putCount", "20");
        getRequestDone = true;
    }

    transaction2.oncomplete = function() {
        debug("Transaction 1 (readonly) completed");
        shouldBeTrue("readWriteTransactionDone");
        shouldBeTrue("getRequestDone");
        shouldBe("putCount", "20");
        finishJSTest();
    }
}
