description("This test makes sure that two read-only transactions to two different object stores are active at the same time.");

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
    var objectStore = database.createObjectStore("OS1");
    var request = objectStore.put("foo1", "bar1");

    request.onerror = function(event) {
        debug("put1 FAILED - " + event);
        done();
    }
    
    objectStore = database.createObjectStore("OS2");
    request = objectStore.put("foo2", "bar2");

    request.onerror = function(event) {
        debug("put2 FAILED - " + event);
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
        debug("Unexpected error - " + osname + " - " + event);
        done();
    }
    request.onblocked = function(event) {
        debug("Unexpected blocked - " + osname + " - " + event);
        done();
    }
    request.onupgradeneeded = function(event) {
        debug("Unexpected upgradeneeded - " + osname + " - " + event);
        done();
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
            debug("Two transactions open at once. Yay.");
            done();
        }
        startTransactionLoop(event.target.transaction, osname, false);
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


