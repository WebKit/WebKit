description("This test makes sure that two read-only transactions to two different object stores are active at the same time.");

indexedDBTest(prepareDatabase);

var dbname;
function prepareDatabase(event)
{
    debug("Upgrade needed: Old version - " + event.oldVersion + " New version - " + event.newVersion);

    event.target.onerror = null;

    var versionTransaction = event.target.transaction;
    var database = event.target.result;
    dbname = database.name;
    var objectStore = database.createObjectStore("OS1");
    var request = objectStore.put("foo1", "bar1");

    request.onerror = function(event) {
        endTestWithLog("put1 FAILED - " + event);
    }
    
    objectStore = database.createObjectStore("OS2");
    request = objectStore.put("foo2", "bar2");

    request.onerror = function(event) {
        endTestWithLog("put2 FAILED - " + event);
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

    setupRequest(request1, "OS1");
    setupRequest(request2, "OS2");
}

function setupRequest(request, osname)
{
    request.onsuccess = function(event) {
        debug("Success opening database connection - Starting transaction to ObjectStore " + osname);
        startTransactionLoop(event.target.result.transaction(osname, "readonly"), osname, true);
    }
    request.onerror = function(event) {
        endTestWithLog("Unexpected error - " + osname + " - " + event);
    }
    request.onblocked = function(event) {
        endTestWithLog("Unexpected blocked - " + osname + " - " + event);
    }
    request.onupgradeneeded = function(event) {
        endTestWithLog("Unexpected upgradeneeded - " + osname + " - " + event);
    } 
}

var numberOfOpenTransactions = 0;

function startTransactionLoop(transaction, osname, isFirstTime)
{
    var objectStore = transaction.objectStore(osname);
    var request = objectStore.get("bar");
    
    request.onsuccess = function(event) {
        if (isFirstTime)
            numberOfOpenTransactions++;
        
        if (numberOfOpenTransactions == 2) {
            endTestWithLog("Two transactions open at once. Yay.");
        }
        startTransactionLoop(event.target.transaction, osname, false);
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


