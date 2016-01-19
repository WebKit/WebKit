description("This test makes sure that two read-only transactions to an object store are active at the same time.");


if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function done()
{
    finishJSTest();
}

var createRequest = window.indexedDB.open("TransactionScheduler1Database");

createRequest.onupgradeneeded = function(event) {
    debug("ALERT: " + "Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    var versionTransaction = createRequest.transaction;
    var database = event.target.result;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        debug("ALERT: " + "put FAILED - " + event);
        done();
    }
    
    versionTransaction.onabort = function(event) {
        debug("ALERT: " + "versionchange transaction aborted");
        done();
    }

    versionTransaction.oncomplete = function(event) {
        debug("ALERT: " + "versionchange transaction completed");
        continueTest();
        database.close();
    }

    versionTransaction.onerror = function(event) {
        debug("ALERT: " + "versionchange transaction error'ed - " + event);
        done();
    }
}

function continueTest()
{
    var request1 = window.indexedDB.open("TransactionScheduler1Database", 1);
    var request2 = window.indexedDB.open("TransactionScheduler1Database", 1);

    setupRequest(request1, "request 1");
    setupRequest(request2, "request 2");
}

function setupRequest(request, reqName)
{
    request.onsuccess = function(event) {
        debug("ALERT: " + "Success opening database connection - " + reqName);
        var database = event.target.result;
    
        startTransactionLoop(event.target.result.transaction("TestObjectStore", "readonly"), true);
    }
    request.onerror = function(event) {
        debug("ALERT: " + "Unexpected error - " + reqName + " - " + event);
        done();
    }
    request.onblocked = function(event) {
        debug("ALERT: " + "Unexpected blocked - " + reqName + " - " + event);
        done();
    }
    request.onupgradeneeded = function(event) {
        debug("ALERT: " + "Unexpected upgradeneeded - " + reqName + " - " + event);
        done();
    } 
}

var numberOfOpenTransactions = 0;

function startTransactionLoop(transaction, isFirstTime)
{
    var objectStore = transaction.objectStore("TestObjectStore");
    var request = objectStore.get("bar");
    
    request.onsuccess = function(event) {
        if (isFirstTime)
            numberOfOpenTransactions++;
        
        if (numberOfOpenTransactions == 2) {
            debug("ALERT: " + "Two transactions open at once. Yay.");
            done();
        }
        startTransactionLoop(event.target.transaction, false);
    }

    request.onerror = function(event) {
        debug("ALERT: " + "Unexpected request error - " + event);
        done();
    }

    transaction.onerror = function(event) {
        debug("ALERT: " + "Unexpected transaction error - " + event);
        done();
    }

    transaction.onabort = function(event) {
        --numberOfOpenTransactions;
        debug("ALERT: " + "Unexpected transaction abort - " + event);
        done();
    }

    transaction.oncomplete = function(event) {
        --numberOfOpenTransactions;
        debug("ALERT: " + "Unexpected transaction complete - " + event);
        done();
    }
}


