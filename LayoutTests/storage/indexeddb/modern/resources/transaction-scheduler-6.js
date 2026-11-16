description("This test starts two read-only transactions to an object store followed by a read-write transaction. \
It verifies that the read-write doesn't start until both read-onlys have finished.");

indexedDBTest(prepareDatabase);

function log(msg)
{
	debug(msg);
}

function done()
{
    finishJSTest();
}

var database;

function prepareDatabase(event)
{
    log("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = event.target.transaction;
    database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        log("put FAILED - " + event);
        done();
    }
    
    versionTransaction.onabort = function(event) {
        log("versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        log("versionchange transaction completed");
        continueTest();
    }

    versionTransaction.onerror = function(event) {
        log("versionchange transaction error'ed - " + event);
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
        log("Write in readwrite transaction succeeded");
    }
    
    request.onerror = function(event) {
        log("Write in readwrite transaction unexpectedly failed");
        done();
    }
    
    transaction.onabort = function(event) {
        log("readwrite transaction expectedly aborted");
        done();
    }

    transaction.oncomplete = function(event) {
        log("readwrite transaction completed");
        done();
    }

    transaction.onerror = function(event) {
        log("readwrite transaction error'ed - " + event);
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
            log("Starting a readonly transaction");
            numberOfOpenTransactions++;
        }
        
        if (numberOfOpenTransactions == 2)
            return;

        startTransactionLoop(event.target.transaction, false);
    }

    request.onerror = function(event) {
        log("Unexpected request error - " + event);
        done();
    }

    transaction.onerror = function(event) {
        log("Unexpected transaction error - " + event);
        done();
    }

    transaction.onabort = function(event) {
        log("Unexpected transaction abort - " + event);
        done();
    }

    transaction.oncomplete = function(event) {
        log("readonly transaction completed");
    }
}


