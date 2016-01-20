description("This test starts two read-only transactions to an object store followed by a read-write transaction. \
It verifies that the read-write doesn't start until both read-onlys have finished.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("TransactionScheduler6Database");
var database;

createRequest.onupgradeneeded = function(event) {
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        debug("put FAILED - " + event);
        done();
    }
    
    versionTransaction.onabort = function(event) {
        debug("versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("versionchange transaction completed");
        continueTest();
    }

    versionTransaction.onerror = function(event) {
        debug("versionchange transaction error'ed - " + event);
        done();
    }
}

function continueTest()
{
    startTransactionLoop(database.transaction("TestObjectStore", "readonly"), true);
    startTransactionLoop(database.transaction("TestObjectStore", "readonly"), true);
    
    var transaction = database.transaction("TestObjectStore", "readwrite");
    var objectStore = transaction.objectStore("TestObjectStore");
    var request = objectStore.put("baz", "foo");

    request.onsuccess = function(event) {
        debug("Write in readwrite transaction succeeded");
    }
    
    request.onerror = function(event) {
        debug("Write in readwrite transaction unexpectedly failed");
        done();
    }
    
    transaction.onabort = function(event) {
        debug("readwrite transaction expectedly aborted");
        done();
    }

    transaction.oncomplete = function(event) {
        debug("readwrite transaction completed");
        done();
    }

    transaction.onerror = function(event) {
        debug("readwrite transaction error'ed - " + event);
        done();
    }
}

var numberOfOpenTransactions = 0;

function startTransactionLoop(transaction, isFirstTime)
{
    var objectStore = transaction.objectStore("TestObjectStore");
    var request = objectStore.get("bar");
    
    request.onsuccess = function(event) {
        if (isFirstTime) {
            debug("Starting a readonly transaction");
            numberOfOpenTransactions++;
        }
        
        if (numberOfOpenTransactions == 2)
            return;

        startTransactionLoop(event.target.transaction, false);
    }

    request.onerror = function(event) {
        debug("Unexpected request error - " + event);
        done();
    }

    transaction.onerror = function(event) {
        debug("Unexpected transaction error - " + event);
        done();
    }

    transaction.onabort = function(event) {
        --numberOfOpenTransactions;
        debug("Unexpected transaction abort - " + event);
        done();
    }

    transaction.oncomplete = function(event) {
        --numberOfOpenTransactions;
        debug("readonly transaction completed");
    }
}


