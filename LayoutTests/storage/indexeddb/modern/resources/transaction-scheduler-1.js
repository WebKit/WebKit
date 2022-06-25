description("This test makes sure that two read-only transactions to an object store are active at the same time.");

indexedDBTest(prepareDatabase);


function done()
{
    finishJSTest();
}

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
        database.close();
    }

    versionTransaction.onerror = function(event) {
        debug("versionchange transaction error'ed - " + event);
        done();
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
        debug("Unexpected error - " + reqName + " - " + event);
        done();
    }
    request.onblocked = function(event) {
        debug("Unexpected blocked - " + reqName + " - " + event);
        done();
    }
    request.onupgradeneeded = function(event) {
        debug("Unexpected upgradeneeded - " + reqName + " - " + event);
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
            debug("Two transactions open at once. Yay.");
            done();
        }
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
        debug("Unexpected transaction complete - " + event);
        done();
    }
}


