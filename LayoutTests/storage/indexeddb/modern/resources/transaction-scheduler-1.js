description("This test makes sure that two read-only transactions to an object store are active at the same time.");

indexedDBTest(prepareDatabase);

var dbname;
function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("TestObjectStore");
    var request = objectStore.put("foo", "bar");

    request.onerror = function(event) {
        endTestWithLog("put FAILED - " + event);
    }
    
    versionTransaction.onabort = function(event) {
        endTestWithLog("versionchange transaction aborted");
    }

    versionTransaction.oncomplete = function(event) {
        debug("versionchange transaction completed");
        continueTest();
        database.close();
    }

    versionTransaction.onerror = function(event) {
        endTestWithLog("versionchange transaction error'ed - " + event);
    }
}

function continueTest()
{
    var request1 = window.indexedDB.open(dbname, 1);
    var request2 = window.indexedDB.open(dbname, 1);

    setupRequest(request1, "request 1");
    setupRequest(request2, "request 2");
}

function setupRequest(request, reqName)
{
    request.onsuccess = function(event) {
        debug("Success opening database connection - " + reqName);
        var database = event.target.result;
    
        startTransactionLoop(event.target.result.transaction("TestObjectStore", "readonly"), true);
    }
    request.onerror = function(event) {
        endTestWithLog("Unexpected error - " + reqName + " - " + event);
    }
    request.onblocked = function(event) {
        endTestWithLog("Unexpected blocked - " + reqName + " - " + event);
    }
    request.onupgradeneeded = function(event) {
        endTestWithLog("Unexpected upgradeneeded - " + reqName + " - " + event);
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
            endTestWithLog("Two transactions open at once. Yay.");
        }
        startTransactionLoop(event.target.transaction, false);
    }

    request.onerror = function(event) {
        endTestWithLog("Unexpected request error - " + event);
    }

    transaction.onerror = function(event) {
        endTestWithLog("Unexpected transaction error - " + event);
    }

    transaction.onabort = function(event) {
        --numberOfOpenTransactions;
        endTestWithLog("Unexpected transaction abort - " + event);
    }

    transaction.oncomplete = function(event) {
        --numberOfOpenTransactions;
        endTestWithLog("Unexpected transaction complete - " + event);
    }
}


